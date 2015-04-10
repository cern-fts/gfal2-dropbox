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

// Plugin entry point

#include "gfal_dropbox.h"
#include <gfal_plugins_api.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


const GQuark dropbox_domain() {
    return g_quark_from_static_string("dropbox");
}


// Plugin unique name
const char* gfal2_dropbox_getName()
{
    return GFAL2_PLUGIN_VERSIONED("dropbox", VERSION);
}


// Returns true if the plugin recognises the URL
static gboolean gfal2_dropbox_check_url(plugin_handle plugin_data,
        const char* url, plugin_mode operation, GError** err)
{
    switch (operation) {
        case GFAL_PLUGIN_RENAME:
        case GFAL_PLUGIN_STAT:
        case GFAL_PLUGIN_LSTAT:
        case GFAL_PLUGIN_MKDIR:
        case GFAL_PLUGIN_RMDIR:
        case GFAL_PLUGIN_OPENDIR:
        case GFAL_PLUGIN_OPEN:
        case GFAL_PLUGIN_UNLINK:
            return strncmp(url, "dropbox:", 8) == 0;
        default:
            return FALSE;
    }
}


// Frees the memory used by the plugin data
static void gfal2_dropbox_delete_data(plugin_handle plugin_data)
{
    DropboxHandle* dropbox = (DropboxHandle*)(plugin_data);
    curl_easy_cleanup(dropbox->curl_handle);
    free(dropbox);
}


// Logging callback
static int gfal2_dropbox_debug_callback(CURL *handle, curl_infotype type,
        char *data, size_t size, void *userptr)
{
    char msg_fmt[64];
    switch (type) {
        case CURLINFO_TEXT:
            snprintf(msg_fmt, sizeof(msg_fmt), "INFO: %%.%zds", size - 1); // Mute \n
            gfal_log(GFAL_VERBOSE_VERBOSE, msg_fmt, data);
            break;
        case CURLINFO_HEADER_IN:
            snprintf(msg_fmt, sizeof(msg_fmt), "HEADER IN: %%.%zds", size - 2); // Mute \n\r
            gfal_log(GFAL_VERBOSE_DEBUG, msg_fmt, data);
            break;
        case CURLINFO_HEADER_OUT:
            snprintf(msg_fmt, sizeof(msg_fmt), "HEADER OUT: %%.%zds", size - 2); // Mute \n\r
            gfal_log(GFAL_VERBOSE_DEBUG, msg_fmt, data);
            break;
        case CURLINFO_DATA_IN:
            snprintf(msg_fmt, sizeof(msg_fmt), "DATA IN: %%.%zds", size);
            //gfal_log(GFAL_VERBOSE_TRACE, msg_fmt, data);
            break;
        case CURLINFO_DATA_OUT:
            snprintf(msg_fmt, sizeof(msg_fmt), "DATA OUT: %%.%zds", size);
            gfal_log(GFAL_VERBOSE_TRACE, msg_fmt, data);
            break;
        default:
            break;
    }
    return 0;
}


// Set logging
static void gfal2_dropbox_set_logging(DropboxHandle* dropbox)
{
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(dropbox->curl_handle, CURLOPT_DEBUGFUNCTION, gfal2_dropbox_debug_callback);
}

// GFAL2 will look for this symbol to register the plugin
gfal_plugin_interface gfal_plugin_init(gfal2_context_t handle, GError** err)
{
    gfal_plugin_interface dropbox_plugin;
    memset(&dropbox_plugin, 0, sizeof(gfal_plugin_interface));

    DropboxHandle* dropbox = calloc(1, sizeof(DropboxHandle));
    dropbox->curl_handle = curl_easy_init();
    dropbox->gfal2_context = handle;

    gfal2_dropbox_set_logging(dropbox);

    dropbox_plugin.plugin_data = dropbox;
    dropbox_plugin.plugin_delete = gfal2_dropbox_delete_data;

    dropbox_plugin.getName = gfal2_dropbox_getName;
    dropbox_plugin.check_plugin_url = gfal2_dropbox_check_url;

    dropbox_plugin.opendirG = gfal2_dropbox_opendir;
    dropbox_plugin.readdirG = gfal2_dropbox_readdir;
    dropbox_plugin.readdirppG = gfal2_dropbox_readdirpp;
    dropbox_plugin.closedirG = gfal2_dropbox_closedir;

    dropbox_plugin.statG = gfal2_dropbox_stat;
    dropbox_plugin.lstatG = gfal2_dropbox_stat;
    dropbox_plugin.mkdirpG = gfal2_dropbox_mkdir;
    dropbox_plugin.rmdirG = gfal2_dropbox_rmdir;
    dropbox_plugin.unlinkG = gfal2_dropbox_unlink;
    dropbox_plugin.renameG = gfal2_dropbox_rename;

    dropbox_plugin.openG = gfal2_dropbox_fopen;
    dropbox_plugin.closeG = gfal2_dropbox_fclose;
    dropbox_plugin.readG = gfal2_dropbox_fread;
    dropbox_plugin.writeG = gfal2_dropbox_fwrite;
    dropbox_plugin.lseekG = gfal2_dropbox_fseek;

    return dropbox_plugin;
}
