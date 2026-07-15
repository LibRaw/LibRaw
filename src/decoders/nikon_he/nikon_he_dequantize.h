/* -*- C++ -*-
 * File: nikon_he_dequantize.h
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE coefficient dequantization interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE dequantization.
//
// The encoder truncates the bottom `gtli` bits of every coefficient.
// The decoder reconstructs those bits using a deadzone-midpoint formula,
// scaling each coefficient by a 16-entry constant table indexed by the
// number of bit-planes actually encoded.
//
// LL bands (sub-bands 12 and 23) use a simpler formula: a fixed factor
// instead of the deadzone table.
//
// Z9 HE constants:
//   w4_shift = 4   (image-wide tone shift)
// The "encoded_gtli" and "target_gtli" parameters of the formula are
// BOTH the sub-band's own gtli (from the (Bp, Br) GTLI table).
//


#ifndef LIBRAW_NIKON_HE_DEQUANTIZE_H
#define LIBRAW_NIKON_HE_DEQUANTIZE_H

#include <cstdint>

namespace nikon_he {

// Deadzone-midpoint scaling constants. Indexed by (bit_plane_count - 1),
// i.e. bc[w2 - 1] where w2 = gcli - encoded_gtli.
// bc[15] = 0 is a sentinel (16 bit-planes → unreachable in this format).
constexpr int32_t kMidpointScaleTable[16] = {
    87381, 74898, 69905, 67650, 66576, 66052, 65793, 65664,
    65600, 65568, 65552, 65544, 65540, 65538, 65537, 0,
};

// Z9 HE image-wide tone shift.
constexpr int kW4Shift = 4;

// Dequantize one coefficient (non-LL).
//   coefficient — signed raw coefficient from entropy decode
//   gcli        — this group's GCLI value
//   gtli        — this sub-band's GTLI (from the (Bp, Br) GTLI table)
int32_t dequantize_coefficient(int32_t coefficient,
                               int gcli,
                               int gtli);

// Dequantize one LL-band coefficient (sb 12, sb 23).
//   result = sign(coef) × (abs(coef) >> gtli) × ((1 << w4) + 1) << w4
int32_t dequantize_ll_coefficient(int32_t coefficient,
                                  int gcli,
                                  int gtli);

// Batch dequantize an array of coefficients for one sub-band.
// For LL bands, pass is_ll_band = true.
void dequantize_coefficient_array(
    int32_t* coefficients,
    int coefficient_count,
    const uint8_t* gcli_values,
    int num_groups,
    int target_gtli,
    bool is_ll_band);

}  // namespace nikon_he

#endif  // LIBRAW_NIKON_HE_DEQUANTIZE_H
