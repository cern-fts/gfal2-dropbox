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

// Directory listing functions

#include "gfal_dropbox.h"
#include "gfal_dropbox_helpers.h"
#include <logger/gfal_logger.h>
#include <common/gfal_common_err_helpers.h>
#include <json.h>
#include <string.h>
#include <time.h>


struct DropboxDir {
    json_object* root;
    struct array_list* contents;
    int content_lenght;
    int i;
    struct dirent ent;
};
typedef struct DropboxDir DropboxDir;


static time_t _dropbox_time(const char* stime)
{
    struct tm tim;
    strptime(stime, "%a, %d %b %Y %H:%M:%S %z", &tim);
    return mktime(&tim);
}


gfal_file_handle gfal2_dropbox_opendir(plugin_handle plugin_data,
        const char* url, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    char url_buffer[GFAL_URL_MAX_LEN];
    gfal2_dropbox_build_url(
            "https://api.dropbox.com/1/metadata/auto/", url,
            "list=true", url_buffer, sizeof(url_buffer), &tmp_err
    );
    if (tmp_err) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return NULL;
    }

    char buffer[102400];
    ssize_t ret = gfal2_dropbox_get(dropbox, url_buffer, buffer, sizeof(buffer), &tmp_err);
    if (ret > 0) {
        json_object* root = json_tokener_parse(buffer);
        if (root) {
            DropboxDir* dir_handle = calloc(1, sizeof(DropboxDir));
            dir_handle->root = root;
            json_object* contents = json_object_object_get(root, "contents");

            if (contents && json_object_is_type(contents, json_type_array)) {
                dir_handle->contents = json_object_get_array(contents);
                dir_handle->content_lenght = json_object_array_length(contents);
                return gfal_file_handle_new2(gfal2_dropbox_getName(), dir_handle, NULL, url);
            }
            else {
                json_object_put(root);
                free(dir_handle);
                gfal2_set_error(error, dropbox_domain(), EIO, __func__, "The response didn't include 'content'");
            }
        }
        else {
            gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Could not parse the response sent by Dropbox");
        }
    }
    else {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
    }

    return NULL;
}


int gfal2_dropbox_closedir(plugin_handle plugin_data, gfal_file_handle dir_desc,
        GError** error)
{
    DropboxDir* dir_handle = gfal_file_handle_get_fdesc(dir_desc);
    json_object_put(dir_handle->root);
    free(dir_handle);
    gfal_file_handle_delete(dir_desc);
    return 0;
}


struct dirent* gfal2_dropbox_readdir(plugin_handle plugin_data,
        gfal_file_handle dir_desc, GError** error)
{
    struct stat st;
    return gfal2_dropbox_readdirpp(plugin_data, dir_desc, &st, error);
}


struct dirent* gfal2_dropbox_readdirpp(plugin_handle plugin_data,
        gfal_file_handle dir_desc, struct stat* st, GError** error)
{
    DropboxDir* dir_handle = gfal_file_handle_get_fdesc(dir_desc);
    if (dir_handle->i >= dir_handle->content_lenght)
        return NULL;

    json_object* entry = (json_object*)array_list_get_idx(dir_handle->contents, dir_handle->i++);

    json_object* aux = json_object_object_get(entry, "path");
    if (aux) {
        const char* path = json_object_get_string(aux);
        const char* fname = strrchr(path, '/');
        if (fname)
            fname++;
        else
            fname = path;
        strncpy(dir_handle->ent.d_name, fname, sizeof(dir_handle->ent.d_name));
        dir_handle->ent.d_reclen = strlen(dir_handle->ent.d_name);
    }

    st->st_mode = 0700;
    aux = json_object_object_get(entry, "is_dir");
    if (aux && json_object_get_boolean(aux))
        st->st_mode |= S_IFDIR;

    aux = json_object_object_get(entry, "bytes");
    if (aux)
        st->st_size = json_object_get_int64(aux);

    aux = json_object_object_get(entry, "modified");
    if (aux)
        st->st_mtime = _dropbox_time(json_object_get_string(aux));

    return &dir_handle->ent;
}
