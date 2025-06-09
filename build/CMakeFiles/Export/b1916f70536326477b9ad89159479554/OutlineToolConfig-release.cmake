#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OutlineToolLib" for configuration "Release"
set_property(TARGET OutlineToolLib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OutlineToolLib PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "opencv_core;opencv_imgproc;opencv_imgcodecs"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/liboutlinetool.1.0.0.dylib"
  IMPORTED_SONAME_RELEASE "@rpath/liboutlinetool.1.dylib"
  )

list(APPEND _cmake_import_check_targets OutlineToolLib )
list(APPEND _cmake_import_check_files_for_OutlineToolLib "${_IMPORT_PREFIX}/lib/liboutlinetool.1.0.0.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
