// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#define VIAL_VALID_LAYER_MASK ((1UL << 3) - 1)

static layer_state_t remap_compacted_layers(layer_state_t state) {
    // Vial keeps tap-dance actions in EEPROM, so older setups may still target
    // the pre-compaction symbol and adjust layers (4 and 6).
    if (state & (1UL << 4)) {
        state = (state & ~(1UL << 4)) | (1UL << 1);
    }
    if (state & (1UL << 6)) {
        state = (state & ~(1UL << 6)) | (1UL << 2);
    }
    return state;
}

void keyboard_post_init_user(void) {
    // Discard stale layer state saved by the pre-compaction Vial keymap.
    default_layer_set(1UL);
    layer_clear();
}

layer_state_t default_layer_state_set_user(layer_state_t state) {
    return remap_compacted_layers(state) & VIAL_VALID_LAYER_MASK;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    return remap_compacted_layers(state) & VIAL_VALID_LAYER_MASK;
}
