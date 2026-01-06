#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "serverlink::serverlink" for configuration "Release"
set_property(TARGET serverlink::serverlink APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(serverlink::serverlink PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libserverlink.so.0.1.0"
  IMPORTED_SONAME_RELEASE "libserverlink.so.0"
  )

list(APPEND _cmake_import_check_targets serverlink::serverlink )
list(APPEND _cmake_import_check_files_for_serverlink::serverlink "${_IMPORT_PREFIX}/lib/libserverlink.so.0.1.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
