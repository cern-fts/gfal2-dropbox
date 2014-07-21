cmake_minimum_required (VERSION 2.6)

add_executable (test_oauth_sign_bin test_oauth_sign.c)
target_link_libraries (test_oauth_sign_bin gfal_plugin_dropbox)

add_test(test_oauth_sign test_oauth_sign_bin)
