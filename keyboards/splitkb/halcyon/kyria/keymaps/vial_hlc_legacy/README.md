# Halcyon Kyria VIAL keymaps

Both VIAL keymaps now expose three layers: base (0), symbols/numbers (1, formerly 4), and
adjust (2, formerly 6). Layer references and encoder mappings are compacted to match.

The compiled fallback map uses `MO(1)` on both thumb keys, so layer access works even before
loading Vial's saved actions. The clean `layout.vil` assigns tap dance 0 to those thumb keys:
tap toggles layer 1 (`TG(1)`), hold momentarily activates it (`MO(1)`). Tap dance 2 sends
parentheses on a single tap and brackets on a double tap. From layer 1, `TO(2)` enters adjust;
layer 2 has keys to return to layers 1 and 0.

To reset stale Vial settings, temporarily assign `EE_CLR` to a reachable base-layer key in
Vial and press it. This erases Vial's saved keymap, tap dances, macros, and settings. After
the keyboard restarts, import `layout.vil` to restore the clean three-layer map and both tap
dances. The TFT layer labels are ABC, 123, and CFG.
