cmake_minimum_required (VERSION 2.6)

project (gfal2-plugin-dropbox)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 1)
set (VERSION_PATCH 1)
set (VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

add_definitions(-DVERSION="${VERSION_STRING}")

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

include (DefineInstallationPaths REQUIRED)
include (CMakeGeneratePkgConfig REQUIRED)

set (GFAL2_PLUGIN_INSTALL_DIR
    "${LIB_INSTALL_DIR}/gfal2-plugins/"
    CACHE PATH "The subdirectory relative to the install prefix where plugins will be installed"
)

find_package (PkgConfig REQUIRED)

add_definitions (-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE)

enable_testing()

add_subdirectory (dist)
add_subdirectory (src)

install (
    FILES "README" "LICENSE" "RELEASE-NOTES"
    DESTINATION ${DOC_INSTALL_DIR}
)
