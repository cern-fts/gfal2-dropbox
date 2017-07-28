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
#include <string.h>

#include "test_help.h"


void test_extract_path()
{
    char buffer[1024];

    g_assert(NULL == gfal2_dropbox_extract_path("//something/path", buffer, sizeof(buffer)));
    g_assert(NULL == gfal2_dropbox_extract_path("dropbox://path", buffer, sizeof(buffer)));
    g_assert(NULL != gfal2_dropbox_extract_path("dropbox://dropbox.com/my/path", buffer, sizeof(buffer)));

    ASSERT_STR_EQ("/my/path", buffer);

    printf("Extract path OK\n");
}


void test_build_url()
{
    GError *error = NULL;
    char buffer[1024];
    gfal2_dropbox_build_url("https://api.dropbox.com/base", "dropbox://dropbox.com/my/path",
            buffer, sizeof(buffer), &error);

    ASSERT_STR_EQ("https://api.dropbox.com/base/my/path", buffer);

    printf("Build url path OK\n");
}


void test_normalize_url()
{
    char buffer[1024];

    gfal2_dropbox_normalize_url("dROPbox://MyHost.com//path///file%a5/SOM///thing", buffer, sizeof(buffer));

    ASSERT_STR_EQ("dropbox://myhost.com/path/file%A5/SOM/thing", buffer);

    g_assert(0 > gfal2_dropbox_normalize_url("dROPbox://MyHost.com//path///file%a5/SOM///thing", buffer, 5));
}


int main(int argc, char** argv)
{
    test_extract_path();
    test_build_url();
    test_normalize_url();
    return 0;
}
