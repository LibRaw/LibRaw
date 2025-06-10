prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@/libraw

Name: libraw_r
Description: Raw image decoder library (thread-safe)
URL: http://www.libraw.org
Version: @RAW_LIB_VERSION_STRING@
Requires: @PC_REQUIRES_R@
Libs: -L${libdir} -lraw_r
Libs.private: @PC_LIBS_PRIVATE_R@
Cflags: -I${includedir}/libraw -I${includedir}
