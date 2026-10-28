// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

enum layers {
    _QWERTY = 0,
    _SYM,
    _ADJUST,
};


// Aliases for readability
#define QWERTY   DF(_QWERTY)

#define SYM      MO(_SYM)

#define CTL_ESC  MT(MOD_LCTL, KC_ESC)
#define CTL_QUOT MT(MOD_RCTL, KC_QUOTE)
#define CTL_MINS MT(MOD_RCTL, KC_MINUS)
#define ALT_ENT  MT(MOD_LALT, KC_ENT)

// These layer indices were compacted; ignore a stale default layer saved by
// the previous firmware and always boot into the QWERTY base layer.
void keyboard_post_init_user(void) {
    default_layer_set(1UL << _QWERTY);
    layer_clear();
}

// TD(0): tap toggles Sym; hold momentarily activates Sym.
// TD(2): tap/hold and double-tap/hold produce (), [], respectively.
static bool td_sym_held;
static void td_sym_finished(tap_dance_state_t *state, void *user_data) {
    if (state->count == 1 && state->pressed) {
        layer_on(_SYM);
        td_sym_held = true;
    } else if (state->count == 1) {
        layer_invert(_SYM);
    }
}
static void td_sym_reset(tap_dance_state_t *state, void *user_data) {
    if (td_sym_held) {
        layer_off(_SYM);
        td_sym_held = false;
    }
}
static void td_brackets_finished(tap_dance_state_t *state, void *user_data) {
    if (state->count >= 2) {
        tap_code16(state->pressed ? KC_RBRC : KC_LBRC);
    } else {
        tap_code16(state->pressed ? S(KC_0) : S(KC_9));
    }
}
tap_dance_action_t tap_dance_actions[] = {
    [0] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_sym_finished, td_sym_reset),
    [2] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_brackets_finished, NULL),
};

// Note: LAlt/Enter (ALT_ENT) is not the same thing as the keyboard shortcut Alt+Enter.
// The notation `mod/tap` denotes a key that activates the modifier `mod` when held down, and
// produces the key `tap` when tapped (i.e. pressed and released).

// There is an extra row added for the Halcyon modules. Currently only the Encoder module is
// supported but we reserve 5 keys per half for future expansion. Your personal keymap will also
// need to be updated to include this row, and the `LAYOUT` macro will need to be updated to
// `LAYOUT` in order to compile.

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_QWERTY] = LAYOUT(
        KC_ESC, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_BSPC,
        KC_TAB, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOTE,
        KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_MINS, KC_CAPS, LCA(KC_DEL), TD(2), KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
        KC_LCTL, KC_LGUI, KC_LALT, KC_SPC, TD(0), TD(0), KC_ENT, KC_RALT, KC_APP, KC_DEL
    ), // QWERTY
    [_SYM] = LAYOUT(
        KC_GRV, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_BSPC,
        S(KC_GRV), S(KC_1), S(KC_2), S(KC_3), S(KC_4), S(KC_5), S(KC_6), S(KC_7), S(KC_8), S(KC_9), S(KC_0), KC_EQL,
        S(KC_BSLS), KC_BSLS, S(KC_SCLN), KC_SCLN, KC_MINS, KC_LBRC, S(KC_LBRC), QWERTY, TO(_ADJUST), S(KC_RBRC), KC_RBRC, S(KC_MINS), KC_COMM, KC_DOT, KC_SLSH, S(KC_KP_PLUS),
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______
    ), // Symbols and numbers
    [_ADJUST] = LAYOUT(
        RM_TOGG, RM_NEXT, RM_PREV, QWERTY, _______, _______, _______, _______, _______, _______, _______, _______,
        RM_TOGG, RM_NEXT, RM_NEXT, KC_NO, _______, RM_TOGG, RM_TOGG, RM_SATU, RM_HUEU, RM_VALU, RM_NEXT, _______,
        RM_NEXT, RM_NEXT, RM_NEXT, KC_NO, _______, _______, _______, LCA(KC_DEL), TO(_SYM), _______, _______, RM_SATD, RM_HUED, RM_VALD, RM_PREV, _______,
        _______, _______, _______, KC_NO, TO(0), TO(0), _______, _______, _______, _______
    ), // Adjust
};
// clang-format on

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_PGUP, KC_PGDN), ENCODER_CCW_CW(KC_LEFT, KC_RGHT) },
    { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_PGUP, KC_PGDN), ENCODER_CCW_CW(KC_PGUP, KC_PGDN) },
    { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_PGUP, KC_PGDN), ENCODER_CCW_CW(RM_VALD, RM_VALU) },
};
#endif

#if defined (HALCYON_ENABLE)
const uint16_t left_halcyon_buttons[10][5] = {
    [_QWERTY] = { KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO },
    [_SYM] = { _______, _______, _______, _______, _______ },
    [_ADJUST] = { _______, _______, _______, _______, _______ },
};

const uint16_t right_halcyon_buttons[10][5] = {
    [_QWERTY] = { RM_TOGG, KC_NO, KC_NO, KC_NO, KC_NO },
    [_SYM] = { _______, _______, _______, _______, _______ },
    [_ADJUST] = { _______, _______, _______, _______, _______ },
};
#endif
