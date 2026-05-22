/* -*- C++ -*-
 * File: nikon_he_predecessor.h
 * Copyright (C) 2026 Dmitri Sotnikov
 *
   Nikon HE cross-precinct GCLI predecessor state interface

LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

// Nikon HE precinct predecessor state.
//
// Manages the inter-precinct GCLI prediction state for the 26 sub-bands.
// Two sub-bands use cross-band prediction (mode 0x73):
//
//   sb 12 (LL, LB 4): prev_band = sb 23 from the PREVIOUS precinct
//   sb 23 (LL, LB 7): prev_band = sb 12 from the CURRENT precinct
//
// The rotation buffer swaps at each precinct boundary so that sb 12's
// "previous" rotates between the buffer holding last precinct's sb 23
// and the buffer being filled by the current precinct's sb 12.
//
// All other sub-bands use zero-prediction (mode 0x71): prev = 0, so no
// cross-band state is needed — but GCLI values are still saved for
// consistency and potential use by the fully-insignificant flag logic.
//
// Precinct-16 reset: when precinct index == 16 and
// (Bp == 5 || (Bp == 4 && Br ≤ 7)), all GCLI prediction state is
// zeroed.
//


#ifndef LIBRAW_NIKON_HE_PREDECESSOR_H
#define LIBRAW_NIKON_HE_PREDECESSOR_H

#include "nikon_he_subband_config.h"
#include <cstdint>

namespace nikon_he {

class PrecinctPredecessorState {
public:
    PrecinctPredecessorState() = default;

    // Initialize storage based on sub-band layout.
    // Must be called once before any other methods.
    // Allocates: sum(ng[i]) uint8_t for GCLI vectors + rotation buffers.
    void init(const SubbandConfig config[26]);

    // Free allocated storage.
    void destroy();

    // Get the "previous band" GCLI values for a sub-band.
    // For sb 12: returns sb 23's GCLIs from the previous precinct.
    // For sb 23: returns sb 12's GCLIs from the current precinct.
    // For all others: returns a zero-initialized vector (mode 0x71).
    // The returned pointer is valid until the next save_gcli() or
    // advance_precinct() call for that sub-band.
    const uint8_t* get_previous_gcli(int sb_index) const;

    // Save decoded GCLI values for a sub-band. For sb 12 and sb 23,
    // this also updates the cross-band rotation buffers.
    // gcli_values must have length ≥ config[sb_index].ng.
    void save_gcli(int sb_index, const uint8_t* gcli_values);

    // Fully-insignificant flag per sub-band.
    void set_fully_insig(int sb_index, bool flag);
    bool is_fully_insig(int sb_index) const;

    // Advance to the next precinct. Swaps rotation buffers for sb 12/23.
    void advance_precinct();

    // Get current precinct index (0-based).
    int precinct_index() const { return precinct_index_; }

    // Reset all GCLI state to zero. Called on precinct-16 boundary
    // when the reset condition is met.
    void reset_gcli_state(const SubbandConfig config[26]);

private:
    // Per-sub-band GCLI storage. Each gcli_store_[i] points to a
    // buffer of config[i].ng uint8_t.
    uint8_t* gcli_store_[26] = {};

    // Number of groups per sub-band (set in init).
    int ng_[26] = {};

    // Rotation buffers for sb 12 / sb 23 cross-band prediction.
    // rotation_buf_a_ holds previous precinct's sb 23 → current sb 12 prev.
    // rotation_buf_b_ holds current precinct's sb 12 → current sb 23 prev.
    // At advance: buf_a = buf_b, buf_b = buf_a.
    uint8_t* rotation_buf_a_ = nullptr;  // sb 23 → for sb 12 prev
    uint8_t* rotation_buf_b_ = nullptr;  // sb 12 → for sb 23 prev

    int rotation_buf_size_ = 0;  // max(config[12].ng, config[23].ng)

    // Fully-insignificant flags.
    bool fully_insig_[26] = {};

    int precinct_index_ = 0;
};

// Static helper: check if GCLI reset condition is met.
// p is the 0-based precinct index within the tile (0..17).
inline bool should_reset_gcli(int precinct_index, int Bp, int Br) {
    if (precinct_index != 16) return false;
    if (Bp == 5) return true;
    if (Bp == 4 && Br <= 7) return true;
    return false;
}

}  // namespace nikon_he

#endif  // LIBRAW_NIKON_HE_PREDECESSOR_H
