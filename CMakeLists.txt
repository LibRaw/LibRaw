# ===========================================================
#
# This file is a part of Libraw project
# <a href="http://www.libraw.org">http://www.libraw.org</a>
#
# @brief  CMake build file for LibRaw
#
# ============================================================

CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

# Determine if libraw is built as a subproject (using add_subdirectory)
# or if it is the master project.
set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MASTER_PROJECT ON)
    message(STATUS "CMake version: ${CMAKE_VERSION}")
endif ()

# ==================================================================================================
# Library version info extraction

FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/libraw/libraw_version.h _libraw_version_content)

# API version strings
STRING(REGEX MATCH "#define LIBRAW_MAJOR_VERSION[ \t]*([0-9]*)\n" _version_major_match ${_libraw_version_content})
SET(RAW_LIB_MAJOR_VERSION "${CMAKE_MATCH_1}")

STRING(REGEX MATCH "#define LIBRAW_MINOR_VERSION[ \t]*([0-9]*)\n" _version_minor_match ${_libraw_version_content})
SET(RAW_LIB_MINOR_VERSION "${CMAKE_MATCH_1}")

STRING(REGEX MATCH "#define LIBRAW_PATCH_VERSION[ \t]*([0-9]*)\n" _version_patch_match ${_libraw_version_content})
SET(RAW_LIB_PATCH_VERSION "${CMAKE_MATCH_1}")

# ABI version strings
STRING(REGEX MATCH "#define LIBRAW_SHLIB_CURRENT[ \t]*([0-9]*)\n" _version_socur_match ${_libraw_version_content})
SET(RAW_LIB_SO_CUR_VERSION "${CMAKE_MATCH_1}")

STRING(REGEX MATCH "#define LIBRAW_SHLIB_REVISION[ \t]*([0-9]*)\n" _version_sorev_match ${_libraw_version_content})
SET(RAW_LIB_SO_REV_VERSION "${CMAKE_MATCH_1}")

STRING(REGEX MATCH "#define LIBRAW_SHLIB_AGE[ \t]*([0-9]*)\n" _version_soage_match ${_libraw_version_content})
SET(RAW_LIB_SO_AGE_VERSION "${CMAKE_MATCH_1}")

# Set env. variables accordingly.
SET(RAW_LIB_VERSION_STRING    "${RAW_LIB_MAJOR_VERSION}.${RAW_LIB_MINOR_VERSION}.${RAW_LIB_PATCH_VERSION}")
SET(RAW_LIB_SO_VERSION_STRING "${RAW_LIB_SO_CUR_VERSION}.${RAW_LIB_SO_REV_VERSION}.${RAW_LIB_SO_AGE_VERSION}")

MESSAGE(STATUS "LibRaw string version: ${RAW_LIB_VERSION_STRING}")
MESSAGE(STATUS "LibRaw SO version:     ${RAW_LIB_SO_VERSION_STRING}")

project(libraw VERSION ${RAW_LIB_VERSION_STRING} LANGUAGES CXX C)

# ==================================================================================================
# Project Options
OPTION(BUILD_SHARED_LIBS           "Build library as shared library                 (default=ON)"                 ON)
OPTION(ENABLE_OPENMP               "Build library with OpenMP support               (default=ON)"                 ON)
OPTION(ENABLE_LCMS                 "Build library with LCMS support                 (default=ON)"                 ON)
OPTION(ENABLE_EXAMPLES             "Build library with sample command-line programs (default=OFF)"                OFF)
OPTION(ENABLE_RAWSPEED             "Build library with extra RawSpeed codec support (default=OFF)"                OFF)
OPTION(ENABLE_DCRAW_DEBUG          "Build library with debug message from dcraw     (default=OFF)"                OFF)
OPTION(ENABLE_X3FTOOLS             "Build library with Foveon X3F support           (default=ON)"                 ON)
option(LIBRAW_INSTALL              "Generate the install target."                                    ${MASTER_PROJECT})
option(LIBRAW_UNINSTALL_TARGET     "Add a custom target to ease removal of installed targets"        ${MASTER_PROJECT})

# ==================================================================================================
# General definitions rules

SET(LIB_SUFFIX "" CACHE STRING "Define suffix of lib directory name (32/64)" )

IF(WIN32 AND NOT DEFINED CMAKE_DEBUG_POSTFIX)
    SET(CMAKE_DEBUG_POSTFIX "d")
ENDIF()

# To prevent warnings from M$ compiler
IF(WIN32 AND MSVC)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_ATL_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_AFX_SECURE_NO_WARNINGS)
ENDIF()

# Under Windows, use specific flag to compile.
IF(WIN32)
    ADD_DEFINITIONS(-DDJGPP)
ENDIF()

# -- Check dependencies --------------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

INCLUDE(MacroBoolTo01)
INCLUDE(MacroLogFeature)

# Math library check
IF(NOT WIN32)
    FIND_LIBRARY(MATH_LIBRARY m)
ENDIF()

# LCMS version 1 and 2 library check
SET(LCMS_SUPPORT_CAN_BE_COMPILED false)
IF (ENABLE_LCMS)
    MESSAGE(STATUS "Check for LCMS2 availability...")
    FIND_PACKAGE(LCMS2)
    IF (LCMS2_FOUND AND (LCMS2_VERSION VERSION_EQUAL 2.1 OR LCMS2_VERSION VERSION_GREATER 2.1))
        MESSAGE(STATUS "Found LCMS2 : ${LCMS2_LIBRARIES} ${LCMS2_INCLUDE_DIR}")
        INCLUDE_DIRECTORIES(${LCMS2_INCLUDE_DIR})
        MACRO_LOG_FEATURE(LCMS2_FOUND "LCMS2" "A small-footprint color management engine" "http://www.littlecms.com" FALSE "" "Needed by libkdcraw")
        ADD_DEFINITIONS(-DUSE_LCMS2)
        SET(LCMS_SUPPORT_CAN_BE_COMPILED true)
    ELSE ()
        MESSAGE(STATUS "Check for LCMS availability instead LCMS2...")
        FIND_PACKAGE(LCMS)
        IF (LCMS_FOUND)
            MESSAGE(STATUS "Found LCMS1: ${LCMS_LIBRARIES} ${LCMS_INCLUDE_DIR}")
            INCLUDE_DIRECTORIES(${LCMS_INCLUDE_DIR})
            MACRO_LOG_FEATURE(LCMS_FOUND "LCMS1" "A small-footprint color management engine" "http://www.littlecms.com" TRUE "" "Needed by libkdcraw")
            ADD_DEFINITIONS(-DUSE_LCMS)
            SET(LCMS2_LIBRARIES ${LCMS_LIBRARIES})
            SET(LCMS_SUPPORT_CAN_BE_COMPILED true)
        ENDIF ()
    ENDIF ()
ENDIF()
MACRO_BOOL_TO_01(LCMS_SUPPORT_CAN_BE_COMPILED LIBRAW_USE_LCMS)

# zlib library check
FIND_PACKAGE(ZLIB)
IF(ZLIB_FOUND)
    ADD_DEFINITIONS(-DUSE_ZLIB)
ENDIF()
MACRO_BOOL_TO_01(ZLIB_FOUND LIBRAW_USE_ZLIB)

# JPEG library check
FIND_PACKAGE(JPEG)
IF(JPEG_FOUND)
    ADD_DEFINITIONS(-DUSE_JPEG)
ENDIF()
MACRO_BOOL_TO_01(JPEG_FOUND LIBRAW_USE_JPEG)

# OpenMP
IF(ENABLE_OPENMP)
    find_package(OpenMP)
ENDIF()
MACRO_BOOL_TO_01(OpenMP_FOUND LIBRAW_USE_OPENMP)

# Jasper library check
FIND_PACKAGE(Jasper)
IF(JASPER_FOUND)
    ADD_DEFINITIONS(-DUSE_JASPER)
    INCLUDE_DIRECTORIES(${JASPER_INCLUDE_DIR})
ENDIF()
MACRO_BOOL_TO_01(JASPER_FOUND LIBRAW_USE_JASPER)

# RawSpeed dependencies
SET(RAWSPEED_SUPPORT_CAN_BE_COMPILED false)
IF(ENABLE_RAWSPEED)
    FIND_PACKAGE(LibXml2)
    FIND_PACKAGE(Threads REQUIRED)
    IF(LIBXML2_FOUND AND Threads_FOUND)
        SET(RAWSPEED_SUPPORT_CAN_BE_COMPILED true)
        ADD_DEFINITIONS(-DUSE_RAWSPEED)
    ENDIF()
ENDIF()
MACRO_BOOL_TO_01(RAWSPEED_SUPPORT_CAN_BE_COMPILED LIBRAW_USE_RAWSPEED)

# For registration to libraw_config.h
MACRO_BOOL_TO_01(ENABLE_DCRAW_DEBUG LIBRAW_USE_DCRAW_DEBUG)
MACRO_BOOL_TO_01(ENABLE_X3FTOOLS LIBRAW_USE_X3FTOOLS)

# Create a config header for client application.
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/data/libraw_config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libraw_config.h)

# Put the include dirs which are in the source or build tree
# before all other include dirs, so the headers in the sources
# are preferred over the already installed ones
SET(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# -- Log messages about configuration ------------------------------------------------------------------

MESSAGE(STATUS "")
MESSAGE(STATUS "----------------------------------------------------------------------------------")
MESSAGE(STATUS " Libraw ${RAW_LIB_VERSION_STRING} configuration            <http://www.libraw.org>")
MESSAGE(STATUS "")
MESSAGE(STATUS " Libraw will be compiled with OpenMP support .................. " ${OpenMP_FOUND})
MESSAGE(STATUS " Libraw will be compiled with LCMS support .................... " ${LCMS_SUPPORT_CAN_BE_COMPILED})
MESSAGE(STATUS " Libraw will be compiled with example command-line programs ... " ${ENABLE_EXAMPLES})
MESSAGE(STATUS " Libraw will be compiled with Jasper support .................. " ${JASPER_FOUND})
MESSAGE(STATUS " Libraw will be compiled with ZLIB support .................... " ${ZLIB_FOUND})
MESSAGE(STATUS " Libraw will be compiled with JPEG support .................... " ${JPEG_FOUND})
MESSAGE(STATUS " Libraw will be compiled with RawSpeed support ................ " ${RAWSPEED_SUPPORT_CAN_BE_COMPILED})
MESSAGE(STATUS " Libraw will be compiled with debug message from dcraw ........ " ${ENABLE_DCRAW_DEBUG})
MESSAGE(STATUS " Libraw will be compiled with Foveon X3F support .............. " ${ENABLE_X3FTOOLS})
IF(BUILD_SHARED_LIBS)
    MESSAGE(STATUS " Libraw will be compiled as a shared library")
ELSE()
    MESSAGE(STATUS " Libraw will be compiled as a static library")
ENDIF()
MESSAGE(STATUS "----------------------------------------------------------------------------------")
MESSAGE(STATUS "")

# -- Source files list ------------------------------------------------------------------
set(libraw_sources
    src/libraw_c_api.cpp
    src/libraw_datastream.cpp
    src/decoders/canon_600.cpp
    src/decoders/crx.cpp
    src/decoders/pana8.cpp
    src/decoders/decoders_dcraw.cpp
    src/decoders/sonycc.cpp
    src/decompressors/losslessjpeg.cpp
    src/decoders/decoders_libraw_dcrdefs.cpp
    src/decoders/olympus14.cpp
    src/decoders/decoders_libraw.cpp
    src/decoders/dng.cpp
    src/decoders/fp_dng.cpp
    src/decoders/fuji_compressed.cpp
    src/decoders/generic.cpp
    src/decoders/kodak_decoders.cpp
    src/decoders/load_mfbacks.cpp
    src/decoders/smal.cpp
    src/decoders/unpack_thumb.cpp
    src/decoders/unpack.cpp
    src/demosaic/aahd_demosaic.cpp
    src/demosaic/ahd_demosaic.cpp
    src/demosaic/dcb_demosaic.cpp
    src/demosaic/dht_demosaic.cpp
    src/demosaic/misc_demosaic.cpp
    src/demosaic/xtrans_demosaic.cpp
    src/integration/dngsdk_glue.cpp
    src/metadata/adobepano.cpp
    src/metadata/canon.cpp
    src/metadata/ciff.cpp
    src/metadata/cr3_parser.cpp
    src/metadata/epson.cpp
    src/metadata/exif_gps.cpp
    src/metadata/fuji.cpp
    src/metadata/identify_tools.cpp
    src/metadata/identify.cpp
    src/metadata/kodak.cpp
    src/metadata/leica.cpp
    src/metadata/makernotes.cpp
    src/metadata/mediumformat.cpp
    src/metadata/minolta.cpp
    src/metadata/misc_parsers.cpp
    src/metadata/nikon.cpp
    src/metadata/normalize_model.cpp
    src/metadata/olympus.cpp
    src/metadata/hasselblad_model.cpp
    src/metadata/p1.cpp
    src/metadata/pentax.cpp
    src/metadata/samsung.cpp
    src/metadata/sony.cpp
    src/metadata/tiff.cpp
    src/postprocessing/aspect_ratio.cpp
    src/postprocessing/dcraw_process.cpp
    src/postprocessing/mem_image.cpp
    src/postprocessing/postprocessing_aux.cpp
    src/postprocessing/postprocessing_utils_dcrdefs.cpp
    src/postprocessing/postprocessing_utils.cpp
    src/preprocessing/ext_preprocess.cpp
    src/preprocessing/raw2image.cpp
    src/preprocessing/subtract_black.cpp
    src/tables/cameralist.cpp
    src/tables/colorconst.cpp
    src/tables/colordata.cpp
    src/tables/wblists.cpp
    src/utils/curves.cpp
    src/utils/decoder_info.cpp
    src/utils/init_close_utils.cpp
    src/utils/open.cpp
    src/utils/phaseone_processing.cpp
    src/utils/read_utils.cpp
    src/utils/thumb_utils.cpp
    src/utils/utils_dcraw.cpp
    src/utils/utils_libraw.cpp
    src/write/apply_profile.cpp
    src/write/file_write.cpp
    src/write/tiff_writer.cpp
)

if(ENABLE_X3FTOOLS)
    list(APPEND libraw_sources
        src/x3f/x3f_parse_process.cpp
        src/x3f/x3f_utils_patched.cpp
    )
endif()

if(RAWSPEED_SUPPORT_CAN_BE_COMPILED)
    list(APPEND libraw_sources
        src/integration/rawspeed_glue.cpp
    )
endif()

# -- Dedicated libraw target which does not support multi-threading ---------------------------------------
add_library(raw ${libraw_sources})
add_library(libraw::libraw ALIAS raw)
target_compile_definitions(raw PRIVATE LIBRAW_NOTHREADS)
target_compile_options(raw PRIVATE -w)

target_include_directories(raw PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(raw PRIVATE ${MATH_LIBRARY})

if(MINGW)
    target_link_libraries(raw PRIVATE ws2_32)
endif()

if(LCMS_SUPPORT_CAN_BE_COMPILED)
    target_link_libraries(raw PRIVATE ${LCMS2_LIBRARIES})
endif()

if(ZLIB_FOUND)
    target_link_libraries(raw PRIVATE ZLIB::ZLIB)
endif()

if(JPEG_FOUND)
    target_link_libraries(raw PRIVATE JPEG::JPEG)
endif()

if(JASPER_FOUND)
    target_link_libraries(raw PRIVATE ${JASPER_LIBRARIES})
endif()

set_target_properties(raw PROPERTIES
    VERSION ${RAW_LIB_SO_VERSION_STRING}
    SOVERSION ${RAW_LIB_SO_CUR_VERSION}
    OUTPUT_NAME "raw"
    POSITION_INDEPENDENT_CODE ON
)

# -- Dedicated libraw target to support multi-threading ---------------------------------------------
add_library(raw_r ${libraw_sources})
add_library(libraw::libraw_r ALIAS raw_r)
target_compile_options(raw_r PRIVATE -w)

target_include_directories(raw_r PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(raw_r PRIVATE ${MATH_LIBRARY})

if(MINGW)
    target_link_libraries(raw_r PRIVATE ws2_32)
endif()

if(OpenMP_FOUND)
    target_link_libraries(raw_r PRIVATE OpenMP::OpenMP_CXX)
endif()

if(LCMS_SUPPORT_CAN_BE_COMPILED)
    target_link_libraries(raw_r PRIVATE ${LCMS2_LIBRARIES})
endif()

if(ZLIB_FOUND)
    target_link_libraries(raw_r PRIVATE ZLIB::ZLIB)
endif()

if(JPEG_FOUND)
    target_link_libraries(raw_r PRIVATE JPEG::JPEG)
endif()

if(JASPER_FOUND)
    target_link_libraries(raw_r PRIVATE ${JASPER_LIBRARIES})
endif()

if(RAWSPEED_SUPPORT_CAN_BE_COMPILED)
    target_link_libraries(raw_r PRIVATE ${LIBXML2_LIBRARIES} Threads::Threads)
endif()

set_target_properties(raw_r PROPERTIES
    VERSION ${RAW_LIB_SO_VERSION_STRING}
    SOVERSION ${RAW_LIB_SO_CUR_VERSION}
    OUTPUT_NAME "raw_r"
    POSITION_INDEPENDENT_CODE ON
)

# -- Example binaries ------------------------------------------------------------------
if(ENABLE_EXAMPLES)
    add_executable(raw-identify samples/raw-identify.cpp)
    target_link_libraries(raw-identify PRIVATE raw)

    add_executable(unprocessed_raw samples/unprocessed_raw.cpp)
    target_link_libraries(unprocessed_raw PRIVATE raw)

    add_executable(4channels samples/4channels.cpp)
    target_link_libraries(4channels PRIVATE raw)

    add_executable(rawtextdump samples/rawtextdump.cpp)
    target_link_libraries(rawtextdump PRIVATE raw)

    add_executable(simple_dcraw samples/simple_dcraw.cpp)
    target_link_libraries(simple_dcraw PRIVATE raw)

    add_executable(mem_image samples/mem_image_sample.cpp)
    target_link_libraries(mem_image PRIVATE raw)

    add_executable(dcraw_half samples/dcraw_half.c)
    target_link_libraries(dcraw_half PRIVATE raw)

    add_executable(half_mt samples/half_mt.c)
    target_link_libraries(half_mt PRIVATE raw_r)

    add_executable(multirender_test samples/multirender_test.cpp)
    target_link_libraries(multirender_test PRIVATE raw)

    add_executable(postprocessing_benchmark samples/postprocessing_benchmark.cpp)
    target_link_libraries(postprocessing_benchmark PRIVATE raw)

    add_executable(dcraw_emu samples/dcraw_emu.cpp)
    target_link_libraries(dcraw_emu PRIVATE raw)
endif()

# -- Files to install -------------------------------------------------------------------------------------
if (LIBRAW_INSTALL)
    include(GNUInstallDirs)

    # Set variables for pkg-config files
    set(PC_REQUIRES "")
    set(PC_LIBS_PRIVATE "")
    if(LCMS_SUPPORT_CAN_BE_COMPILED)
        set(PC_REQUIRES "${PC_REQUIRES} lcms2")
    endif()
    if(JPEG_FOUND)
        set(PC_LIBS_PRIVATE "${PC_LIBS_PRIVATE} -ljpeg")
    endif()
    if(ZLIB_FOUND)
        set(PC_LIBS_PRIVATE "${PC_LIBS_PRIVATE} -lz")
    endif()

    set(PC_REQUIRES_R ${PC_REQUIRES})
    set(PC_LIBS_PRIVATE_R ${PC_LIBS_PRIVATE})
    if(OpenMP_FOUND)
        set(PC_LIBS_PRIVATE_R "${PC_LIBS_PRIVATE_R} -fopenmp")
    endif()

    # Configure and install pkg-config files
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/data/libraw.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libraw.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libraw.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/data/libraw_r.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libraw_r.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libraw_r.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

    # Install headers
    install(FILES
        libraw/libraw.h
        libraw/libraw_alloc.h
        libraw/libraw_const.h
        libraw/libraw_datastream.h
        libraw/libraw_internal.h
        libraw/libraw_types.h
        libraw/libraw_version.h
        ${CMAKE_CURRENT_BINARY_DIR}/libraw_config.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libraw
        COMPONENT Devel
    )

    # Install libraries
    install(TARGETS raw raw_r
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    # Install documentation
    install(FILES
        COPYRIGHT
        LICENSE.CDDL
        LICENSE.LGPL
        Changelog.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/libraw
        COMPONENT main
    )
endif()

# -- Uninstall target ------------------------------------------------------------------
if(LIBRAW_UNINSTALL_TARGET)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/Uninstall.cmake ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake COPYONLY)
    add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")
endif()
