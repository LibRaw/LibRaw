/* -*- C++ -*-
 * File: nikon_he_tile.h
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE per-tile decode orchestration interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE tile orchestrator.
//
// A tile consists of 18 precincts × 2 passes (A/B) of horizontal IDWT
// and vertical IDWT processing. The tile produces 32 stripes of
// reconstructed coefficients into tile_coeff_buf, with 2 rows of overflow
// carried to the next tile.
//
// Processing order per precinct:
//   1. Entropy decode (26 sub-bands) → bufA / bufB
//   2. Horizontal IDWT pass A (LBs 0-3) + vertical IDWT pass A
//   3. Horizontal IDWT pass B (LBs 4-7) + vertical IDWT pass B
//
// Buffer model:
//   tile_coeff_buf[tile * 32 * bufStripeInts .. (tile+1) * 32 * bufStripeInts]
//     = tile output (32 stripes of reconstructed coefficients)
//   overflow_carry[2 * bufStripeInts] = cross-tile carry
//


#ifndef LIBRAW_NIKON_HE_TILE_H
#define LIBRAW_NIKON_HE_TILE_H

#include "nikon_he_subband_config.h"
#include "nikon_he_predecessor.h"
#include "nikon_he_idwt_horizontal.h"
#include "nikon_he_idwt_vertical.h"
#include <cstddef>
#include <cstdint>

namespace nikon_he {

// Number of precincts per tile.
constexpr int kPrecinctsPerTile = 18;

// Number of ver_lift loops per tile (18 precincts × 2 passes).
constexpr int kVerLiftLoopsPerTile = 36;

// Number of output stripes per tile.
constexpr int kStripesPerTile = 32;

// Compute the stripe stride in int32_t units.
// passA_stride = 3 * lift_LB + memcpy_LB
// bufStripeInts = 4 * passA_stride
inline int compute_buf_stripe_ints(const LayoutInfo* li) {
    int passA_stride = 3 * li->lift_LB + li->memcpy_LB;
    return 4 * passA_stride;
}

// Compute per-band stride (= lift_LB * 4).
inline int compute_kband(const LayoutInfo* li) {
    return li->lift_LB * 4;
}

// Result of decoding one tile.
struct TileDecodeResult {
    int precincts_decoded;  // should be 18 for a full tile
    bool success;
};

// Decode one tile.
//
// Parameters:
//   precinct_data[18] — array of (pointer, size) for each precinct's raw data
//   image_width       — full image width in pixels
//   config[26]        — sub-band layout
//   pred_state        — predecessor state (shared across tiles)
//   predict_lut       — prediction LUT
//   tile_coeff_buf          — output buffer (image-wide, this tile's 32 stripes written)
//   tile_index        — 0-based tile index
//   overflow_carry    — 2 * bufStripeInts int32_t for cross-tile carry (in/out)
//   is_first_tile     — true for tile 0 (path A init)
//   is_last_tile      — true for final tile (may have < 18 precincts)
//   precinct_count    — number of precincts in this tile (≤ 18)
//
TileDecodeResult decode_tile(
    const uint8_t* const* precinct_data,
    const size_t* precinct_sizes,
    int image_width,
    const SubbandConfig config[26],
    PrecinctPredecessorState& pred_state,
    const uint8_t* predict_lut,
    int32_t* tile_coeff_buf,
    int tile_index,
    int32_t* overflow_carry,
    bool is_first_tile,
    bool is_last_tile,
    int precinct_count);

}  // namespace nikon_he

#endif  // LIBRAW_NIKON_HE_TILE_H
