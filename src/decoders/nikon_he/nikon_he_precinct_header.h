/* -*- C++ -*-
 * File: nikon_he_precinct_header.h
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE precinct header parser interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE precinct header parser.
//
// Each precinct in a Z9 HE image starts with a compact binary header
// that describes the byte layout of the subsequent entropy-coded data.
// The header is read MSB-first (big-endian) from the raw precinct byte
// stream.
//


#ifndef LIBRAW_NIKON_HE_PRECINCT_HEADER_H
#define LIBRAW_NIKON_HE_PRECINCT_HEADER_H

#include <cstddef>
#include <cstdint>

#include "nikon_he_gtli_table.h"

namespace nikon_he {

// Number of line-blocks per precinct pass.
constexpr int kLineBlocksPerPrecinct = 8;

// Number of Dpb (Débit par bloc) entries.
constexpr int kDpbCount = 28;

// Result of parsing a precinct header.
struct PrecinctSizes {
    // Total size of the precinct data in bytes (as stored in the header).
    uint32_t total_size;

    // Precinct bit-depth parameters.
    uint8_t Bp;
    uint8_t Br;

    // Dpb fields: 28 entries, each 2 bits (0..3).
    // Dpb[0..15] = primary, Dpb[16..27] = secondary.
    uint8_t Dpb[kDpbCount];

    // Derived: f20 = ceil(image_width / 256). Number of sig-substream
    // bytes per LB when f20_sign=1; 0 when f20_sign=0.
    int f20;

    // Per-LB substream byte counts.
    uint32_t lb_sig_bytes[kLineBlocksPerPrecinct];   // = f20
    uint32_t lb_data_bytes[kLineBlocksPerPrecinct];  // f28: data substream
    uint32_t lb_gcli_bytes[kLineBlocksPerPrecinct];  // f24: GCLI+significance substreams
    uint32_t lb_sign_bytes[kLineBlocksPerPrecinct];  // f2c: sign substream
    uint8_t  lb_f20_sign[kLineBlocksPerPrecinct];    // f20 sign flag (informational; always 0 in real data)

    // Per-LB offset from the START of precinct_data to the LB's SIG substream
    // (= first byte after its 7-byte mini-header). The data/gcli/sign offsets
    // follow contiguously after.
    uint32_t lb_sig_offset[kLineBlocksPerPrecinct];
};

// Derive f20 from the HALF-PASS width W = image_width / 2.
// f20 = ceil(W / 256). For FF (W=4140) → 17; for DX (W=2704) → 11.
inline int compute_f20(int half_pass_width) {
    return (half_pass_width + 255) / 256;
}

// Parse a precinct header from raw bytes (MSB-first big-endian).
//
// Parameters:
//   data        — pointer to the start of the precinct data
//   data_size   — size of the buffer (must be ≥ 68 bytes)
//   image_width — full image width in pixels (used to derive f20)
//   out         — output PrecinctSizes structure (always written on success)
//
// Returns true on success, false if the buffer is too short.
//
bool parse_precinct_header(const uint8_t* data,
                           size_t data_size,
                           int image_width,
                           PrecinctSizes& out);

}  // namespace nikon_he

#endif  // LIBRAW_NIKON_HE_PRECINCT_HEADER_H
