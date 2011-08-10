/* -*- C++ -*-
 * File: libraw_version.h
 * Copyright 2008-2010 LibRaw LLC (info@libraw.org)
 * Created: Mon Sept  8, 2008 
 *
 * LibRaw C++ interface
 *

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of three licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
(See the file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
(See the file LICENSE.CDDL provided in LibRaw distribution archive for details).

3. LibRaw Software License 27032010
  (See the file LICENSE.LibRaw.pdf provided in LibRaw distribution archive for details).

 */

#ifndef __VERSION_H
#define __VERSION_H

#define LIBRAW_MAJOR_VERSION  0
#define LIBRAW_MINOR_VERSION  14
#define LIBRAW_PATCH_VERSION  0
#define LIBRAW_VERSION_TAIL   Alpha4

#define LIBRAW_SHLIB_CURRENT  	4
#define LIBRAW_SHLIB_REVISION 	0
#define LIBRAW_SHLIB_AGE     	0

#define _LIBRAW_VERSION_MAKE(a,b,c,d) #a"."#b"."#c"-"#d
#define LIBRAW_VERSION_MAKE(a,b,c,d) _LIBRAW_VERSION_MAKE(a,b,c,d)

#define LIBRAW_VERSION_STR LIBRAW_VERSION_MAKE(LIBRAW_MAJOR_VERSION,LIBRAW_MINOR_VERSION,LIBRAW_PATCH_VERSION,LIBRAW_VERSION_TAIL)

#define LIBRAW_MAKE_VERSION(major,minor,patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define LIBRAW_VERSION \
    LIBRAW_MAKE_VERSION(LIBRAW_MAJOR_VERSION,LIBRAW_MINOR_VERSION,LIBRAW_PATCH_VERSION)

#define LIBRAW_CHECK_VERSION(major,minor,patch) \
    ( LIBRAW_VERSION >= LIBRAW_MAKE_VERSION(major,minor,patch) )


#endif
