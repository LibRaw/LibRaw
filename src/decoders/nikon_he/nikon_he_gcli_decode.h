/* -*- C++ -*-
 * File: nikon_he_gcli_decode.h
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE GCLI (group code-length info) decode interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE GCLI (Greatest Coded Line Index) decode.
//
// Decodes per-group GCLI values for one sub-band from the significance
// and GCLI substreams. Two modes are used in Z9 HE:
//
//   Mode 0x71 — zero-prediction (prev=0). Used for all sub-bands except
//               the LL bands (sb 12 and sb 23). Simple, no cross-band state.
//   Mode 0x73 — vertical prediction via lookup table. Used for LL bands
//               only. Uses previous-band GCLI values as context.
//
// The significance substream provides ONE bit per block of 8 GCLI groups:
//   sig=1 → all 8 GCLIs equal the baseline prediction (cheap, smooth areas)
//   sig=0 → each GCLI has a unary-encoded delta in the GCLI substream
//


#ifndef LIBRAW_NIKON_HE_GCLI_DECODE_H
#define LIBRAW_NIKON_HE_GCLI_DECODE_H

#include "nikon_he_bit_reader.h"
#include "nikon_he_predict_lut.h"

#include <cstddef>
#include <cstdint>

namespace nikon_he {

// Decode GCLI values for one sub-band.
//
// Parameters:
//   sig_reader    — bit reader positioned at the sig substream for this sub-band
//   gcli_reader   — bit reader positioned at the GCLI substream for this sub-band
//   mode          — 0x71 (zero-pred) or 0x73 (vertical-pred via LUT)
//   num_groups    — number of coefficient groups (ng) in this sub-band
//   gtli          — the sub-band's GTLI (quantization threshold)
//   predict_lut   — the 8192-entry prediction LUT (from build_prediction_lookup_table)
//   previous_gcli — for mode 0x73: previous band's GCLI values (length ≥ num_groups)
//                    for mode 0x71: ignored, pass nullptr
//   gcli_output   — output buffer of at least num_groups uint8_t entries
//
void decode_gcli_values(
    BitReader& sig_reader,
    BitReader& gcli_reader,
    int mode,
    int num_groups,
    int gtli,
    const uint8_t* predict_lut,
    const uint8_t* previous_gcli,
    uint8_t* gcli_output);

}  // namespace nikon_he

#endif  // LIBRAW_NIKON_HE_GCLI_DECODE_H
