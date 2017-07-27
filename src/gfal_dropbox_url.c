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
#include <ctype.h>
#include <string.h>


#define g_stpncpy(dst, src, max) (g_strlcpy(dst, src, max) + dst)


char* gfal2_dropbox_extract_path(const char* url, char* output, size_t output_size)
{
    g_assert(url != NULL && output != NULL);

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
        return g_stpncpy(output, "/", output_size);
    return g_stpncpy(output, p, output_size);
}


int gfal2_dropbox_build_url(const char* api_base, const char* url,
        char* output_url, size_t output_size, GError** error)
{
    g_assert(api_base != NULL && url != NULL && output_url != NULL && error != NULL);

    char* end = g_stpncpy(output_url, api_base, output_size);
    size_t api_base_len = (end - output_url);
    end = gfal2_dropbox_extract_path(url, end, output_size - api_base_len);
    if (end == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }
    return 0;
}


int gfal2_dropbox_normalize_url(const char* url, char* out, size_t outsize)
{
    enum {
        NORM_SCHEME,
        NORM_HOST,
        NORM_PATH,
        NORM_ESCAPED
    } stage = NORM_SCHEME;

    size_t url_len = strlen(url);
    if (url_len > outsize)
        return -1;

    size_t i = 0;
    const char* p = url;

    while (i < outsize && *p != '\0') {
        switch (stage) {
            case NORM_SCHEME:
                out[i++] = tolower(*p);
                if (*p == ':') {
                    out[i++] = '/';
                    out[i++] = '/';
                    ++p;
                    while(*p == '/') ++p;
                    stage = NORM_HOST;
                    continue;
                }
                break;
            case NORM_HOST:
                out[i++] = tolower(*p);
                if (*p == '/') {
                    stage = NORM_PATH;
                    --i;
                    continue;
                }
                break;
            case NORM_PATH:
                out[i++] = *p;
                if (*p == '/') {
                    while (*p == '/') ++p;
                    continue;
                }
                if (*p == '%') {
                    stage = NORM_ESCAPED;
                }
                break;
            case NORM_ESCAPED:
                out[i++] = toupper(*(p++));
                out[i++] = toupper(*(p++));
                stage = NORM_PATH;
                continue;
        }
        ++p;
    }
    out[i] = '\0';
    return 0;
}


time_t gfal2_dropbox_time(const char* stime)
{
    struct tm tim;
    strptime(stime, "%Y-%m-%dT%H:%M:%S%Z", &tim);
    return mktime(&tim);
}