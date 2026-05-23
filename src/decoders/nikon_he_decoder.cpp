/* -*- C++ -*-
 * File: nikon_he_decoder.cpp
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon Z9 High-Efficiency RAW decoder LibRaw integration glue

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon High Efficiency RAW decoder — LibRaw integration glue.
//
// This translation unit:
//   1. Forward-declares the decoder's public entry from
//      nikon_he/nikon_he_decode.h (the rest of nikon_he/*.cpp are
//      compiled separately via Makefile.am and linked in).
//   2. Implements LibRaw::nikon_he_load_raw() — reads the precinct
//      strip from the datastream, dispatches to the decoder, copies
//      the decoded bayer into raw_image.
//
// The decoder takes a contiguous precinct-stream buffer, so we read
// the entire raw strip upfront. The full strip is small enough (~30 MB
// for Z9 FF) to keep in memory.

// Decoder headers FIRST — they live in the nikon_he namespace and pull
// in only <cstdint>, <cstddef>, etc. Including before dcraw_defs.h
// avoids any collision with LibRaw's macro-defined names
// (width/height/etc).
#include "nikon_he/nikon_he_decode.h"
#include "nikon_he/nikon_he_iqx_iqp_lut_data.h"

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Now bring in LibRaw's macros / definitions.
#include "../../internal/dcraw_defs.h"

void LibRaw::nikon_he_load_raw()
{
    if (dng_version) throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;
    if (!raw_image)  throw LIBRAW_EXCEPTION_ALLOC;

    const int img_w = (int)raw_width;
    const int img_h = (int)raw_height;
    if (img_w <= 0 || img_h <= 0 || (img_w & 1)) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }

    // TIFF raw-IFD StripOffsets / StripByteCounts — populated by
    // identify(). `data_offset` and `data_size` are macros from
    // var_defines.h expanding to the unpacker_data fields.
    const uint64_t tiff_strip_offset = (uint64_t)data_offset;
    const uint64_t tiff_strip_size   = (uint64_t)data_size;

    // The Z9 HE precinct stream starts at strip_offset + 0x9B (the
    // 0x9B-byte prefix is a fixed header we don't currently parse).
    constexpr uint64_t kPrecinctOffsetFromStrip = 0x9b;
    if (tiff_strip_size <= kPrecinctOffsetFromStrip) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }
    const uint64_t precinct_off  = tiff_strip_offset + kPrecinctOffsetFromStrip;
    const size_t   precinct_size = (size_t)(tiff_strip_size - kPrecinctOffsetFromStrip);

    // Slurp the precinct stream from the datastream into memory.
    std::vector<uint8_t> precinct_bytes(precinct_size);
    auto* ds = libraw_internal_data.internal_data.input;
    ds->seek((INT64)precinct_off, SEEK_SET);
    if (ds->read(precinct_bytes.data(), 1, precinct_size) != precinct_size) {
        throw LIBRAW_EXCEPTION_IO_EOF;
    }

    if (precinct_size < 12) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }

    // Decode into a scratch bayer buffer, then copy out.
    std::vector<uint16_t> bayer((size_t)img_w * img_h, 0);
    auto result = nikon_he::decode_nikon_he_image(
        precinct_bytes.data(),
        precinct_size,
        img_w, img_h,
        nikon_he::iqx_iqp_lut(),
        bayer.data());

    if (!result.success) {
        std::fprintf(stderr,
            "[nikon_he_load_raw] decode failed (image %dx%d).\n",
            img_w, img_h);
        std::memset(raw_image, 0,
                    (size_t)img_w * img_h * sizeof(unsigned short));
        maximum = 16383;
        return;
    }

    std::memcpy(raw_image, bayer.data(),
                (size_t)img_w * img_h * sizeof(unsigned short));
    maximum = 16383;
}
