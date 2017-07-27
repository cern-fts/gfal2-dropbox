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

// Methods to perform requests to the Dropbox API

#pragma once
#ifndef _GFAL_DROPBOX_REQUESTS_H
#define _GFAL_DROPBOX_REQUESTS_H

#include "gfal_dropbox.h"

enum Method {
    M_GET,
    M_POST,
    M_PUT
};
typedef enum Method Method;

/// These methods take care of setting the OAuth headers!

// Perform the request method (GET, POST, PUT), building it with the provided headers,
// offset, etc.
ssize_t gfal2_dropbox_perform(DropboxHandle* dropbox,
    Method method, const char* url,
    off_t offset, off_t size,
    char* output, size_t output_size,
    const char *payload_mimetype,
    const char* payload, size_t payload_size,
    GError** error,
    size_t headers_count, ...);


// Post a JSON body
// Returns the response size
ssize_t gfal2_dropbox_post_json(DropboxHandle *dropbox,
    const char *url, char *output, size_t output_size, GError **error,
    size_t n_args, ...);


#endif
