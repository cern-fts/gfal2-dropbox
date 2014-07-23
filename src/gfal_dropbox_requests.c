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

#include "gfal_dropbox_requests.h"
#include "gfal_dropbox_url.h"
#include "gfal_dropbox_oauth.h"
#include <common/gfal_common_err_helpers.h>
#include <stdarg.h>
#include <string.h>

enum Method {
    M_GET,
    M_POST,
    M_PUT
};
typedef enum Method Method;


static const char* method_str(Method m)
{
    switch (m) {
        case M_GET:
            return "GET";
        case M_POST:
            return "POST";
        case M_PUT:
            return "PUT";
    }
    return "";
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


static ssize_t gfal2_dropbox_perform_v(DropboxHandle* dropbox,
        Method method, const char* url,
        off_t offset, off_t size,
        char* output, size_t output_size,
        const char* payload, size_t payload_size,
        GError** error,
        size_t n_args, va_list args)
{
    g_assert(dropbox != NULL && url != NULL && output != NULL && error != NULL);

    GError* tmp_err = NULL;
    OAuth oauth;
    struct curl_slist* headers = NULL;

    // OAuth
    if (oauth_setup(dropbox->gfal2_context, &oauth, &tmp_err) < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }

    char authorization_buffer[1024];
    va_list oauth_args;
    va_copy(oauth_args, args);
    int r = oauth_get_header(authorization_buffer, sizeof(authorization_buffer), &oauth, method_str(method), url, n_args, oauth_args);
    va_end(oauth_args);
    if (r < 0) {
        gfal2_set_error(error, dropbox_domain(), ENOBUFS, __func__, "Could not generate the OAuth header");
        return -1;
    }

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

    // What and where (need to concat the arguments)
    switch (method) {
        case M_PUT:
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 1);
            break;
        case M_POST:
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_POST, 1);
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_POSTFIELDSIZE, 0);
            break;
        case M_GET:
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
            break;
    }
    char url_buffer[GFAL_URL_MAX_LEN];
    gfal2_dropbox_concat_args(url, n_args, args, url_buffer, sizeof(url_buffer));
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url_buffer);

    // Payload
    FILE* payload_fd = NULL;
    if (payload) {
        payload_fd = fmemopen((char*)payload, payload_size, "rb");
        curl_easy_setopt(dropbox->curl_handle, CURLOPT_READFUNCTION, fread);
        curl_easy_setopt(dropbox->curl_handle, CURLOPT_READDATA, payload_fd);
    }

    // Do!
    gfal_log(GFAL_VERBOSE_VERBOSE, "%s %s", method_str(method), url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_HTTPHEADER, headers);
    int perform_result = curl_easy_perform(dropbox->curl_handle);

    // Release resources
    fclose(fd);
    if (payload_fd)
        fclose(payload_fd);
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


ssize_t gfal2_dropbox_get_range(DropboxHandle* dropbox,
        const char* url, off_t offset, off_t size,
        char* output, size_t output_size, GError** error,
        size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    GError* tmp_err = NULL;
    ssize_t r = gfal2_dropbox_perform_v(dropbox, M_GET, url, offset, size,
            output, output_size, NULL, 0, &tmp_err, n_args, args);
    if (r < 0)
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
    va_end(args);
    return r;
}


ssize_t gfal2_dropbox_get(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error,
        size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    GError* tmp_err = NULL;
    ssize_t r = gfal2_dropbox_perform_v(dropbox, M_GET, url, 0, 0,
            output, output_size, NULL, 0, &tmp_err, n_args, args);
    if (r < 0)
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
    va_end(args);
    return r;
}


ssize_t gfal2_dropbox_post(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error,
        size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    GError* tmp_err = NULL;
    ssize_t r = gfal2_dropbox_perform_v(dropbox, M_POST, url, 0, 0, output,
            output_size, NULL, 0, &tmp_err, n_args, args);
    if (r < 0)
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);;
    va_end(args);
    return r;
}


ssize_t gfal2_dropbox_put(DropboxHandle* dropbox, const char* url,
        const char* payload, size_t payload_size, char* output, size_t output_size,
        GError** error, size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    GError* tmp_err = NULL;
    ssize_t r = gfal2_dropbox_perform_v(dropbox, M_PUT, url, 0, 0, output, output_size, payload, payload_size, &tmp_err, n_args, args);
    if (r < 0)
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
    va_end(args);
    return r;
}
