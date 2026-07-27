/* -*- C++ -*-
 * File: nikon_he_picture_header.cpp
 *
   Nikon HE / HE* JPEG XS picture-header parser

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "nikon_he_picture_header.h"

#include <cstring>

namespace nikon_he {

namespace {

const uint16_t kSOC = 0xff10;
const uint16_t kEOC = 0xff11;
const uint16_t kPIH = 0xff12;
const uint16_t kCDT = 0xff13;
const uint16_t kWGT = 0xff14;
const uint16_t kNLT = 0xff16;
const uint16_t kSLH = 0xff20;
const uint16_t kCAP = 0xff50;

inline uint16_t be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
inline uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

}  // namespace

bool parse_picture_header(const uint8_t* strip, size_t strip_size,
                          PictureHeader& out) {
    std::memset(&out, 0, sizeof(out));
    if (!strip || strip_size < 8)
        return false;
    if (be16(strip) != kSOC)
        return false;

    size_t i = 2;
    bool saw_cap = false, saw_pih = false, saw_cdt = false, saw_wgt = false;

    while (i + 2 <= strip_size) {
        const uint16_t marker = be16(strip + i);
        if ((marker & 0xff00) != 0xff00)
            return false;

        if (marker == kEOC)
            return false;
        if (i + 4 > strip_size)
            return false;
        const uint16_t lseg = be16(strip + i + 2);
        if (lseg < 2 || i + 2 + (size_t)lseg > strip_size)
            return false;
        const uint8_t* body = strip + i + 4;
        const size_t body_len = (size_t)lseg - 2;

        switch (marker) {
        case kCAP:
            if (saw_cap || body_len < 16 ||
                std::memcmp(body, "CONTACT_INTOPIX_", 16) != 0)
                return false;
            saw_cap = true;
            break;
        case kPIH: {
            if (saw_pih || body_len < 20)
                return false;
            out.Lcod                    = be32(body + 0);
            out.hdr_width               = be16(body + 8);
            out.hdr_height              = be16(body + 10);
            out.precinct_width          = be16(body + 12);
            out.Hsl                     = be16(body + 14);
            out.comps_num               = body[16];
            out.coeff_group_size        = body[17];
            out.significance_group_size = body[18];
            out.Bw                      = body[19];
            saw_pih = true;
            break;
        }
        case kCDT:
            if (saw_cdt || body_len == 0)
                return false;
            saw_cdt = true;
            break;
        case kWGT: {
            if (saw_wgt || (body_len & 1u))
                return false;
            int n = (int)(body_len / 2);
            if (n <= 0 || n > kMaxWgtBands)
                return false;
            for (int b = 0; b < n; b++) {
                out.gain[b]     = body[2 * b];
                out.priority[b] = body[2 * b + 1];
            }
            out.nbands = n;
            saw_wgt = true;
            break;
        }
        case kNLT:
            out.has_nlt = true;
            break;
        default:
            break;
        }

        i += 2 + (size_t)lseg;

        if (marker == kSLH) {
            out.precinct_offset = i;
            out.valid = saw_cap && saw_pih && saw_cdt && saw_wgt;
            return out.valid;
        }
    }
    return false;
}

bool is_supported_picture_header(const PictureHeader& ph, size_t strip_size) {
    return ph.valid && ph.Lcod == strip_size &&
           ph.hdr_width > 0 && (ph.hdr_width & 1u) == 0 &&
           ph.hdr_height > 0 &&
           ph.precinct_width == 0 && ph.Hsl == 16 &&
           ph.comps_num == 4 && ph.coeff_group_size == 4 &&
           ph.significance_group_size == 8 && ph.Bw == 18 &&
           ph.nbands == kNikonHeWgtBands && !ph.has_nlt;
}

}  // namespace nikon_he
