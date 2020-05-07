/*
  Copyright 2008-2020 LibRaw LLC (info@libraw.org)

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

#include "dcraw_defs.h"

#include "../src/utils/read_utils.cpp"
#include "../src/utils/curves.cpp"
#include "../src/utils/utils_dcraw.cpp"

#include "../src/tables/colordata.cpp"

#include "../src/decoders/canon_600.cpp"
#include "../src/decoders/decoders_dcraw.cpp"
#include "../src/decoders/decoders_libraw_dcrdefs.cpp"
#include "../src/decoders/generic.cpp"
#include "../src/decoders/kodak_decoders.cpp"
#include "../src/decoders/dng.cpp"
#include "../src/decoders/smal.cpp"
#include "../src/decoders/load_mfbacks.cpp"

#include "../src/metadata/sony.cpp"
#include "../src/metadata/nikon.cpp"
#include "../src/metadata/samsung.cpp"
#include "../src/metadata/cr3_parser.cpp"
#include "../src/metadata/canon.cpp"
#include "../src/metadata/epson.cpp"
#include "../src/metadata/olympus.cpp"
#include "../src/metadata/leica.cpp"
#include "../src/metadata/fuji.cpp"
#include "../src/metadata/adobepano.cpp"
#include "../src/metadata/pentax.cpp"
#include "../src/metadata/p1.cpp"
#include "../src/metadata/makernotes.cpp"
#include "../src/metadata/exif_gps.cpp"
#include "../src/metadata/kodak.cpp"
#include "../src/metadata/tiff.cpp"
#include "../src/metadata/ciff.cpp"
#include "../src/metadata/mediumformat.cpp"
#include "../src/metadata/minolta.cpp"
#include "../src/metadata/identify_tools.cpp"
#include "../src/metadata/normalize_model.cpp"
#include "../src/metadata/identify.cpp"
#include "../src/metadata/hasselblad_model.cpp"
#include "../src/metadata/misc_parsers.cpp"
#include "../src/tables/wblists.cpp"
#include "../src/postprocessing/postprocessing_aux.cpp"
#include "../src/postprocessing/postprocessing_utils_dcrdefs.cpp"
#include "../src/postprocessing/aspect_ratio.cpp"

#include "../src/demosaic/misc_demosaic.cpp"
#include "../src/demosaic/xtrans_demosaic.cpp"
#include "../src/demosaic/ahd_demosaic.cpp"
#include "../src/write/file_write.cpp"
