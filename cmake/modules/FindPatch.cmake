# - Find Patch
# Find the Patch tool  <http://savannah.gnu.org/projects/patch/>
# This module defines
#  PATCH_EXECUTABLE, the patch executable
#
# Copyright (c) 2013, Gilles Caulier <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (NOT PATCH_EXECUTABLE)

    MESSAGE(STATUS "Checking for patch executable...")

    FIND_PROGRAM(PATCH_EXECUTABLE patch)
    SET(PATCH_EXECUTABLE ${PATCH_EXECUTABLE} CACHE STRING "")

    IF (PATCH_EXECUTABLE)
        
        MESSAGE(STATUS "Patch found: ${PATCH_EXECUTABLE}")
        MARK_AS_ADVANCED(PATCH_EXECUTABLE)

    ELSE()
    
        MESSAGE(FATAL_ERROR "Patch executable not found")
    
    ENDIF()
                
    MESSAGE("")
ENDIF()
