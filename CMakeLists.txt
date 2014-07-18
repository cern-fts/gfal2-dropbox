cmake_minimum_required (VERSION 2.6)

project (gfal2-plugin-dropbox)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 0)
set (VERSION_PATCH 1)
set (VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")

include (DefineInstallationPaths REQUIRED)
include (CMakeGeneratePkgConfig REQUIRED)

find_package (PkgConfig REQUIRED)

add_definitions (-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE)

add_subdirectory (src)
add_subdirectory (dist)

install (
    FILES "README" "LICENSE" "RELEASE-NOTES"
    DESTINATION ${DOC_INSTALL_DIR}
)
