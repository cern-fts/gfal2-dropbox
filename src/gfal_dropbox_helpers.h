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

// Some handy methods to avoid writting the same boilerplate code in
// every function

#pragma once
#ifndef _GFAL_DROPBOX_HELPERS_H
#define _GFAL_DROPBOX_HELPERS_H

#include "gfal_dropbox.h"

// Extract the path from the URL
char* gfal2_dropbox_extract_path(const char* url, char* output, size_t output_size);

// Build the right Dropbox URL
int gfal2_dropbox_build_url(const char* api_base, const char* url, const char* args,
        char* output_url, size_t output_size, GError** error);

/// These methods take care of setting the OAuth headers!

// Do the request, put the output in output
ssize_t gfal2_dropbox_get(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error);

// Request with a range header
ssize_t gfal2_dropbox_get_range(DropboxHandle* dropbox,
        const char* url, off_t offset, off_t size,
        char* output, size_t output_size, GError** error);

// Post a JSON body
// Returns the response size
ssize_t gfal2_dropbox_post(DropboxHandle* dropbox,
        const char* url, const char* payload, const char* content_type,
        char* output, size_t output_size, GError** error);

// Do a PUT
// Returns the response size
ssize_t gfal2_dropbox_put(DropboxHandle* dropbox,
        const char* url,
        const char* buffer, size_t count,
        char* payload, size_t payload_size,
        GError** error);

#endif
