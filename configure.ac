# Initialization
AC_INIT([LibRaw], [0.12.0-alpha0], [info@libraw.org], [], [http://www.libraw.org])
AM_INIT_AUTOMAKE([foreign no-define])
AM_SILENT_RULES([yes])

# Tools to use
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

# Config files to generate
AC_CONFIG_FILES([
	Makefile
	libraw.pc
	libraw_r.pc
])

# check if we want OpenMP support
AC_ARG_ENABLE([openmp],
	[  --enable-openmp    Enable OpenMP support],
	[case "${enableval}" in
		yes) 	openmp=true ;;
		no)		openmp=false ;;
		*)		AC_MSG_ERROR([bad value ${enableval} for --enable-openmp]) ;;
	esac],[openmp=true])
if test x$openmp = xtrue ; then
	AX_OPENMP([
		CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
		CFLAGS="$CFLAGS $OPENMP_CFLAGS"
		AC_SUBST([PC_OPENMP],[" $OPENMP_CFLAGS"])		
	],[
		AC_MSG_WARN([OpenMP support cannot be enabled because your system doesn't support it.])		
	])
fi

# check if we want LCMS support
AC_ARG_ENABLE([lcms],
	[  --enable-lcms    Enable LCMS support],
	[case "${enableval}" in
		yes)	lcms=true ;;
		no)		lcms=false ;;
		*)		AC_MSG_ERROR([bad value ${enableval} for --enable-lcms]) ;;
	esac],[lcms=true])
if test x$lcms = xtrue; then
	PKG_CHECK_MODULES([LCMS2],[lcms2],[
		CPPFLAGS="$CPPFLAGS $LCMS2_CFLAGS -DUSE_LCMS2"
		LDFLAGS="$LDFLAGS $LCMS2_LIBS"
	],[
	PKG_CHECK_MODULES([LCMS],[lcms],[
		CPPFLAGS="$CPPFLAGS $LCMS_CFLAGS -DUSE_LCMS"
		LDFLAGS="$LDFLAGS $LCMS_LIBS"
	],[
		AC_MSG_WARN([LCMS support cannot be enabled])
	])
	])
fi

# check if we want build examples
AC_ARG_ENABLE([examples],
	[  --enable-examples    Enable building of examples],
	[case "${enableval}" in
		yes) 	examples=true ;;
		no)		examples=false ;;
		*)		AC_MSG_ERROR([bad value ${enableval} for --enable-examples]) ;;
	esac],[examples=true])
AM_CONDITIONAL([EXAMPLES], [test x$examples = xtrue])

AC_OUTPUT

