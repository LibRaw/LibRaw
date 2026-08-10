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

// Keep decoder headers before dcraw_defs.h, which defines width and height as
// macros.
#include "nikon_he/nikon_he_decode.h"
#include "nikon_he/nikon_he_iqx_iqp_lut_data.h"
#include "nikon_he/nikon_he_gtli_table.h"
#include "nikon_he/nikon_he_picture_header.h"

#include <vector>
#include <cstdint>
#include <cstring>

#include "../../internal/dcraw_defs.h"

namespace {

constexpr unsigned kNefCompressionHe = 13;
constexpr unsigned kNefCompressionHeStar = 14;

class ActivePictureHeaderScope {
public:
    explicit ActivePictureHeaderScope(const nikon_he::PictureHeader& ph) {
        nikon_he::set_active_picture_header(&ph);
    }
    ~ActivePictureHeaderScope() {
        nikon_he::set_active_picture_header(nullptr);
    }

    ActivePictureHeaderScope(const ActivePictureHeaderScope&) = delete;
    ActivePictureHeaderScope& operator=(const ActivePictureHeaderScope&) = delete;
};

}  // namespace

void LibRaw::nikon_he_load_raw()
{
    if (dng_version) throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;
    if (!raw_image)  throw LIBRAW_EXCEPTION_ALLOC;

    const int img_w = (int)raw_width;
    const int img_h = (int)raw_height;
    if (img_w <= 0 || img_h <= 0 || (img_w & 1)) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }

    const unsigned nef_compression = imgdata.makernotes.nikon.NEFCompression;
    if (nef_compression != kNefCompressionHe &&
        nef_compression != kNefCompressionHeStar) {
        throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;
    }

    const uint64_t tiff_strip_offset = (uint64_t)data_offset;
    const uint64_t tiff_strip_size   = (uint64_t)data_size;
    if (tiff_strip_size < 64 || tiff_strip_size > (uint64_t)INT32_MAX) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }
    if (tiff_strip_size >
        (uint64_t)imgdata.rawparams.max_raw_memory_mb * 1024ULL * 1024ULL) {
        throw LIBRAW_EXCEPTION_ALLOC;
    }

    std::vector<uint8_t> strip((size_t)tiff_strip_size);
    auto* ds = libraw_internal_data.internal_data.input;
    ds->seek((INT64)tiff_strip_offset, SEEK_SET);
    if (ds->read(strip.data(), 1, strip.size()) != strip.size()) {
        throw LIBRAW_EXCEPTION_IO_EOF;
    }

    nikon_he::PictureHeader ph;
    if (!nikon_he::parse_picture_header(strip.data(), strip.size(), ph) ||
        !nikon_he::is_supported_picture_header(ph, strip.size()) ||
        ph.precinct_offset >= strip.size()) {
        throw LIBRAW_EXCEPTION_UNSUPPORTED_FORMAT;
    }
    if ((int)ph.hdr_width != img_w || (int)ph.hdr_height != img_h) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }

    std::vector<uint16_t> bayer((size_t)img_w * img_h, 0);
    ActivePictureHeaderScope active_header(ph);
    const nikon_he::HeDecodeResult result = nikon_he::decode_nikon_he_image(
        strip.data() + ph.precinct_offset,
        strip.size() - ph.precinct_offset,
        img_w, img_h,
        nikon_he::iqx_iqp_lut(),
        bayer.data());

    if (!result.success) {
        throw LIBRAW_EXCEPTION_DECODE_RAW;
    }

    std::memcpy(raw_image, bayer.data(),
                (size_t)img_w * img_h * sizeof(unsigned short));
    maximum = 16383;
}
