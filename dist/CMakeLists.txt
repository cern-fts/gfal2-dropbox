cmake_minimum_required (VERSION 2.6)

install (FILES etc/gfal2.d/dropbox_plugin.conf
         DESTINATION ${SYSCONF_INSTALL_DIR}/gfal2.d/)
