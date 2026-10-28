VIA_ENABLE = yes
VIAL_ENABLE = yes
VIALRGB_ENABLE = yes
TAP_DANCE_ENABLE = yes

ENCODER_MAP_ENABLE = yes

# This adds module functionality to your keyboard (files found in users/halcyon_modules)
USER_NAME := halcyon_modules

# Keep stale Vial EEPROM actions from activating removed layers.
SRC += $(USER_PATH)/splitkb/vial_layer_safety.c
