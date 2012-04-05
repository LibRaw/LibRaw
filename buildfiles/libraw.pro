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
macx: CONFIG+= static x86 x86_x64
macx: QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
DEFINES+=LIBRAW_BUILDLIB

SOURCES=../internal/dcraw_common.cpp \
	../internal/dcraw_fileio.cpp \
	../internal/demosaic_packs.cpp \
	../src/libraw_cxx.cpp \
	../src/libraw_datastream.cpp \
	../src/libraw_c_api.cpp

win32:CONFIG(debug,debug|release) {
	OUTD=debug-$$QMAKE_TARGET.arch
} else {
	OUTD=release-$$QMAKE_TARGET.arch
}

win32: {
	OBJECTS_DIR = $$OUTD/
	MOC_DIR = $$OUTD/
	RCC_DIR = $$OUTD/
	UI_DIR = $$OUTD/
	DESTDIR = $$OUTD/
}
