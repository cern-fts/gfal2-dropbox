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

// Namespace operations, except listing dir

#include "gfal_dropbox.h"
#include "gfal_dropbox_requests.h"
#include "gfal_dropbox_url.h"
#include <common/gfal_common_err_helpers.h>
#include <json.h>
#include <string.h>


int gfal2_dropbox_stat(plugin_handle plugin_data, const char* url,
        struct stat *buf, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    char url_buffer[GFAL_URL_MAX_LEN];
    gfal2_dropbox_build_url("https://api.dropbox.com/1/metadata/auto", url,
            url_buffer, sizeof(url_buffer), &tmp_err);
    if (tmp_err) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }

    char buffer[1024];
    ssize_t ret = gfal2_dropbox_get(dropbox, url_buffer, buffer, sizeof(buffer), &tmp_err, 1, "list", "false");
    if (ret > 0) {
        json_object* stat = json_tokener_parse(buffer);
        if (stat) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = 0700;

            json_object* aux = json_object_object_get(stat, "is_deleted");
            if (aux && json_object_get_boolean(aux)) {
                gfal2_set_error(error, dropbox_domain(), ENOENT, __func__,
                           "The entry has been deleted");
                return -1;
            }

            aux = json_object_object_get(stat, "is_dir");

            if (aux && json_object_get_boolean(aux))
                buf->st_mode |= S_IFDIR;

            aux = json_object_object_get(stat, "bytes");
            if (aux)
                buf->st_size = json_object_get_int64(aux);

            json_object_put(stat);
        }
        else {
            gfal2_set_error(error, dropbox_domain(), EIO, __func__,
                    "Could not parse the response sent by Dropbox");
        }
    }
    else {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
    }
    return *error?-1:0;
}


int gfal2_dropbox_mkdir(plugin_handle plugin_data, const char* url, mode_t mode,
        gboolean rec_flag, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    struct stat st;
    int ret = gfal2_dropbox_stat(plugin_data, url, &st, &tmp_err);
    if (ret == 0) {
        gfal2_set_error(error, dropbox_domain(), EEXIST, __func__, "The directory already exists");
        return -1;
    }
    else if (tmp_err->code != ENOENT) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }
    else {
        g_error_free(tmp_err);
        tmp_err = NULL;
    }

    char path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(url, path, sizeof(path)) < 0) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_post(dropbox,
            "https://api.dropbox.com/1/fileops/create_folder", NULL, NULL,
            output, sizeof(output), &tmp_err,
            2, "root", "auto", "path", path);
    if (resp_size < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }
    return 0;
}


int gfal2_dropbox_rmdir(plugin_handle plugin_data, const char* url,
        GError** error)
{
    return gfal2_dropbox_unlink(plugin_data, url, error);
}


int gfal2_dropbox_unlink(plugin_handle plugin_data, const char* url,
        GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    struct stat st;
    int ret = gfal2_dropbox_stat(plugin_data, url, &st, &tmp_err);
    if (ret < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }

    char path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(url, path, sizeof(path)) < 0) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_post(dropbox,
            "https://api.dropbox.com/1/fileops/delete", NULL, NULL,
            output, sizeof(output), &tmp_err,
            2, "root", "auto", "path", path);
    if (resp_size < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }
    return 0;
}


int gfal2_dropbox_rename(plugin_handle plugin_data, const char * oldurl,
        const char * urlnew, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    struct stat st;
    int ret = gfal2_dropbox_stat(plugin_data, oldurl, &st, &tmp_err);
    if (ret < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }

    char from_path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(oldurl, from_path, sizeof(from_path)) < 0) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char to_path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(urlnew, to_path, sizeof(to_path)) < 0) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_post(dropbox,
            "https://api.dropbox.com/1/fileops/move", NULL, NULL,
            output, sizeof(output), &tmp_err,
            2, "from_path", from_path, "to_path", to_path);
    if (resp_size < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }
    return 0;
}
