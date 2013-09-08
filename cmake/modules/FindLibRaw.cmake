# - Find LibRaw
# Find the LibRaw library
# This module defines
#  LibRaw_INCLUDE_DIR, where to find libraw.h
#  LibRaw_LIBRARIES, the libraries needed to use LibRaw
#  LibRaw_VERSION_STRING, the version string of LibRaw
#  LibRaw_DEFINITIONS, the definitions needed to use LibRaw


# Copyright (c) 2013, Pino Toscano <pino@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
   pkg_check_modules(PC_LIBRAW libraw)
   set(LibRaw_DEFINITIONS ${PC_LIBRAW_CFLAGS_OTHER})
endif()

find_path(LibRaw_INCLUDE_DIR libraw.h
   HINTS
   ${PC_LIBRAW_INCLUDEDIR}
   ${PC_LibRaw_INCLUDE_DIRS}
   PATH_SUFFIXES libraw
)

find_library(LibRaw_LIBRARIES NAMES raw
   HINTS
   ${PC_LIBRAW_LIBDIR}
   ${PC_LIBRAW_LIBRARY_DIRS}
)

if(LibRaw_INCLUDE_DIR)
   file(READ ${LibRaw_INCLUDE_DIR}/libraw_version.h _libraw_version_content)
   string(REGEX MATCH "#define LIBRAW_MAJOR_VERSION[ ]*([0-9]*)\n" _version_major_match ${_libraw_version_content})
   set(_libraw_version_major "${CMAKE_MATCH_1}")
   string(REGEX MATCH "#define LIBRAW_MINOR_VERSION[ ]*([0-9]*)\n" _version_minor_match ${_libraw_version_content})
   set(_libraw_version_minor "${CMAKE_MATCH_1}")
   string(REGEX MATCH "#define LIBRAW_PATCH_VERSION[ ]*([0-9]*)\n" _version_patch_match ${_libraw_version_content})
   set(_libraw_version_patch "${CMAKE_MATCH_1}")
   if(_version_major_match AND _version_minor_match AND _version_patch_match)
      set(LibRaw_VERSION_STRING "${_libraw_version_major}.${_libraw_version_minor}.${_libraw_version_patch}")
   else()
      if(NOT LibRaw_FIND_QUIETLY)
         message(STATUS "Failed to get version information from ${LibRaw_INCLUDE_DIR}/libraw_version.h")
      endif()
   endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibRaw
   REQUIRED_VARS LibRaw_LIBRARIES LibRaw_INCLUDE_DIR
   VERSION_VAR LibRaw_VERSION_STRING
)

mark_as_advanced(LibRaw_INCLUDE_DIR
   LibRaw_LIBRARIES
   LibRaw_VERSION_STRING
   LibRaw_DEFINITIONS
)

