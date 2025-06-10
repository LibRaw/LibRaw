prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: libraw
Description: Raw image decoder library (non-thread-safe)
URL: http://www.libraw.org
Version: @RAW_LIB_VERSION_STRING@
Requires: @PC_REQUIRES@
Libs: -L${libdir} -lraw
Libs.private: @PC_LIBS_PRIVATE@
Cflags: -I${includedir}/libraw -I${includedir}
