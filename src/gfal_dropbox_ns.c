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
#include <json.h>
#include <string.h>


int gfal2_dropbox_stat(plugin_handle plugin_data, const char* url,
        struct stat *buf, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    char path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(url, path, sizeof(path)) == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    // Handle root ourselves, since Dropbox will say it is not supported
    if (g_strcmp0(path, "/") == 0) {
        buf->st_mode = 0700 | S_IFDIR;
        return 0;
    }

    char output[1024];

    ssize_t resp_size = gfal2_dropbox_post_json(dropbox, "https://api.dropbox.com/2/files/get_metadata",
        output, sizeof(output), &tmp_err, 1,
        "path", path);
    if (resp_size < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }

    json_object* stat = json_tokener_parse(output);
    if (stat) {
        memset(buf, 0, sizeof(struct stat));
        buf->st_mode = 0700;

        json_object* tag = NULL;
        if (json_object_object_get_ex(stat, ".tag", &tag)) {
            const char *tag_str = json_object_get_string(tag);

            if (g_strcmp0(tag_str, "folder") == 0) {
                buf->st_mode |= S_IFDIR;
            }
            else if (g_strcmp0(tag_str, "file") == 0) {
                json_object *size = NULL;
                if (json_object_object_get_ex(stat, "size", &size)) {
                    buf->st_size = json_object_get_int64(size);
                }

                json_object *modified = NULL;
                if (json_object_object_get_ex(stat, "client_modified", &modified)) {
                    const char *time_str = json_object_get_string(modified);
                    buf->st_atime = buf->st_mtime = buf->st_ctime = gfal2_dropbox_time(time_str);
                }
            }
            else if (g_strcmp0(tag_str, "deleted") == 0) {
                gfal2_set_error(error, dropbox_domain(), ENOENT, __func__, "The entry has been deleted");
            }
            else {
                gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Unsupported .tag: %s", tag_str);
            }
        }
        else {
            gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Could not find .tag");
        }

        json_object_put(stat);
    }
    else {
        gfal2_set_error(error, dropbox_domain(), EIO, __func__,
                "Could not parse the response sent by Dropbox");
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
    if (gfal2_dropbox_extract_path(url, path, sizeof(path)) == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_post_json(dropbox,
        "https://api.dropboxapi.com/2/files/create_folder_v2",
        output, sizeof(output), &tmp_err,
        1, "path", path);
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
    if (gfal2_dropbox_extract_path(url, path, sizeof(path)) == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_post_json(dropbox,
        "https://api.dropboxapi.com/2/files/delete_v2",
        output, sizeof(output), &tmp_err,
        1, "path", path);
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
    if (gfal2_dropbox_extract_path(oldurl, from_path, sizeof(from_path))  == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char to_path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(urlnew, to_path, sizeof(to_path)) == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return -1;
    }

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_post_json(dropbox,
        "https://api.dropboxapi.com/2/files/move_v2",
        output, sizeof(output), &tmp_err,
        2, "from_path", from_path, "to_path", to_path);
    if (resp_size < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return -1;
    }
    return 0;
}
