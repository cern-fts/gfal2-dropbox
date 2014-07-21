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

#pragma once
#ifndef _GFAL_DROPBOX_HELPERS_H
#define _GFAL_DROPBOX_HELPERS_H

#include <glib.h>

// Extract the path from the URL
char* gfal2_dropbox_extract_path(const char* url, char* output, size_t output_size);

// Put into output_url the URL resulting of concatenating the api call and the url
// Note that url will be passed to gfal2_dropbox_extract_path
int gfal2_dropbox_build_url(const char* api_base, const char* url,
        char* output_url, size_t output_size, GError** error);

// Concatenate the arguments to the URL
int gfal2_dropbox_concat_args(const char* url, size_t n_args, va_list args,
        char* url_buffer, size_t bufsize);

#endif
