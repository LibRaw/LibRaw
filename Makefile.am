# autoconf macros
ACLOCAL_AMFLAGS = -I m4

# Headers
nobase_include_HEADERS = 	libraw/libraw.h \
							libraw/libraw_alloc.h \
							libraw/libraw_const.h \
							libraw/libraw_datastream.h \
							libraw/libraw_internal.h \
							libraw/libraw_types.h \
							libraw/libraw_version.h

# Docs
doc_DATA =	COPYRIGHT \
			LICENSE.CDDL \
			LICENSE.LGPL \
			LICENSE.LibRaw.pdf \
			Changelog.txt

# pkg-config .pc files
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libraw.pc libraw_r.pc

# Libraries
lib_LIBRARIES = libraw.a libraw_r.a

libraw_a_CPPFLAGS = -DLIBRAW_NOTHREADS
libraw_a_SOURCES = internal/dcraw_common.cpp \
				 internal/dcraw_fileio.cpp \
				 src/libraw_cxx.cpp \
				 src/libraw_c_api.cpp

libraw_r_a_CXXFLAGS = -pthread
libraw_r_a_SOURCES = $(libraw_a_SOURCES)


# Sample binaries
if EXAMPLES
bin_PROGRAMS =	raw-identify \
				unprocessed_raw \
				4channels \
				simple_dcraw \
				mem_image \
				dcraw_half \
				half_mt \
				dcraw_emu
endif

raw_identify_SOURCES = samples/raw-identify.cpp
raw_identify_CPPFLAGS = $(libraw_a_CPPFLAGS)
raw_identify_LDADD = libraw.a  -lm -lraw $(LCMS_LIBS)

unprocessed_raw_SOURCES = samples/unprocessed_raw.cpp
unprocessed_raw_CPPFLAGS = $(libraw_a_CPPFLAGS)
unprocessed_raw_LDADD = libraw.a -lm -lraw $(LCMS_LIBS)

4channels_SOURCES = samples/4channels.cpp
4channels_CPPFLAGS = $(libraw_a_CPPFLAGS)
4channels_LDADD = libraw.a -lm -lraw $(LCMS_LIBS)

simple_dcraw_SOURCES = samples/simple_dcraw.cpp
simple_dcraw_CPPFLAGS = $(libraw_a_CPPFLAGS)
simple_dcraw_LDADD = libraw.a -lm -lraw $(LCMS_LIBS)

mem_image_SOURCES = samples/mem_image.cpp
mem_image_CPPFLAGS = $(libraw_a_CPPFLAGS)
mem_image_LDADD = libraw.a -lm -lraw $(LCMS_LIBS)

dcraw_half_SOURCES = samples/dcraw_half.c
dcraw_half_CPPFLAGS = $(libraw_a_CPPFLAGS)
dcraw_half_LDADD = libraw.a -lm -lraw -lstdc++ $(LCMS_LIBS)

half_mt_SOURCES = samples/half_mt.c
half_mt_CFLAGS = $(libraw_r_a_CXXFLAGS)
half_mt_LDADD = libraw_r.a -lm -lraw_r -lstdc++ $(LCMS_LIBS)

dcraw_emu_SOURCES = samples/dcraw_emu.cpp
dcraw_emu_CPPFLAGS = $(libraw_a_CPPFLAGS)
dcraw_emu_LDADD = libraw.a -lm -lraw $(LCMS_LIBS)
