/* -*- C++ -*-
 * File: nikon_he_predecessor.cpp
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE cross-precinct GCLI predecessor state

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE precinct predecessor state — implementation.
//
// See nikon_he_predecessor.h for the public API.

#include "nikon_he_predecessor.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace nikon_he {

void PrecinctPredecessorState::init(const SubbandConfig config[26]) {
    // Store ng for each sub-band.
    for (int i = 0; i < 26; i++) {
        ng_[i] = config[i].ng;
    }

    // Allocate per-sub-band GCLI storage.
    for (int i = 0; i < 26; i++) {
        int ng = config[i].ng;
        if (ng > 0) {
            gcli_store_[i] = new uint8_t[ng];
            std::memset(gcli_store_[i], 0, ng);
        } else {
            gcli_store_[i] = nullptr;
        }
    }

    // Allocate rotation buffers for sb 12 and sb 23.
    int ng12 = config[12].ng;
    int ng23 = config[23].ng;
    rotation_buf_size_ = std::max(ng12, ng23);
    if (rotation_buf_size_ > 0) {
        rotation_buf_a_ = new uint8_t[rotation_buf_size_];
        rotation_buf_b_ = new uint8_t[rotation_buf_size_];
        std::memset(rotation_buf_a_, 0, rotation_buf_size_);
        std::memset(rotation_buf_b_, 0, rotation_buf_size_);
    }

    // gcli_store_[12] and gcli_store_[23] point to the rotation buffers:
    // Initially:
    //   rotation_buf_a_ = sb 23 from prev precinct → sb 12's "previous"
    //   rotation_buf_b_ = sb 12 from this precinct  → sb 23's "previous"
    //   gcli_store_[12] is the saved sb 12 GCLIs (→ rotation_buf_b_)
    //   gcli_store_[23] is the saved sb 23 GCLIs (→ rotation_buf_a_)
    //
    // But gcli_store_ already has its own allocations from above.
    // We need to redirect sb 12 and sb 23 to the rotation buffers.

    // Free the redundant per-sb allocations for 12 and 23
    delete[] gcli_store_[12];
    delete[] gcli_store_[23];

    // Redirect: sb 12 saves into rotation_buf_b_ (goes to sb 23 as prev)
    //           sb 23 saves into rotation_buf_a_ (goes to next precinct's sb 12)
    gcli_store_[12] = rotation_buf_b_;
    gcli_store_[23] = rotation_buf_a_;

    std::memset(fully_insig_, 0, sizeof(fully_insig_));
    precinct_index_ = 0;
}

void PrecinctPredecessorState::destroy() {
    for (int i = 0; i < 26; i++) {
        // Don't double-free rotation buffers (sb 12 and 23 share them)
        if (i == 12 || i == 23) continue;
        delete[] gcli_store_[i];
        gcli_store_[i] = nullptr;
    }
    delete[] rotation_buf_a_;
    rotation_buf_a_ = nullptr;
    delete[] rotation_buf_b_;
    rotation_buf_b_ = nullptr;
    rotation_buf_size_ = 0;
    gcli_store_[12] = nullptr;
    gcli_store_[23] = nullptr;
}

const uint8_t* PrecinctPredecessorState::get_previous_gcli(int sb_index) const {
    if (sb_index < 0 || sb_index >= 26) return nullptr;

    if (sb_index == 12) {
        // sb 12's prev = sb 23 from previous precinct → rotation_buf_a_
        return rotation_buf_a_;
    } else if (sb_index == 23) {
        // sb 23's prev = sb 12 from current precinct → rotation_buf_b_
        return rotation_buf_b_;
    } else {
        // All other sub-bands: return their own saved GCLIs (which are
        // the "previous" for zero-prediction mode 0x71). Mode 0x71 uses
        // zero-prediction, but the previous GCLIs are still returned for
        // potential reference.
        return gcli_store_[sb_index];
    }
}

void PrecinctPredecessorState::save_gcli(int sb_index, const uint8_t* gcli_values) {
    if (sb_index < 0 || sb_index >= 26) return;
    if (!gcli_store_[sb_index] || !gcli_values) return;

    int n = ng_[sb_index];
    if (n > 0) {
        std::memcpy(gcli_store_[sb_index], gcli_values, n);
    }
}

void PrecinctPredecessorState::set_fully_insig(int sb_index, bool flag) {
    if (sb_index >= 0 && sb_index < 26) {
        fully_insig_[sb_index] = flag;
    }
}

bool PrecinctPredecessorState::is_fully_insig(int sb_index) const {
    if (sb_index >= 0 && sb_index < 26) {
        return fully_insig_[sb_index];
    }
    return false;
}

void PrecinctPredecessorState::advance_precinct() {
    precinct_index_++;

    // The rotation works as follows:
    //   rotation_buf_a_ holds sb 23 from previous precinct (→ sb 12 prev)
    //   rotation_buf_b_ holds sb 12 from current precinct  (→ sb 23 prev)
    //
    // After a precinct is fully decoded:
    //   rotation_buf_a_ has sb 23's GCLIs → stays for next precinct's sb 12 prev
    //   rotation_buf_b_ has sb 12's GCLIs → consumed by sb 23, zero for next
    //
    // No swap — just zero the consumed sb 12 buffer for the next precinct.

    if (rotation_buf_b_) {
        std::memset(rotation_buf_b_, 0, rotation_buf_size_);
    }

    // gcli_store_[12] points to rotation_buf_b_ → already zeroed above
    // gcli_store_[23] points to rotation_buf_a_ → still has last precinct's sb 23
}

void PrecinctPredecessorState::reset_gcli_state(const SubbandConfig config[26]) {
    for (int i = 0; i < 26; i++) {
        if (gcli_store_[i] && ng_[i] > 0) {
            std::memset(gcli_store_[i], 0, ng_[i]);
        }
        fully_insig_[i] = false;
    }
    // Also zero the non-store rotation buffer
    if (rotation_buf_a_ && rotation_buf_a_ != gcli_store_[12]
        && rotation_buf_a_ != gcli_store_[23]) {
        std::memset(rotation_buf_a_, 0, rotation_buf_size_);
    }
    if (rotation_buf_b_ && rotation_buf_b_ != gcli_store_[12]
        && rotation_buf_b_ != gcli_store_[23]) {
        std::memset(rotation_buf_b_, 0, rotation_buf_size_);
    }
}

}  // namespace nikon_he
