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

#include "gfal_dropbox_helpers.h"
#include <common/gfal_common_err_helpers.h>
#include <string.h>

// Generate OAuth Authorization header
static int gfal2_dropbox_get_oauth_header(gfal2_context_t gfal2_context, char* buffer, size_t buffer_size, GError** error)
{
    const char* app_key = gfal2_get_opt_string(gfal2_context, "OAUTH", "APP_KEY", NULL);
    const char* access_token = gfal2_get_opt_string(gfal2_context, "OAUTH", "ACCESS_TOKEN", NULL);
    const char* app_secret = gfal2_get_opt_string(gfal2_context, "OAUTH", "APP_SECRET", NULL);
    const char* access_token_secret = gfal2_get_opt_string(gfal2_context, "OAUTH", "ACCESS_TOKEN_SECRET", NULL);

    if (!app_key || !access_token || !app_secret || !access_token_secret) {
        gfal2_set_error(
                error, dropbox_domain(), EINVAL, __func__,
                "Missing OAuth values. Make sure you pass APP_KEY, APP_SECRET, ACCESS_TOKEN and ACCESS_TOKEN_SECRET inside the group OAUTH"
        );
        return -1;
    }

    snprintf(buffer, buffer_size,
            "Authorization: OAuth oauth_version=\"1.0\", oauth_signature_method=\"PLAINTEXT\", oauth_consumer_key=\"%s\", oauth_token=\"%s\", oauth_signature=\"%s&%s\"",
             app_key, access_token, app_secret, access_token_secret
    );

    return 0;
}


char* gfal2_dropbox_extract_path(const char* url, char* output, size_t output_size)
{
    char* p = strchr(url, ':');
    if (!p)
        return NULL;
    // Jump over the //
    ++p;
    while (*p != '\0' && *p == '/')
        ++p;
    if (*p == '\0')
        return NULL;
    // We are now pointing to the host, so jump to the first /
    p = strchr(p, '/');
    if (!p)
        return NULL;
    return stpncpy(output, p, output_size);
}


int gfal2_dropbox_build_url(const char* api_base, const char* url, const char* args,
        char* output_url, size_t output_size, GError** error)
{
    char* end = stpncpy(output_url, api_base, output_size);
    size_t api_base_len = (end - output_url);
    end = gfal2_dropbox_extract_path(url, end, output_size - api_base_len);
    if (end == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }
    size_t url_len = (end - output_url);
    snprintf(end, output_size - url_len, "?%s", args?args:"");
    return 0;
}


static int gfal2_dropbox_map_http_status(long response, GError** error, const char* func)
{
    if (response < 400)
        return 0;
    int errval = 0;
    if (response == 400)
        errval = EINVAL;
    else if (response >= 401 && response <= 403)
        errval = EACCES;
    else if (response == 404)
        errval = ENOENT;
    else
        errval = EIO;

    gfal2_set_error(error, dropbox_domain(), errval, func, "HTTP Response %ld", response);
    return -1;
}


ssize_t gfal2_dropbox_get_range(DropboxHandle* dropbox,
        const char* url, off_t offset, off_t size,
        char* output, size_t output_size, GError** error)
{
    char authorization_buffer[1024];
    if (gfal2_dropbox_get_oauth_header(dropbox->gfal2_context, authorization_buffer, sizeof(authorization_buffer), error) < 0)
        return -1;

    // OAuth header
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, authorization_buffer);

    // Follow redirection
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_FOLLOWLOCATION, 1);

    // Range, if needed
    if (offset || size) {
        char range_buffer[512];
        snprintf(range_buffer, sizeof(range_buffer), "Range: bytes=%ld-%ld", offset, offset + size - 1);
        headers = curl_slist_append(headers, range_buffer);
    }

    // Where to write
    // Note, the b is important, or the last byte will be lost!
    FILE* fd = fmemopen(output, output_size, "wb");
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_WRITEDATA, fd);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_WRITEFUNCTION, fwrite);

    // Error buffer
    char err_buffer[CURL_ERROR_SIZE];
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_ERRORBUFFER, err_buffer);

    // Where
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url);

    // Do!
    gfal_log(GFAL_VERBOSE_VERBOSE, "GET %s", url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_HTTPHEADER, headers);
    int perform_result = curl_easy_perform(dropbox->curl_handle);

    // Release resources
    fclose(fd);
    curl_slist_free_all(headers);

    if (perform_result != 0) {
        gfal2_set_error(error, dropbox_domain(), EIO, __func__, "%s", err_buffer);
        return -1;
    }

    long response;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_RESPONSE_CODE, &response);
    if (gfal2_dropbox_map_http_status(response, error, __func__) < 0)
        return -1;

    double total_size;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_SIZE_DOWNLOAD, &total_size);
    return (ssize_t)(total_size);
}


ssize_t gfal2_dropbox_get(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error)
{
    return gfal2_dropbox_get_range(dropbox, url, 0, 0, output, output_size, error);
}


ssize_t gfal2_dropbox_post(DropboxHandle* dropbox,
        const char* url, const char* payload, const char* content_type,
        char* output, size_t output_size, GError** error)
{
    char authorization_buffer[1024];
    if (gfal2_dropbox_get_oauth_header(dropbox->gfal2_context, authorization_buffer, sizeof(authorization_buffer), error) < 0)
        return -1;

    // OAuth header
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, authorization_buffer);

    // Follow redirection
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_FOLLOWLOCATION, 1);

    // Where to write
    // Note, the b is important, or the last byte will be lost!
    FILE* fd = fmemopen(output, output_size, "wb");
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_WRITEDATA, fd);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_WRITEFUNCTION, fwrite);

    // Error buffer
    char err_buffer[CURL_ERROR_SIZE];
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_ERRORBUFFER, err_buffer);

    // Where and what
    if (payload && content_type)
        headers = curl_slist_append(headers, content_type);

    curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_POST, 1);
    if (payload)
        curl_easy_setopt(dropbox->curl_handle, CURLOPT_POSTFIELDS, payload);
    else
        curl_easy_setopt(dropbox->curl_handle, CURLOPT_POSTFIELDSIZE, 0);

    // Do!
    gfal_log(GFAL_VERBOSE_VERBOSE, "POST %s", url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_HTTPHEADER, headers);
    int perform_result = curl_easy_perform(dropbox->curl_handle);

    // Release resources
    fclose(fd);
    curl_slist_free_all(headers);

    if (perform_result != 0) {
        gfal2_set_error(error, dropbox_domain(), EIO, __func__, "%s", err_buffer);
        return -1;
    }

    long response;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_RESPONSE_CODE, &response);
    if (gfal2_dropbox_map_http_status(response, error, __func__) < 0)
        return -1;

    double total_size;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_SIZE_DOWNLOAD, &total_size);
    return (ssize_t)(total_size);
}


ssize_t gfal2_dropbox_put(DropboxHandle* dropbox, const char* url,
        const char* payload, size_t payload_size, char* output, size_t output_size,
        GError** error)
{
    char authorization_buffer[1024];
    if (gfal2_dropbox_get_oauth_header(dropbox->gfal2_context, authorization_buffer, sizeof(authorization_buffer), error) < 0)
        return -1;

    // OAuth header
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, authorization_buffer);

    // Follow redirection
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_FOLLOWLOCATION, 1);

    // Where to write
    // Note, the b is important, or the last byte will be lost!
    FILE* fd = fmemopen(output, output_size, "wb");
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_WRITEDATA, fd);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_WRITEFUNCTION, fwrite);

    // Error buffer
    char err_buffer[CURL_ERROR_SIZE];
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_ERRORBUFFER, err_buffer);

    // Where and what
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 1);

    FILE* fd_payload = fmemopen((char*)payload, payload_size, "rb");
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_READDATA, fd_payload);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_INFILESIZE, payload_size);

    // Do!
    gfal_log(GFAL_VERBOSE_VERBOSE, "PUT %s", url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_HTTPHEADER, headers);
    int perform_result = curl_easy_perform(dropbox->curl_handle);

    // Release resources
    fclose(fd);
    fclose(fd_payload);
    curl_slist_free_all(headers);

    if (perform_result != 0) {
        gfal2_set_error(error, dropbox_domain(), EIO, __func__, "%s", err_buffer);
        return -1;
    }

    long response;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_RESPONSE_CODE, &response);
    if (gfal2_dropbox_map_http_status(response, error, __func__) < 0)
        return -1;

    double total_size;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_SIZE_DOWNLOAD, &total_size);
    return (ssize_t)(total_size);
}
