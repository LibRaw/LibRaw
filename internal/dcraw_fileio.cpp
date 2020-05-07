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

   This file is generated from Dave Coffin's dcraw.c
   dcraw.c -- Dave Coffin's raw photo decoder
   Copyright 1997-2010 by Dave Coffin, dcoffin a cybercom o net

   Look into dcraw homepage (probably http://cybercom.net/~dcoffin/dcraw/)
   for more information
*/

#include "dcraw_fileio_defs.h"

#include "../src/preprocessing/ext_preprocess.cpp"
#include "../src/write/apply_profile.cpp"
