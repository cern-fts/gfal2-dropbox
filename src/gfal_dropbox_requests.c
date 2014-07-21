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



static ssize_t gfal2_dropbox_get_range_v(DropboxHandle* dropbox,
        const char* url, off_t offset, off_t size,
        char* output, size_t output_size, GError** error,
        size_t n_args, va_list args)
{
    OAuth oauth;

    if (oauth_setup(dropbox->gfal2_context, &oauth, error) < 0)
        return -1;

    char authorization_buffer[1024];
    va_list oauth_args;
    va_copy(oauth_args, args);
    oauth_get_header(authorization_buffer, sizeof(authorization_buffer), &oauth, "GET", url, n_args, oauth_args);
    va_end(oauth_args);

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

    // Where (need to concat the arguments)
    char url_buffer[GFAL_URL_MAX_LEN];
    gfal2_dropbox_concat_args(url, n_args, args, url_buffer, sizeof(url_buffer));
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url_buffer);

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


ssize_t gfal2_dropbox_get_range(DropboxHandle* dropbox,
        const char* url, off_t offset, off_t size,
        char* output, size_t output_size, GError** error,
        size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    ssize_t r = gfal2_dropbox_get_range_v(dropbox, url, offset, size, output, output_size, error, n_args, args);
    va_end(args);
    return r;
}


ssize_t gfal2_dropbox_get(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error,
        size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    ssize_t r = gfal2_dropbox_get_range_v(dropbox, url, 0, 0, output, output_size, error, n_args, args);
    va_end(args);
    return r;
}


static ssize_t gfal2_dropbox_post_v(DropboxHandle* dropbox,
        const char* url, const char* payload, const char* content_type,
        char* output, size_t output_size, GError** error,
        size_t n_args, va_list args)
{
    OAuth oauth;

    if (oauth_setup(dropbox->gfal2_context, &oauth, error) < 0)
        return -1;

    char authorization_buffer[1024];
    va_list oauth_args;
    va_copy(oauth_args, args);
    oauth_get_header(authorization_buffer, sizeof(authorization_buffer), &oauth, "POST", url, n_args, oauth_args);
    va_end(oauth_args);

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

    // Where (need to concat the arguments)
    char url_buffer[GFAL_URL_MAX_LEN];
    gfal2_dropbox_concat_args(url, n_args, args, url_buffer, sizeof(url_buffer));

    if (payload && content_type)
        headers = curl_slist_append(headers, content_type);

    curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url_buffer);
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


ssize_t gfal2_dropbox_post(DropboxHandle* dropbox,
        const char* url, const char* payload, const char* content_type,
        char* output, size_t output_size, GError** error,
        size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    ssize_t r = gfal2_dropbox_post_v(dropbox, url, payload, content_type, output, output_size, error, n_args, args);
    va_end(args);
    return r;
}


static ssize_t gfal2_dropbox_put_v(DropboxHandle* dropbox, const char* url,
        const char* payload, size_t payload_size, char* output, size_t output_size,
        GError** error, size_t n_args, va_list args)
{
    OAuth oauth;

    if (oauth_setup(dropbox->gfal2_context, &oauth, error) < 0)
        return -1;

    char authorization_buffer[1024];
    va_list oauth_args;
    va_copy(oauth_args, args);
    oauth_get_header(authorization_buffer, sizeof(authorization_buffer), &oauth, "PUT", url, n_args, oauth_args);
    va_end(oauth_args);

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

    // Where (need to concat the arguments)
    char url_buffer[GFAL_URL_MAX_LEN];
    gfal2_dropbox_concat_args(url, n_args, args, url_buffer, sizeof(url_buffer));

    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url_buffer);
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


ssize_t gfal2_dropbox_put(DropboxHandle* dropbox, const char* url,
        const char* payload, size_t payload_size, char* output, size_t output_size,
        GError** error, size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    ssize_t r = gfal2_dropbox_put_v(dropbox, url, payload, payload_size, output, output_size, error, n_args, args);
    va_end(args);
    return r;
}
