/* -*- C++ -*-
 * File: libraw_cxx.cpp
 * Copyright 2008-2020 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8 , 2008
 *
 * This file is provided for compatibility w/ old build scripts/tools:
 * It includes multiple separate files that should be built separately
 * if new build tools are used

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../internal/libraw_cxx_defs.h"

#include "tables/cameralist.cpp"
#include "decoders/fuji_compressed.cpp"
#include "decoders/crx.cpp"
#include "decoders/fp_dng.cpp"
#include "decoders/decoders_libraw.cpp"
#include "decoders/unpack.cpp"
#include "decoders/unpack_thumb.cpp"

#include "integration/dngsdk_glue.cpp"
#include "integration/rawspeed_glue.cpp"

#include "tables/colorconst.cpp"
#include "utils/utils_libraw.cpp"
#include "utils/init_close_utils.cpp"
#include "utils/decoder_info.cpp"
#include "utils/open.cpp"
#include "utils/phaseone_processing.cpp"
#include "utils/thumb_utils.cpp"

#include "write/tiff_writer.cpp"
#include "preprocessing/subtract_black.cpp"
#include "preprocessing/raw2image.cpp"
#include "postprocessing/postprocessing_utils.cpp"
#include "postprocessing/dcraw_process.cpp"
#include "postprocessing/mem_image.cpp"

/* DS conflicts with a define in /usr/include/sys/regset.h on Solaris */
#if defined __sun && defined DS
#undef DS
#endif
#undef ID /* used in x3f utils */
#include "x3f/x3f_utils_patched.cpp"
#include "x3f/x3f_parse_process.cpp"
