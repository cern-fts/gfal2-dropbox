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

// Code to test the URL methods

#include "../gfal_dropbox_url.h"
#include <glib.h>
#include <stdio.h>


void test_extract_path()
{
    char buffer[1024];

    g_assert(NULL == gfal2_dropbox_extract_path("//something/path", buffer, sizeof(buffer)));
    g_assert(NULL == gfal2_dropbox_extract_path("dropbox://path", buffer, sizeof(buffer)));
    g_assert(NULL != gfal2_dropbox_extract_path("dropbox://dropbox.com/my/path", buffer, sizeof(buffer)));

    g_assert_cmpstr("/my/path", ==, buffer);

    printf("Extract path OK\n");
}


void test_build_url()
{
    GError *error = NULL;
    char buffer[1024];
    gfal2_dropbox_build_url("https://api.dropbox.com/base", "dropbox://dropbox.com/my/path",
            buffer, sizeof(buffer), &error);

    g_assert_cmpstr("https://api.dropbox.com/base/my/path", ==, buffer);

    printf("Build url path OK\n");
}


static int gfal2_dropbox_concat_args_wrap(const char* url, char* buffer, size_t bufsize, size_t n_args, ...)
{
    va_list args;
    va_start(args, n_args);
    int r = gfal2_dropbox_concat_args(url, n_args, args, buffer, bufsize);
    va_end(args);
    return r;
}


void test_concat_args()
{
    char buffer[1024];

    gfal2_dropbox_concat_args_wrap("https://api.dropbox.com/base/my/path", buffer, sizeof(buffer), 0);
    g_assert_cmpstr("https://api.dropbox.com/base/my/path", ==, buffer);

    gfal2_dropbox_concat_args_wrap("https://api.dropbox.com/base/my/path",
            buffer, sizeof(buffer), 1, "key", "value");
    g_assert_cmpstr("https://api.dropbox.com/base/my/path?key=value", ==, buffer);

    gfal2_dropbox_concat_args_wrap("https://api.dropbox.com/base/my/path",
            buffer, sizeof(buffer), 2, "key", "value", "something", "else");
    g_assert_cmpstr("https://api.dropbox.com/base/my/path?key=value&something=else", ==, buffer);

    printf("Concat args OK\n");
}


int main(int argc, char** argv)
{
    test_extract_path();
    test_build_url();
    test_concat_args();
    return 0;
}
