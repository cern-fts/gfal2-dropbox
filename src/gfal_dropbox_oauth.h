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

// OAuth utility methods

#pragma once
#ifndef _GFAL_DROPBOX_OAUTH_H
#define _GFAL_DROPBOX_OAUTH_H

#include <gfal_api.h>

// Struct to hold the OAuth parameters
struct OAuth {
    char* app_key;
    char* access_token;
    char* app_secret;
    char* access_token_secret;
    char* timestamp; // For convenience, store serialized
    char* nonce;
};
typedef struct OAuth OAuth;

// Setup a OAuth struct from the gfal2_context
// It will setup nonce and timestamp as well
int oauth_setup(gfal2_context_t context, OAuth* oauth, GError** error);

// Releases the memory used by the internal fields of OAuth
// Note: It does NOT free Oauth
void oauth_release(OAuth* oauth);

// Builds the normalized parameters string used for the final OAuth base string
// See http://oauth.net/core/1.0/#signing_process
// url must be the full final Drobox URL
// n_args specify how many key/value pairs follow
// ... n_args pairs, in the order of key, value
int oauth_normalized_parameters(char* output, size_t outsize,
        const OAuth* oauth, size_t n_args, ...);

// Generates the base string
int oauth_get_basestring(const char* method, const char* url,
        const char* norm_params, char* output, size_t outsize);

// Generates the signature for the request
int oauth_get_signature(const char* method, const char* url, const char* norm_params,
        const OAuth* oauth,
        char* output, size_t outsize);

// Writes into buffer the OAuth HTTP Header
int oauth_get_header(char* buffer, size_t buffer_size, const OAuth* oauth,
        const char* method, const char* url, size_t n_args, va_list args);

#endif
