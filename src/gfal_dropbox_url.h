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
// Returns a pointer to the NULL termination character
char* gfal2_dropbox_extract_path(const char* url, char* output, size_t output_size);

// Normalize URL
int gfal2_dropbox_normalize_url(const char* url, char* out, size_t outsize);

// Convert a string with a timestamp to a time_t
time_t gfal2_dropbox_time(const char* stime);

#endif
