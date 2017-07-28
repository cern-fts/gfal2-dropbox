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

// Input/Output functions

#include "gfal_dropbox.h"
#include "gfal_dropbox_requests.h"
#include "gfal_dropbox_url.h"
#include <json.h>
#include <string.h>


struct DropboxIOHandler {
    int  flag;
    char path[GFAL_URL_MAX_LEN];
    char session_id[128];

    off_t size;
    off_t offset;
};
typedef struct DropboxIOHandler DropboxIOHandler;


static int gfal2_dropbox_open_write(DropboxHandle *dropbox, DropboxIOHandler *io_handler, GError **error)
{
    char output[512];
    ssize_t ret = gfal2_dropbox_perform(dropbox,
        M_POST, "https://content.dropboxapi.com/2/files/upload_session/start",
        0, 0,
        (char*)output, sizeof(output),
        "application/octet-stream", NULL, 0,
        error,
        0);
    if (ret < 0) {
        return -1;
    }

    json_object *resp = json_tokener_parse(output);
    json_object *session_id = NULL;
    if (!json_object_object_get_ex(resp, "session_id", &session_id)) {
        json_object_put(resp);
        gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Could not get the upload session id");
        return -1;
    }
    g_strlcpy(io_handler->session_id, json_object_get_string(session_id), sizeof(io_handler->session_id));
    json_object_put(resp);
    return 0;
}


gfal_file_handle gfal2_dropbox_fopen(plugin_handle plugin_data, const char* url,
        int flag, mode_t mode, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    GError* tmp_err = NULL;
    struct stat st;
    st.st_size = 0;

    flag &= O_ACCMODE;
    if (flag == O_RDWR) {
        gfal2_set_error(error, dropbox_domain(), EISDIR, __func__, "Only support read-only or write-only");
        return NULL;
    }

    int ret = gfal2_dropbox_stat(plugin_data, url, &st, &tmp_err);
    if (ret < 0) {
        if (tmp_err->code == ENOENT && (mode & O_CREAT)) {
            g_error_free(tmp_err);
            tmp_err = NULL;
        }
        else {
            gfal2_propagate_prefixed_error(error, tmp_err, __func__);
            return NULL;
        }
    }
    else if (S_ISDIR(st.st_mode)) {
        gfal2_set_error(error, dropbox_domain(), EISDIR, __func__, "Can not open a directory");
        return NULL;
    }

    DropboxIOHandler* io_handler = calloc(1, sizeof(DropboxIOHandler));
    gfal2_dropbox_extract_path(url, io_handler->path, sizeof(io_handler->path));
    io_handler->flag = flag;

    if (flag == O_WRONLY) {
        if (gfal2_dropbox_open_write(dropbox, io_handler, error) < 0) {
            free(io_handler);
            return NULL;
        }
    }

    io_handler->offset = 0;
    io_handler->size = st.st_size;
    return gfal_file_handle_new2(gfal2_dropbox_getName(), io_handler, NULL, url);
}


ssize_t gfal2_dropbox_fread(plugin_handle plugin_data, gfal_file_handle fd, void* buff,
        size_t count, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    DropboxIOHandler* io_handler = gfal_file_handle_get_fdesc(fd);

    if (io_handler->flag == O_WRONLY) {
        gfal2_set_error(error, dropbox_domain(), EBADF, __func__, "Can not read a file open for write");
        return -1;
    }

    if (io_handler->offset >= io_handler->size)
        return 0;

    json_object *req = json_object_new_object();
    json_object *path = json_object_new_string(io_handler->path);
    json_object_object_add(req, "path", path);

    const char *req_str = json_object_to_json_string(req);

    ssize_t ret = gfal2_dropbox_perform(dropbox, M_POST, "https://content.dropboxapi.com/2/files/download",
        io_handler->offset, count,
        (char*)buff, count,
        "text/plain", NULL, 0,
        error,
        1, "Dropbox-API-Arg", req_str);

    json_object_put(req);

    if (ret >= 0) {
        io_handler->offset += ret;
    }

    return ret;
}


ssize_t gfal2_dropbox_fwrite(plugin_handle plugin_data, gfal_file_handle fd,
        const void* buff, size_t count, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    DropboxIOHandler* io_handler = gfal_file_handle_get_fdesc(fd);

    if (io_handler->flag == O_RDONLY) {
        gfal2_set_error(error, dropbox_domain(), EBADF, __func__, "Can not write a file open for read");
        return -1;
    }

    json_object *req = json_object_new_object();
    json_object *cursor = json_object_new_object();
    json_object *session_id = json_object_new_string(io_handler->session_id);
    json_object *offset = json_object_new_int64(io_handler->offset);

    json_object_object_add(cursor, "session_id", session_id);
    json_object_object_add(cursor, "offset", offset);
    json_object_object_add(req, "cursor", cursor);

    const char *req_str = json_object_to_json_string(req);

    char output[1024];
    ssize_t ret = gfal2_dropbox_perform(dropbox,
        M_POST, "https://content.dropboxapi.com/2/files/upload_session/append_v2",
        0, 0,
        output, sizeof(output),
        "application/octet-stream", buff, count,
        error,
        1, "Dropbox-API-Arg", req_str);
    json_object_put(req);
    if (ret < 0) {
        return -1;
    }

    io_handler->offset += count;
    return count;
}


int gfal2_dropbox_fclose(plugin_handle plugin_data, gfal_file_handle fd, GError **error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    DropboxIOHandler* io_handler = gfal_file_handle_get_fdesc(fd);

    if (io_handler->flag == O_WRONLY) {
        json_object *req = json_object_new_object();

        json_object *cursor = json_object_new_object();
        json_object *session_id = json_object_new_string(io_handler->session_id);
        json_object *offset = json_object_new_int64(io_handler->offset);

        json_object_object_add(cursor, "session_id", session_id);
        json_object_object_add(cursor, "offset", offset);

        json_object *commit = json_object_new_object();
        json_object *path = json_object_new_string(io_handler->path);
        json_object *add = json_object_new_string("add");

        json_object_object_add(commit, "path", path);
        json_object_object_add(commit, "mode", add);

        json_object_object_add(req, "cursor", cursor);
        json_object_object_add(req, "commit", commit);

        const char *req_str = json_object_to_json_string(req);
        char output[1024];

        gfal2_dropbox_perform(dropbox,
            M_POST, "https://content.dropboxapi.com/2/files/upload_session/finish",
            0, 0,
            output, sizeof(output),
            "application/octet-stream", NULL, 0,
            error,
            1, "Dropbox-API-Arg", req_str);
        json_object_put(req);
    }

    free(io_handler);
    gfal_file_handle_delete(fd);
    return *error?-1:0;
}


off_t gfal2_dropbox_fseek(plugin_handle plugin_data, gfal_file_handle fd, off_t offset,
        int whence, GError** error)
{
    DropboxIOHandler* io_handler = gfal_file_handle_get_fdesc(fd);
    if (io_handler->flag & O_WRONLY) {
        gfal2_set_error(error, dropbox_domain(), EPERM, __func__, "Seek is only allowed for read file descriptors");
        return -1;
    }

    switch (whence) {
        case SEEK_SET:
            io_handler->offset = offset;
            break;
        case SEEK_CUR:
            io_handler->offset += offset;
            break;
        case SEEK_END:
            io_handler->offset = io_handler->size + offset - 1;
            break;
        default:
            gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid value for whence");
            return -1;
    }

    return 0;
}
