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

// Url methods

#include "gfal_dropbox.h"
#include "gfal_dropbox_url.h"
#include <common/gfal_common_err_helpers.h>
#include <string.h>


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


int gfal2_dropbox_build_url(const char* api_base, const char* url,
        char* output_url, size_t output_size, GError** error)
{
    char* end = stpncpy(output_url, api_base, output_size);
    size_t api_base_len = (end - output_url);
    end = gfal2_dropbox_extract_path(url, end, output_size - api_base_len);
    if (end == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }
    return 0;
}


int gfal2_dropbox_concat_args(const char* url, size_t n_args, va_list args,
        char* url_buffer, size_t bufsize)
{
    if (n_args == 0) {
        strncpy(url_buffer, url, bufsize);
        return 0;
    }

    char* end = url_buffer + bufsize;
    char* p = stpncpy(url_buffer, url, bufsize);
    bufsize = end - p;
    p = stpncpy(p, "?", bufsize);

    size_t i;
    for (i = 0; i < n_args; ++i) {
        char* key = curl_easy_escape(NULL, va_arg(args, const char*), 0);
        char* value = curl_easy_escape(NULL, va_arg(args, const char*), 0);
        p = stpncpy(p, key, bufsize);
        bufsize = end - p;
        p = stpncpy(p, "=", bufsize);
        bufsize = end - p;
        p = stpncpy(p, value, bufsize);
        bufsize = end - p;
        p = stpncpy(p, "&", bufsize);
        curl_free(key);
        curl_free(value);
    }

    // Truncate last &
    --p;
    if (*p == '&')
        *p = '\0';

    return 0;
}
