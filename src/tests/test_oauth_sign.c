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

// Code to test the signature methods

#include <glib.h>
#include <stdio.h>
#include "../gfal_dropbox_oauth.h"


// From http://oauth.net/core/1.0/#sig_base_example
void test_oauth_example()
{
    OAuth oauth;
    oauth.access_token = "nnch734d00sl2jdk";
    oauth.access_token_secret = "pfkkdhi9sl3r4s00";
    oauth.app_key = "dpf43f3p2l4k3l03";
    oauth.app_secret = "kd94hf93k423kf44";
    oauth.nonce = "kllo9940pd9333jh";
    oauth.timestamp = "1191242096";

    char params_buffer[1024] = {0};
    oauth_normalized_parameters(params_buffer, sizeof(params_buffer), &oauth, 2,
            "file", "vacation.jpg", "size", "original");

    g_assert(0 == strcmp(
            "file=vacation.jpg&oauth_consumer_key=dpf43f3p2l4k3l03&oauth_nonce=kllo9940pd9333jh"
            "&oauth_signature_method=HMAC-SHA1&oauth_timestamp=1191242096&oauth_token=nnch734d00sl2jdk"
            "&oauth_version=1.0&size=original", params_buffer));

    printf("OAuth Normalized parameters OK\n");

    char signature_buffer[1024] = {0};
    oauth_get_signature("GET", "http://photos.example.net/photos", params_buffer, &oauth, signature_buffer, sizeof(signature_buffer));

    g_assert(0 == strcmp("tR3+Ty81lMeYAr/Fid0kMTYa/WM=", signature_buffer));

    printf("OAuth Signature OK\n");
}

// From https://dev.twitter.com/docs/auth/creating-signature
void test_twitter_example()
{
    OAuth oauth;
    oauth.access_token = "370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb";
    oauth.access_token_secret = "LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE";
    oauth.app_key = "xvz1evFS4wEEPTGEFPHBog";
    oauth.app_secret = "kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw";
    oauth.timestamp = "1318622958";
    oauth.nonce = "kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg";

    char params_buffer[1024] = {0};
    oauth_normalized_parameters(params_buffer, sizeof(params_buffer), &oauth, 2,
               "status", "Hello Ladies + Gentlemen, a signed OAuth request!",
               "include_entities", "true");

    g_assert(0 == strcmp(
            "include_entities=true&oauth_consumer_key=xvz1evFS4wEEPTGEFPHBog&oauth_nonce=kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg&"
            "oauth_signature_method=HMAC-SHA1&oauth_timestamp=1318622958&oauth_token=370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb&"
            "oauth_version=1.0&status=Hello%20Ladies%20%2B%20Gentlemen%2C%20a%20signed%20OAuth%20request%21", params_buffer));

    printf("Twitter example normalized OK\n");

    char basestring_buffer[1024];
    oauth_get_basestring("POST", "https://api.twitter.com/1/statuses/update.json",
            params_buffer, basestring_buffer, sizeof(basestring_buffer));

    g_assert(0 == strcmp(
            "POST&https%3A%2F%2Fapi.twitter.com%2F1%2Fstatuses%2Fupdate.json&include_entities"
            "%3Dtrue%26oauth_consumer_key%3Dxvz1evFS4wEEPTGEFPHBog%26oauth_nonce%3DkYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg"
            "%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1318622958%26oauth_token"
            "%3D370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb%26oauth_version%3D1.0"
            "%26status%3DHello%2520Ladies%2520%252B%2520Gentlemen%252C%2520a%2520signed%2520OAuth%2520request%2521", basestring_buffer)
    );

    printf("Twitter base string OK\n");

    char signature_buffer[1024] = {0};
    oauth_get_signature("POST", "https://api.twitter.com/1/statuses/update.json",
            params_buffer, &oauth, signature_buffer, sizeof(signature_buffer));

    g_assert(0 == strcmp("tnnArxj06cWHq44gCs1OSKk/jLY=", signature_buffer));

    printf("Twitter signature OK\n");
}

// Main
int main(int argc, char **argv)
{
    test_oauth_example();
    test_twitter_example();

    return 0;
}
