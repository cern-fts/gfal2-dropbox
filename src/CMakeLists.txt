cmake_minimum_required (VERSION 2.6)

pkg_check_modules (GFAL2_PKG REQUIRED gfal2>=2.7.0)
pkg_check_modules (GLIB2_PKG REQUIRED glib-2.0)
pkg_check_modules (CURL_PKG REQUIRED libcurl)
pkg_check_modules (JSONC_PKG REQUIRED json)

include_directories (${GFAL2_PKG_INCLUDE_DIRS})
include_directories (${GLIB2_PKG_INCLUDE_DIRS})
include_directories (${CURL_PKG_INCLUDE_DIRS})
include_directories (${JSONC_PKG_INCLUDE_DIRS})

add_definitions (${GFAL2_PKG_CFLAGS})
add_definitions (${GLIB2_PKG_CFLAGS})
add_definitions (${CURL_PKG_CFLAGS})
add_definitions (${JSONC_PKG_CFLAGS})

file (GLOB src_dropbox "*.c")

add_library (gfal_plugin_dropbox SHARED ${src_dropbox})

target_link_libraries (gfal_plugin_dropbox ${GFAL2_PKG_LIBRARIES})
target_link_libraries (gfal_plugin_dropbox ${GLIB2_PKG_LIBRARIES})
target_link_libraries (gfal_plugin_dropbox ${CURL_PKG_LIBRARIES})
target_link_libraries (gfal_plugin_dropbox ${JSONC_PKG_LIBRARIES})

install (
    TARGETS gfal_plugin_dropbox
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
)
