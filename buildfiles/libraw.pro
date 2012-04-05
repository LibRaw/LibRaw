TEMPLATE=lib
TARGET=libraw
INCLUDEPATH+=../
HEADERS=../libraw/libraw.h \
	 ../libraw/libraw_alloc.h \
	../libraw/libraw_const.h \
	../libraw/libraw_datastream.h \
	../libraw/libraw_types.h \
	../libraw/libraw_internal.h \
	../libraw/libraw_version.h \
	../internal/defines.h \
	../internal/var_defines.h \
	../internal/libraw_internal_funcs.h

CONFIG-=qt
CONFIG+=warn_off
macx: CONFIG+= static x86 x86_64
macx: QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
DEFINES+=LIBRAW_BUILDLIB

SOURCES=../internal/dcraw_common.cpp \
	../internal/dcraw_fileio.cpp \
	../internal/demosaic_packs.cpp \
	../src/libraw_cxx.cpp \
	../src/libraw_datastream.cpp \
	../src/libraw_c_api.cpp

win32:CONFIG(debug,debug|release) {
	win32:contains(QMAKE_HOST.arch, x86_64):{
		OBJECTS_DIR = debug-x64/
		MOC_DIR = debug-x64/
		RCC_DIR = debug-x64/
		UI_DIR = debug-x64/
		DESTDIR = debug-x64/
        }
}

win32:CONFIG(release,debug|release) {
	LIBS+=-L../LibRaw/buildfiles/release/
	win32:contains(QMAKE_HOST.arch, x86_64):{
		OBJECTS_DIR = release-x64/
		MOC_DIR = release-x64/
		RCC_DIR = release-x64/
		UI_DIR = release-x64/
		DESTDIR = release-x64/
        }
}
