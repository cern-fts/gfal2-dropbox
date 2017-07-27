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
#include "gfal_dropbox_requests.h"
#include "gfal_dropbox_url.h"
#include <logger/gfal_logger.h>
#include <json.h>
#include <string.h>
#include <time.h>


struct DropboxDir {
    struct json_object* root;
    struct array_list* entries;
    int entries_length;
    int i;
    struct dirent ent;
    int has_more;
    const char *cursor;
};
typedef struct DropboxDir DropboxDir;


gfal_file_handle gfal2_dropbox_opendir(plugin_handle plugin_data,
        const char* url, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;

    char path[GFAL_URL_MAX_LEN];
    if (gfal2_dropbox_extract_path(url, path, sizeof(path)) == NULL) {
        gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid Dropbox url");
        return NULL;
    }

    // Otherwise we get: Specify the root folder as an empty string rather than as "/".
    if (g_strcmp0(path, "/") == 0) {
        path[0] = '\0';
    }

    char output[102400];

    ssize_t resp_size = gfal2_dropbox_post_json(dropbox, "https://api.dropbox.com/2/files/list_folder",
        output, sizeof(output), &tmp_err, 1,
        "path", path);
    if (resp_size < 0) {
        gfal2_propagate_prefixed_error(error, tmp_err, __func__);
        return NULL;
    }

    json_object* root = json_tokener_parse(output);
    if (root) {
        DropboxDir* dir_handle = calloc(1, sizeof(DropboxDir));
        dir_handle->root = root;

        json_object *cursor = NULL, *has_more = NULL;
        if (json_object_object_get_ex(root, "cursor", &cursor)) {
            dir_handle->cursor = json_object_get_string(cursor);
        }
        if (json_object_object_get_ex(root, "has_more", &has_more)) {
            dir_handle->has_more = json_object_get_boolean(has_more);
        }

        json_object* contents = NULL;
        if (json_object_object_get_ex(root, "entries", &contents) && json_object_is_type(contents, json_type_array)) {
            dir_handle->entries = json_object_get_array(contents);
            dir_handle->entries_length = json_object_array_length(contents);
            return gfal_file_handle_new2(gfal2_dropbox_getName(), dir_handle, NULL, url);
        }
        else {
            json_object_put(root);
            free(dir_handle);
            gfal2_set_error(error, dropbox_domain(), EIO, __func__, "The response didn't include 'entries'");
        }
    }
    else {
        gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Could not parse the response sent by Dropbox");
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

// TODO: Continue listing if has_more is true
struct dirent* gfal2_dropbox_readdirpp(plugin_handle plugin_data,
        gfal_file_handle dir_desc, struct stat* st, GError** error)
{
    DropboxDir* dir_handle = gfal_file_handle_get_fdesc(dir_desc);
    if (dir_handle->i >= dir_handle->entries_length) {
        return NULL;
    }

    json_object* entry = (json_object*)array_list_get_idx(dir_handle->entries, dir_handle->i++);

    json_object* name = NULL;
    if (json_object_object_get_ex(entry, "name", &name)) {
        const char* name_str = json_object_get_string(name);
        g_strlcpy(dir_handle->ent.d_name, name_str, sizeof(dir_handle->ent.d_name));
        dir_handle->ent.d_reclen = strlen(dir_handle->ent.d_name);
    }

    st->st_mode = 0700;

    json_object *tag = NULL, *size = NULL, *modified = NULL;

    if (json_object_object_get_ex(entry, ".tag", &tag)) {
        const char *tag_str = json_object_get_string(tag);
        if (g_strcmp0(tag_str, "folder") == 0) {
            st->st_mode |= S_IFDIR;
        }
    }

    if (json_object_object_get_ex(entry, "size", &size)) {
        st->st_size = json_object_get_int64(size);
    }

    if (json_object_object_get_ex(entry, "client_modified", &modified)) {
        st->st_mtime = gfal2_dropbox_time(json_object_get_string(modified));
    }

    return &dir_handle->ent;
}
