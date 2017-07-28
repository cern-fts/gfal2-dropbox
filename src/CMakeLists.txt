cmake_minimum_required (VERSION 2.6)

# json-c pkgconfig is installed under /lib64 instead of /usr/lib64
# https://bugzilla.redhat.com/show_bug.cgi?id=1158842
set (ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/lib64/pkgconfig:/lib/pkgconfig")

find_package (GFAL2 REQUIRED)
find_package (GLIB2 REQUIRED)
find_package (CURL REQUIRED)
find_package (JSONC REQUIRED)
find_package (OpenSSL REQUIRED)

include_directories (
    ${GFAL2_INCLUDE_DIR}
    ${GLIB2_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

add_definitions (${GFAL2_DEFINITIONS})
add_definitions (${GLIB2_DEFINITIONS})
add_definitions (${CURL_PKG_CFLAGS})
add_definitions (${OPENSSL_PKG_CFLAGS})

file (GLOB src_dropbox "*.c")

add_library (gfal_plugin_dropbox SHARED ${src_dropbox})

target_link_libraries (gfal_plugin_dropbox
    ${GFAL2_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${CURL_LIBRARIES}
    ${JSONC_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

install (
    TARGETS gfal_plugin_dropbox
    LIBRARY DESTINATION ${GFAL2_PLUGIN_INSTALL_DIR}
)

add_subdirectory (tests)
