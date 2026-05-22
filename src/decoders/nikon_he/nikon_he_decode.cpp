/* -*- C++ -*-
 * File: nikon_he_decode.cpp
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE image-level decoder implementation

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE image-level decoder — implementation.
//
// See nikon_he_decode.h for the public API.

#include "nikon_he_decode.h"
#include "nikon_he_predecessor.h"
#include "nikon_he_precinct_decode.h"
#include "nikon_he_predict_lut.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace nikon_he {

// Read a 24-bit big-endian value from unaligned bytes.
static uint32_t read_be24(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 16)
         | (static_cast<uint32_t>(p[1]) << 8)
         | static_cast<uint32_t>(p[2]);
}

HeDecodeResult decode_nikon_he_image(
    const uint8_t* precinct_data,
    size_t precinct_stream_size,
    int image_width,
    int image_height,
    const int32_t* lut,
    uint16_t* out_bayer) {

    HeDecodeResult result = {false, 0, 0};

    if (!precinct_data || precinct_stream_size < 68 || image_width <= 0 || image_height <= 0) {
        return result;
    }

    // 1. Compute sub-band layout for half-width.
    int half_pass_W = image_width / 2;
    SubbandConfig config[26];
    compute_subband_layout(half_pass_W, config);
    const LayoutInfo* li = config[0].layout_info;

    // 2. Build prediction LUT.
    const uint8_t* predict_lut = build_prediction_lookup_table();

    // 3. Predecessor state is initialized fresh PER TILE (inside the loop).
    //    Each tile has its own cross-precinct prediction context; the
    //    2-precinct overlap doesn't carry pred state.

    // 4. Walk the precinct stream.
    // The 24-bit field at offset 0..2 is `total_size_minus_12` — the size of
    // the precinct payload AFTER the 12-byte prefix (3 bytes total_size + 1
    // Bp + 1 Br + 7 Dpb). The full precinct occupies `sz + 12` bytes. After
    // every 16th file precinct (index 15, 31, 47, …) there is a 6-byte
    // alignment pad. File precinct count is roughly n_tiles*16 + 2 (the +2
    // is the trailing 2-precinct overlap tail).
    //
    // Tile T uses file precincts T*16 .. T*16+17 — the last 2 precincts of
    // tile T are the first 2 of tile T+1 (2-precinct overlap).
    int n_tiles = (image_height + 63) / 64;
    int n_file_precincts = n_tiles * 16 + 2;  // approximate upper bound
    int stripe_ints = compute_buf_stripe_ints(li);
    int kband = compute_kband(li);

    // Collect FILE-precinct pointers/sizes (stride 16, not 18).
    const uint8_t** prec_ptrs = new const uint8_t*[n_file_precincts]();
    size_t* prec_sizes = new size_t[n_file_precincts]();

    const uint8_t* cursor = precinct_data;
    size_t remaining = precinct_stream_size;
    int n_walked = 0;

    for (int p = 0; p < n_file_precincts; p++) {
        if (remaining < 3) break;
        uint32_t sz = read_be24(cursor);
        // Sentinel: total_size==0 means end-of-stream.
        if (sz == 0) break;
        // The full precinct occupies sz + 12 bytes; bound-check.
        if (sz + 12 > remaining) break;
        prec_ptrs[p] = cursor;
        prec_sizes[p] = sz;
        cursor += sz + 12;
        remaining -= sz + 12;
        n_walked++;

        // 6-byte alignment pad after every 16th file precinct (index 15,31,...)
        if ((p & 0xF) == 15 && remaining >= 6) {
            cursor += 6;
            remaining -= 6;
        }
    }

    // 5. Allocate image-wide tile_coeff_buf and carry buffers.
    int32_t* tile_coeff_buf = new int32_t[static_cast<size_t>(n_tiles) * kStripesPerTile * stripe_ints]();
    int32_t* step1_scratch = new int32_t[static_cast<size_t>(n_tiles) * kStripesPerTile * stripe_ints]();
    int32_t* overflow = new int32_t[2 * stripe_ints]();

    // 6. PASS 1: decode all tiles into tile_coeff_buf. step1/step2 need
    //    cross-tile reads to find next/prev tile's stripes, so all
    //    decode_tile calls must run BEFORE any step1 call.
    int loop_n_tiles = n_tiles;
    for (int t = 0; t < loop_n_tiles; t++) {
        PrecinctPredecessorState pred_state;
        pred_state.init(config);
        bool is_first = (t == 0);
        bool is_last = (t == n_tiles - 1);

        int tile_prec_base = t * 16;
        int prec_count = 0;
        for (int p = 0; p < kPrecinctsPerTile; ++p) {
            int file_idx = tile_prec_base + p;
            if (file_idx >= n_file_precincts || !prec_ptrs[file_idx]) break;
            prec_count++;
        }

        TileDecodeResult tile_result = decode_tile(
            prec_ptrs + tile_prec_base,
            prec_sizes + tile_prec_base,
            image_width, config, pred_state, predict_lut,
            tile_coeff_buf, t, overflow,
            is_first, is_last, prec_count);
        if (!tile_result.success) goto cleanup;
        result.tiles_decoded++;
        result.total_precincts += tile_result.precincts_decoded;
    }

    // 7. PASS 2: step1 over all tiles → step1_scratch.
    for (int t = 0; t < loop_n_tiles; t++) {
        bool is_first = (t == 0);
        bool is_last = (t == n_tiles - 1);
        int w_rows = (is_last && (image_height % 64) != 0)
                     ? (image_height % 64) / 2
                     : kStripesPerTile;
        int w_cols = image_width / 2;

        int32_t* tile_stripes = tile_coeff_buf + static_cast<size_t>(t) * kStripesPerTile * stripe_ints;
        const int32_t* p1 = tile_stripes + kband;     // LL
        const int32_t* p2_orig = tile_stripes;         // LH
        const int32_t* p3 = tile_stripes + 3 * kband;  // HH
        const int32_t* p4 = tile_stripes + 2 * kband;  // HL

        int32_t* step1_L = step1_scratch + static_cast<size_t>(t) * kStripesPerTile * stripe_ints;
        int32_t* step1_H = step1_L + kband;

        step1_merge_4_to_2(p1, p2_orig, p3, p4,
                           w_rows, w_cols,
                           stripe_ints, stripe_ints,
                           step1_L, step1_H,
                           stripe_ints,
                           is_first, is_last);

    }

    // 8. PASS 3: step2 over all tiles → out_bayer.
    for (int t = 0; t < loop_n_tiles; t++) {
        bool is_first = (t == 0);
        bool is_last = (t == n_tiles - 1);
        int w_rows = (is_last && (image_height % 64) != 0)
                     ? (image_height % 64) / 2
                     : kStripesPerTile;
        int w_cols = image_width / 2;

        int32_t* tile_stripes = tile_coeff_buf + static_cast<size_t>(t) * kStripesPerTile * stripe_ints;
        const int32_t* p1 = tile_stripes + kband;
        const int32_t* p4 = tile_stripes + 2 * kband;
        int32_t* step1_L = step1_scratch + static_cast<size_t>(t) * kStripesPerTile * stripe_ints;
        int32_t* step1_H = step1_L + kband;
        int tile_row_start = t * 64;

        step2_bayer_rows(p1, step1_L, step1_H, p4,
                         w_rows, w_cols,
                         stripe_ints, stripe_ints,
                         lut, out_bayer,
                         image_width, tile_row_start,
                         is_first, is_last);
    }

    result.success = true;

cleanup:
    delete[] prec_ptrs;
    delete[] prec_sizes;
    delete[] tile_coeff_buf;
    delete[] step1_scratch;
    delete[] overflow;

    return result;
}

}  // namespace nikon_he
