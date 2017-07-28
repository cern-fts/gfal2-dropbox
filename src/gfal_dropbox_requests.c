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
#include <stdarg.h>
#include <string.h>
#include <json.h>


struct ErrorMapEntry {
    const char *tag;
    int errcode;
};

static const struct ErrorMapEntry ErrorMap[] = {
    {"not_found", ENOENT},
    {NULL, 0}
};


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

static void gfal2_dropbox_map_tag_to_errno(const char *tag, GError **error)
{
    int errcode = EIO;
    int i;
    for (i = 0; ErrorMap[i].tag != NULL; ++i) {
        if (g_strcmp0(ErrorMap[i].tag, tag) == 0) {
            errcode = ErrorMap[i].errcode;
            break;
        }
    }

    gfal2_set_error(error, dropbox_domain(), errcode, __func__, "%s", tag);
}

static void gfal2_dropbox_map_path_error(json_object *error_obj, GError **error)
{
    json_object *path_error = NULL;
    json_object* tag = NULL;
    if (json_object_object_get_ex(error_obj, "path", &path_error) &&
        json_object_object_get_ex(path_error, ".tag", &tag)) {
        const char *tag_str = json_object_get_string(tag);
        gfal2_dropbox_map_tag_to_errno(tag_str, error);
    }
    else {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__,
            "An path error happened, but failed to parse the reply");
    }
}


static void gfal2_dropbox_map_error(const char *output, size_t total_size, GError **error)
{
    json_object *response = json_tokener_parse(output);

    json_object *error_obj = NULL;
    json_object* tag = NULL;

    if (json_object_object_get_ex(response, "error", &error_obj) &&
        json_object_object_get_ex(error_obj, ".tag", &tag)) {
        const char *tag_str = json_object_get_string(tag);
        if (g_strcmp0(tag_str, "path") == 0) {
            gfal2_dropbox_map_path_error(error_obj, error);
        }
        else {
            gfal2_dropbox_map_tag_to_errno(tag_str, error);
        }
    }
    else {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__,
            "An error happened, and couldn't parse the response");
    }
    json_object_put(response);
}


static ssize_t gfal2_dropbox_perform_v(DropboxHandle* dropbox,
        Method method, const char* url,
        off_t offset, off_t size,
        char* output, size_t output_size,
        const char *payload_mimetype,
        const char* payload, size_t payload_size,
        size_t headers_count, va_list headers_args,
        GError** error)
{
    g_assert(dropbox != NULL && url != NULL && output != NULL && error != NULL);

    GError* tmp_err = NULL;
    OAuth oauth;
    struct curl_slist* headers = NULL;

    // OAuth
    if (oauth_setup(dropbox->gfal2_context, &oauth, &tmp_err) < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        goto fail;
    }

    char authorization_buffer[1024];
    int r = oauth_get_header(authorization_buffer, sizeof(authorization_buffer), &oauth, method_str(method), url);
    if (r < 0) {
        gfal2_set_error(error, dropbox_domain(), ENOBUFS, __func__, "Could not generate the OAuth header");
        goto fail;
    }

    headers = curl_slist_append(headers, authorization_buffer);

    // Payload type
    if (payload_mimetype) {
        char type_buffer[512];
        snprintf(type_buffer, sizeof(type_buffer), "Content-Type: %s", payload_mimetype);
        headers = curl_slist_append(headers, type_buffer);
    }

    // Additional headers
    size_t i;
    for (i = 0; i < headers_count; ++i) {
        char header_buffer[512];
        const char *key = va_arg(headers_args, const char*);
        const char *value = va_arg(headers_args, const char*);
        snprintf(header_buffer, sizeof(header_buffer), "%s: %s", key, value);
        headers = curl_slist_append(headers, header_buffer);
    }

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
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_POST, 1);
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_POSTFIELDSIZE, payload_size);
            break;
        case M_GET:
            curl_easy_setopt(dropbox->curl_handle, CURLOPT_UPLOAD, 0);
            break;
    }
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_URL, url);

    // Payload
    FILE* payload_fd = NULL;
    if (payload) {
        payload_fd = fmemopen((char*)payload, payload_size, "rb");
        curl_easy_setopt(dropbox->curl_handle, CURLOPT_READFUNCTION, fread);
        curl_easy_setopt(dropbox->curl_handle, CURLOPT_READDATA, payload_fd);
    }

    // Do!
    gfal2_log(G_LOG_LEVEL_INFO, "%s %s", method_str(method), url);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_HTTPHEADER, headers);
    int perform_result = curl_easy_perform(dropbox->curl_handle);

    // Release resources
    fclose(fd);
    if (payload_fd)
        fclose(payload_fd);
    curl_slist_free_all(headers);

    if (perform_result != 0) {
        gfal2_set_error(error, dropbox_domain(), EIO, __func__, "%s", err_buffer);
        goto fail;
    }

    double total_size;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_SIZE_DOWNLOAD, &total_size);
    oauth_release(&oauth);

    long response;
    curl_easy_getinfo(dropbox->curl_handle, CURLINFO_RESPONSE_CODE, &response);
    if (response / 100 != 2) {
        switch (response) {
            case 400:
                gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Dropbox plugin made an invalid request");
                return -1;
            case 401:
                gfal2_set_error(error, dropbox_domain(), EACCES, __func__, "Token invalid, expired or revoked");
                return -1;
            case 409:
                gfal2_dropbox_map_error(output, total_size, error);
                return -1;
            case 429:
                gfal2_set_error(error, dropbox_domain(), EBUSY, __func__, "Too many request or write operations");
                return -1;
            default:
                gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Dropbox internal error");
                return -1;
        }
    }

    return (ssize_t)(total_size);

fail:
    oauth_release(&oauth);
    return -1;
}


ssize_t gfal2_dropbox_perform(DropboxHandle* dropbox,
    Method method, const char* url,
    off_t offset, off_t size,
    char* output, size_t output_size,
    const char *payload_mimetype,
    const char* payload, size_t payload_size,
    GError** error,
    size_t headers_count, ...)
{
    va_list args;
    va_start(args, headers_count);
    ssize_t ret = gfal2_dropbox_perform_v(dropbox, method, url,
        offset, size, output, output_size,
        payload_mimetype, payload, payload_size,
        headers_count, args,
        error);
    va_end(args);
    return ret;
}


ssize_t gfal2_dropbox_post_json(DropboxHandle *dropbox,
    const char *url, char *output, size_t output_size, GError **error,
    size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    size_t i;
    json_object *request = json_object_new_object();
    for (i = 0; i < n_args; ++i) {
        const char *key = va_arg(args, const char*);
        const char *value = va_arg(args, const char*);
        json_object *value_obj = json_object_new_string(value);
        json_object_object_add(request, key, value_obj);
    }
    va_end(args);

    const char *payload = json_object_to_json_string(request);

    GError* tmp_err = NULL;
    ssize_t r = gfal2_dropbox_perform_v(dropbox,
        M_POST, url,
        0, 0,
        output, output_size,
        "application/json", payload, strlen(payload),
        0, NULL,
        &tmp_err);
    json_object_put(request);
    if (r < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
    }
    return r;
}
