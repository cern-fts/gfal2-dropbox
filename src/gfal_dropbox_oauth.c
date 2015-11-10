/*
 *  Copyright 2014 CERN
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
**/

#include "gfal_dropbox.h"
#include "gfal_dropbox_oauth.h"
#include "gfal_dropbox_url.h"
#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/hmac.h>
#include <string.h>
#include <common/gfal_common_err_helpers.h>


struct KeyValue {
    const char *key, *value;
};
typedef struct KeyValue KeyValue;


int oauth_setup(gfal2_context_t context, OAuth* oauth, GError** error)
{
    g_assert(context != NULL && oauth != NULL && error != NULL);

    oauth->version = gfal2_get_opt_integer_with_default(context, "DROPBOX", "OAUTH", 1);
    oauth->app_key = gfal2_get_opt_string(context, "DROPBOX", "APP_KEY", NULL);
    oauth->access_token = gfal2_get_opt_string(context, "DROPBOX", "ACCESS_TOKEN", NULL);
    oauth->app_secret = gfal2_get_opt_string(context, "DROPBOX", "APP_SECRET", NULL);
    oauth->access_token_secret = gfal2_get_opt_string(context, "DROPBOX", "ACCESS_TOKEN_SECRET", NULL);

    switch (oauth->version) {
        case 1:
            if (!oauth->app_key || !oauth->access_token || !oauth->app_secret || !oauth->access_token_secret) {
                gfal2_set_error(error, dropbox_domain(), EINVAL, __func__,
                                "Missing OAuth values. Make sure you pass APP_KEY, APP_SECRET, ACCESS_TOKEN and ACCESS_TOKEN_SECRET inside the group DROPBOX");
                oauth_release(oauth);
                return -1;
            }
            break;
        case 2:
            if (!oauth->app_key || !oauth->access_token || !oauth->app_secret) {
                gfal2_set_error(error, dropbox_domain(), EINVAL, __func__,
                                "Missing OAuth values. Make sure you pass APP_KEY, APP_SECRET and ACCESS_TOKEN inside "
                                "the group DROPBOX");
                oauth_release(oauth);
                return -1;
            }
            break;
        default:
            gfal2_set_error(error, dropbox_domain(), EINVAL, __func__,
                "Invalid OAuth version (%d)", oauth->version);
            oauth_release(oauth);
            return -1;
    }

    char aux_buffer[512];
    snprintf(aux_buffer, sizeof(aux_buffer), "%ld", time(NULL));
    oauth->timestamp = g_strdup(aux_buffer);
    snprintf(aux_buffer, sizeof(aux_buffer), "%s*%ld", oauth->timestamp, random());
    oauth->nonce = g_strdup(aux_buffer);

    return 0;
}


void oauth_release(OAuth* oauth)
{
    g_assert(oauth != NULL);

    g_free(oauth->app_key);
    g_free(oauth->access_token);
    g_free(oauth->app_secret);
    g_free(oauth->access_token_secret);
    g_free(oauth->timestamp);
    g_free(oauth->nonce);

    memset(oauth, 0, sizeof(OAuth));
}


static size_t oauth_populate_keyvalue_from_args(KeyValue* pairs, size_t start,
        size_t n_args, va_list args)
{
    g_assert(pairs != NULL);

    size_t n, i;
    for (n = 0, i = start; n < n_args; ++n, ++i) {
        pairs[i].key = va_arg(args, const char*);
        pairs[i].value = va_arg(args, const char*);
    }
    return i;
}


static size_t oauth_populate_keyvalue_from_oauth(KeyValue* pairs,
        const OAuth* oauth, size_t start)
{
    g_assert(pairs != NULL && oauth != NULL);

    size_t i = start;
    pairs[i].key = "oauth_version";
    pairs[i].value = "1.0";
    ++i;
    pairs[i].key = "oauth_token";
    pairs[i].value = oauth->access_token;
    ++i;
    pairs[i].key = "oauth_signature_method";
    pairs[i].value = "HMAC-SHA1";
    ++i;
    pairs[i].key = "oauth_consumer_key";
    pairs[i].value = oauth->app_key;
    ++i;
    pairs[i].key = "oauth_nonce";
    pairs[i].value = oauth->nonce;
    ++i;
    pairs[i].key = "oauth_timestamp";
    pairs[i].value = oauth->timestamp;
    return i + 1;
}


static int oauth_compare_key(const void* a_ptr, const void* b_ptr)
{
    g_assert(a_ptr != NULL && b_ptr != NULL);

    KeyValue *a, *b;
    a = (KeyValue*)a_ptr;
    b = (KeyValue*)b_ptr;
    return strcmp(a->key, b->key);
}


static int oauth_normalized_parameters_v(char* output, size_t outsize,
        const OAuth* oauth, size_t n_args, va_list args)
{
    g_assert(output != NULL && oauth != NULL);

    // Account for oauth* headers
    size_t n_parameters = n_args + 6;
    KeyValue *pairs = g_malloc0(n_parameters * sizeof(KeyValue));

    size_t next = oauth_populate_keyvalue_from_args(pairs, 0, n_args, args);
    next = oauth_populate_keyvalue_from_oauth(pairs, oauth, next);

    // Sort
    qsort(pairs, n_parameters, sizeof(KeyValue), oauth_compare_key);

    // Concatenate using & and escaping
    size_t i;
    char *p = output;
    size_t remaining = outsize;
    for (i = 0; i < n_parameters; ++i) {
        char* escaped_key = curl_easy_escape(NULL, pairs[i].key, 0);
        char* escaped_value = curl_easy_escape(NULL, pairs[i].value, 0);

        size_t printed = snprintf(p, remaining, "%s=%s&", escaped_key, escaped_value);
        remaining -= printed;
        p += printed;

        curl_free(escaped_key);
        curl_free(escaped_value);
    }

    // Truncate last &
    --p;
    if (*p == '&')
        *p = '\0';

    g_free(pairs);
    return 0;
}


int oauth_normalized_parameters(char* output, size_t outsize,
        const OAuth* oauth, size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    int r = oauth_normalized_parameters_v(output, outsize, oauth, n_args, args);
    va_end(args);
    return r;
}


static unsigned base64_encode(const char* input, unsigned length, char* output, size_t outsize)
{
    g_assert(input != NULL && output != NULL);

    BIO *bmem, *b64;
    BUF_MEM *bptr;
    unsigned written;

    b64  = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64  = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_ctrl(b64, BIO_CTRL_FLUSH, 0, NULL);
    BIO_get_mem_ptr(b64, &bptr);

    written = bptr->length - 1;
    memcpy(output, bptr->data, written>outsize?outsize:written);
    output[written] = 0;

    BIO_free_all(b64);

    return written;
}


int oauth_get_basestring(const char* method, const char* url, const char* norm_params, char* output, size_t outsize)
{
    g_assert(method != NULL && url != NULL && norm_params != NULL && output != NULL);

    char normalized_url[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_normalize_url(url, normalized_url, sizeof(normalized_url)) < 0)
        return -1;

    gfal2_log(G_LOG_LEVEL_INFO, "%s", normalized_url);

    char* escaped_url = curl_easy_escape(NULL, normalized_url, 0);
    char* escaped_params = curl_easy_escape(NULL, norm_params, 0);

    int basestring_len = snprintf(output, outsize, "%s&%s&%s", method, escaped_url, escaped_params);

    curl_free(escaped_url);
    curl_free(escaped_params);
    return basestring_len;
}


int oauth_get_signature(const char* method, const char* url, const char* norm_params,
        const OAuth* oauth,
        char* output, size_t outsize)
{
    g_assert(method != NULL && url != NULL && norm_params != NULL && oauth != NULL && output != NULL);

    char payload[2048];
    char key_buffer[512];

    char *escaped_app_secret = curl_easy_escape(NULL, oauth->app_secret, 0);
    char *escaped_token_secret = curl_easy_escape(NULL, oauth->access_token_secret, 0);
    unsigned key_size = snprintf(key_buffer, sizeof(key_buffer), "%s&%s", escaped_app_secret, escaped_token_secret);

    int basestring_len = oauth_get_basestring(method, url, norm_params, payload, sizeof(payload));
    if (basestring_len < 0)
        return -1;

    curl_free(escaped_app_secret);
    curl_free(escaped_token_secret);

    gfal2_log(G_LOG_LEVEL_DEBUG, "Signing %s", payload);

    char buffer[2048];
    unsigned outl = 0;
    HMAC(EVP_sha1(), key_buffer, key_size,
            (const unsigned char*)payload, basestring_len,
            (unsigned char*)buffer, &outl);

    base64_encode(buffer, outl, output, outsize);

    return 0;
}


static int oauth1_get_header(char* buffer, size_t buffer_size, const OAuth* oauth,
        const char* method, const char* url, size_t n_args, va_list args)
{
    g_assert(buffer != NULL && oauth != NULL && method != NULL && url != NULL);

    char normalized[1024];
    char signature[1024];

    oauth_normalized_parameters_v(normalized, sizeof(normalized), oauth, n_args, args);
    if (oauth_get_signature(method, url, normalized, oauth, signature, sizeof(signature)) < 0)
        return -1;

    return snprintf(buffer, buffer_size,
                    "Authorization: OAuth oauth_version=\"1.0\", oauth_signature_method=\"HMAC-SHA1\", "
                            "oauth_nonce=\"%s\", oauth_timestamp=\"%s\", "
                            "oauth_consumer_key=\"%s\", oauth_token=\"%s\", oauth_signature=\"%s\"",
                    oauth->nonce, oauth->timestamp, oauth->app_key, oauth->access_token, signature
    );
}


static int oauth2_get_header(char* buffer, size_t buffer_size, const OAuth* oauth,
    const char* method, const char* url, size_t n_args, va_list args)
{
    g_assert(buffer != NULL && oauth != NULL && method != NULL && url != NULL);

    return snprintf(buffer, buffer_size, "Authorization: Bearer %s", oauth->access_token);
}


int oauth_get_header(char* buffer, size_t buffer_size, const OAuth* oauth,
        const char* method, const char* url, size_t n_args, va_list args)
{
    g_assert(oauth != NULL);
    g_assert(oauth->version == 1 || oauth->version == 2);

    if (oauth->version == 1)
        return oauth1_get_header(buffer, buffer_size, oauth, method, url, n_args, args);
    else
        return oauth2_get_header(buffer, buffer_size, oauth, method, url, n_args, args);
}
