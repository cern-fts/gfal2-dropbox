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

/// These methods take care of setting the OAuth headers!

// Do the request, put the output in output
// n_args specifies how many arguments the URL will have, and then
// key and value must be passed separately gfal2_dropbox_get(..., 1, "list", "true)
ssize_t gfal2_dropbox_get(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error,
        size_t n_args, ...);

// Request with a range header
// n_args specifies how many arguments the URL will have, and then
// key and value must be passed separately gfal2_dropbox_get(..., 1, "list", "true)
ssize_t gfal2_dropbox_get_range(DropboxHandle* dropbox,
        const char* url, off_t offset, off_t size,
        char* output, size_t output_size, GError** error,
        size_t n_args, ...);

// Post a JSON body
// Returns the response size
ssize_t gfal2_dropbox_post(DropboxHandle* dropbox,
        const char* url, char* output, size_t output_size, GError** error,
        size_t n_args, ...);

// Do a PUT
// Returns the response size
ssize_t gfal2_dropbox_put(DropboxHandle* dropbox,
        const char* url,
        const char* buffer, size_t count,
        char* payload, size_t payload_size,
        GError** error,
        size_t n_args, ...);

#endif
