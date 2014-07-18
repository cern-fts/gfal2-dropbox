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
#include "gfal_dropbox_helpers.h"
#include <common/gfal_common_err_helpers.h>
#include <json.h>
#include <string.h>


struct DropboxIOHandler {
    int  flag;
    char url[GFAL_URL_MAX_LEN], original[GFAL_URL_MAX_LEN];
    char upload_id[128];
    int got_upload_id;

    off_t size;
    off_t cur;

    int dirty;
};
typedef struct DropboxIOHandler DropboxIOHandler;


gfal_file_handle gfal2_dropbox_fopen(plugin_handle plugin_data, const char* url,
        int flag, mode_t mode, GError** error)
{
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
    strncpy(io_handler->original, url, sizeof(io_handler->original));
    io_handler->flag = flag;

    if (flag == O_RDONLY) {
        gfal2_dropbox_build_url(
                "https://api-content.dropbox.com/1/files/auto/",
                url, NULL, io_handler->url, sizeof(io_handler->url), &tmp_err
        );
    }
    else {
        strncpy(io_handler->url, "https://api-content.dropbox.com/1/chunked_upload", sizeof(io_handler->url));
    }

    io_handler->cur = 0;
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

    if (io_handler->cur >= io_handler->size)
        return 0;

    ssize_t ret = gfal2_dropbox_get_range(dropbox, io_handler->url, io_handler->cur, count,
            (char*)buff, count, error);

    if (ret >= 0)
        io_handler->cur += ret;

    return ret;
}


ssize_t gfal2_dropbox_fwrite(plugin_handle plugin_data, gfal_file_handle fd,
        const void* buff, size_t count, GError** error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    DropboxIOHandler* io_handler = gfal_file_handle_get_fdesc(fd);

    if (io_handler->flag == O_RDONLY) {
        io_handler->dirty = 1;
        gfal2_set_error(error, dropbox_domain(), EBADF, __func__, "Can not write a file open for read");
        return -1;
    }

    char url_buffer[GFAL_URL_MAX_LEN];
    if (io_handler->got_upload_id)
        snprintf(url_buffer, sizeof(url_buffer), "%s?%s&offset=%zd", io_handler->url, io_handler->upload_id, io_handler->cur);
    else
        strncpy(url_buffer, io_handler->url, sizeof(url_buffer));

    char output[1024];
    ssize_t resp_size = gfal2_dropbox_put(dropbox, url_buffer, buff, count, output, sizeof(output), error);
    if (resp_size < 0) {
        io_handler->dirty = 1;
        return -1;
    }
    else if (!io_handler->got_upload_id) {
        json_object* response = json_tokener_parse(output);
        json_object* up_id = json_object_object_get(response, "upload_id");
        if (up_id) {
            snprintf(io_handler->upload_id, sizeof(io_handler->upload_id), "upload_id=%s", json_object_get_string(up_id));
            io_handler->got_upload_id = 1;
        }
        json_object_put(response);
    }

    io_handler->cur += count;
    return count;
}


int gfal2_dropbox_fclose(plugin_handle plugin_data, gfal_file_handle fd, GError **error)
{
    DropboxHandle* dropbox = (DropboxHandle*)plugin_data;
    DropboxIOHandler* io_handler = gfal_file_handle_get_fdesc(fd);

    if (io_handler->flag == O_WRONLY && io_handler->upload_id[0] && !io_handler->dirty) {
        gfal2_dropbox_build_url(
                    "https://api-content.dropbox.com/1/commit_chunked_upload/auto/",
                    io_handler->original, io_handler->upload_id,
                    io_handler->url, sizeof(io_handler->url), error
        );
        if (*error == NULL) {
            char output[1024];
            ssize_t resp_size = gfal2_dropbox_post(dropbox,
                    io_handler->url, NULL, NULL,
                    output, sizeof(output), error);

            if (resp_size < 0) {
                gfal2_set_error(error, dropbox_domain(), EIO, __func__, "Could not finish the write");
            }
        }
    }
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
            io_handler->cur = offset;
            break;
        case SEEK_CUR:
            io_handler->cur += offset;
            break;
        case SEEK_END:
            io_handler->cur = io_handler->size + offset - 1;
            break;
        default:
            gfal2_set_error(error, dropbox_domain(), EINVAL, __func__, "Invalid value for whence");
            return -1;
    }

    return 0;
}
