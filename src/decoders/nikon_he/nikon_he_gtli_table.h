/* -*- C++ -*-
 * File: nikon_he_gtli_table.h
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE GTLI lookup table interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE GTLI (Greatest Truncated Line Index) lookup table.
//
// Maps a precinct's (Bp, Br) parameters to the target GTLI value for
// each of the 26 sub-bands. The GTLI is the number of bottom bits
// truncated at encode time, which must be reconstructed during
// dequantization.
//
// The table is small — ~20 unique (Bp, Br) combinations observed across
// many NEF files. All rows are content-independent once Bp and Br are
// known.
//


#ifndef LIBRAW_NIKON_HE_GTLI_TABLE_H
#define LIBRAW_NIKON_HE_GTLI_TABLE_H

#include <cstdint>

namespace nikon_he {

// Number of sub-bands per precinct.
constexpr int kSubBandsPerPrecinct = 26;

// Look up the 26-entry GTLI array for the given (Bp, Br) combination.
// Returns a pointer to 26 uint8_t values (each in 0..15), or nullptr if
// the combination is unknown.
const uint8_t* lookup_gtli_table(int Bp, int Br);

// Direct indexed lookup: returns the GTLI for a specific sub-band.
// Returns 0xFF if the (Bp, Br) combination is unknown.
inline uint8_t lookup_gtli_for_sub_band(int Bp, int Br, int sub_band_index) {
    const uint8_t* table = lookup_gtli_table(Bp, Br);
    if (!table) return 0xFF;
    return table[sub_band_index];
}

}  // namespace nikon_he

#endif  // LIBRAW_NIKON_HE_GTLI_TABLE_H
