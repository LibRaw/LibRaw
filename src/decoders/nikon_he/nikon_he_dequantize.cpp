/* -*- C++ -*-
 * File: nikon_he_dequantize.cpp
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE coefficient dequantization implementation

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE dequantization — implementation.
//
// See nikon_he_dequantize.h for the public API.

#include "nikon_he_dequantize.h"
#include <cstdlib>

namespace nikon_he {

int32_t dequantize_coefficient(int32_t coefficient,
                               int gcli,
                               int gtli) {
    // The "encoded_gtli" and "target_gtli" in the formula are BOTH the
    // sub-band's own gtli (from the (Bp, Br) GTLI table). There is no
    // separate image-wide encoded_gtli.
    if (coefficient == 0) {
        return 0;
    }
    if (gcli <= gtli) {
        return 0;  // Group was insignificant.
    }
    int bit_plane_count = gcli - gtli;
    if (bit_plane_count < 1 || bit_plane_count > 15) {
        return 0;
    }
    uint32_t magnitude = static_cast<uint32_t>(std::abs(coefficient)) >> gtli;
    uint64_t product = static_cast<uint64_t>(magnitude)
                     * static_cast<uint64_t>(kMidpointScaleTable[bit_plane_count - 1]);
    int32_t shifted = static_cast<int32_t>(product >> (16 - gtli));
    int32_t result  = shifted << kW4Shift;
    return (coefficient < 0) ? -result : result;
}

int32_t dequantize_ll_coefficient(int32_t coefficient,
                                  int gcli,
                                  int gtli) {
    if (coefficient == 0 || gcli <= gtli) {
        return 0;
    }
    uint32_t magnitude = static_cast<uint32_t>(std::abs(coefficient)) >> gtli;
    int32_t factor = (1 << kW4Shift) + 1;          // = 17
    int32_t result = static_cast<int32_t>(magnitude) * factor << kW4Shift;
    return (coefficient < 0) ? -result : result;
}

void dequantize_coefficient_array(
    int32_t* coefficients,
    int coefficient_count,
    const uint8_t* gcli_values,
    int num_groups,
    int target_gtli,
    bool is_ll_band) {

    // ALL sub-bands (including LL bands sb 12 and sb 23) use the same
    // uniform dequant. The is_ll_band parameter is ignored — kept for
    // API compatibility.
    (void)is_ll_band;
    for (int g = 0; g < num_groups; ++g) {
        int gcli = static_cast<int>(gcli_values[g]);
        for (int c = 0; c < 4; ++c) {
            int idx = g * 4 + c;
            if (idx >= coefficient_count) break;
            coefficients[idx] = dequantize_coefficient(coefficients[idx], gcli, target_gtli);
        }
    }
}

}  // namespace nikon_he
