// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "halcyon.h"
#include "hlc_tft_display.h"

#include "hardware/structs/rosc.h"

static uint8_t lcd_surface_fb[SURFACE_REQUIRED_BUFFER_BYTE_SIZE(135, 240, 16)];

int color_value = 0;

painter_device_t lcd;
painter_device_t lcd_surface;

led_t last_led_usb_state = {0};

#define ALIEN_WIDTH  11
#define ALIEN_HEIGHT 8
#define ALIEN_SCALE  7
#define ALIEN_X      ((LCD_WIDTH - ALIEN_WIDTH * ALIEN_SCALE) / 2)
#define ALIEN_Y      96

static const uint16_t alien_frames[3][ALIEN_HEIGHT] = {
    { // Quiet pose
    0b00100000100, 0b00010001000, 0b00111111100, 0b01101110110,
    0b11111111111, 0b10111111101, 0b10100000101, 0b00011011000,
    },
    { // Typing pose
        0b00100000100, 0b10010001001, 0b10111111101, 0b11101110111,
        0b11111111111, 0b00111111100, 0b00100000100, 0b01000000010,
    },
    { // Brief pose shown after changing layers
        0b00100000100, 0b00100000100, 0b00111111100, 0b01111111110,
        0b11011010110, 0b11111111111, 0b00110101100, 0b01100000110,
    },
};

#define LABEL_SCALE 5
#define LABEL_Y     8
#define LABEL_CHAR_WIDTH (5 * LABEL_SCALE)
#define LABEL_GAP        8

static uint8_t layer_label_width(const char *label) {
    uint8_t character_count = 0;
    while (character_count < 3 && label[character_count]) {
        character_count++;
    }
    return character_count ? character_count * LABEL_CHAR_WIDTH + (character_count - 1) * LABEL_GAP : 0;
}

static const uint8_t label_glyphs[9][7] = {
    [0] = {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}, // A
    [1] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}, // B
    [2] = {0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111}, // C
    [3] = {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}, // 0
    [4] = {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, // 1
    [5] = {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}, // 2
    [6] = {0b11111, 0b00010, 0b00100, 0b01110, 0b00001, 0b10001, 0b01110}, // 3
    [7] = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}, // F
    [8] = {0b01111, 0b10000, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110}, // G
};

static void draw_layer_label(const char *label, uint8_t hue, uint8_t sat, uint8_t val) {
    uint8_t label_x = (LCD_WIDTH - layer_label_width(label)) / 2;
    for (uint8_t character = 0; character < 3; character++) {
        if (!label[character]) break;
        uint8_t glyph;
        switch (label[character]) {
            case 'A': glyph = 0; break;
            case 'B': glyph = 1; break;
            case 'C': glyph = 2; break;
            case '0': glyph = 3; break;
            case '1': glyph = 4; break;
            case '2': glyph = 5; break;
            case '3': glyph = 6; break;
            case 'F': glyph = 7; break;
            case 'G': glyph = 8; break;
            default: continue;
        }
        for (uint8_t row = 0; row < 7; row++) {
            for (uint8_t col = 0; col < 5; col++) {
                if (label_glyphs[glyph][row] & (1U << (4 - col))) {
                    uint16_t x = label_x + character * (LABEL_CHAR_WIDTH + LABEL_GAP) + col * LABEL_SCALE;
                    uint16_t y = LABEL_Y + row * LABEL_SCALE;
                    qp_rect(lcd_surface, x, y, x + LABEL_SCALE - 1, y + LABEL_SCALE - 1, hue, sat, val, true);
                }
            }
        }
    }
}

#define GRID_WIDTH 27
#define GRID_HEIGHT 48
#define CELL_SIZE 4 // Cell size excluding outline
#define OUTLINE_SIZE 1

// Define the probability factor for initial alive cells
#define INITIAL_ALIVE_PROBABILITY 0.2 // 20% chance of being alive

bool grid[GRID_HEIGHT][GRID_WIDTH];         // Current state
bool new_grid[GRID_HEIGHT][GRID_WIDTH];     // Next state
bool changed_grid[GRID_HEIGHT][GRID_WIDTH]; // Tracks changed cells

uint32_t get_random_32bit(void) {
    uint32_t random_value = 0;
    for (int i = 0; i < 32; i++) {
        wait_ms(1);
        random_value = (random_value << 1) | (rosc_hw->randombit & 1);
    }
    return random_value;
}

void init_grid() {
    // Initialize grid with alive cells
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x]         = (rand() < INITIAL_ALIVE_PROBABILITY * RAND_MAX); // Use probability factor
            changed_grid[y][x] = true;                                            // Mark all as changed initially
        }
    }
}

void draw_grid() {
    uint8_t hue      = 0; // Hue for alive cells
    uint8_t sat      = 0; // Saturation for alive cells
    uint8_t val_dead = 0; // Brightness for dead cells

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (changed_grid[y][x]) { // Only update changed cells
                uint16_t left   = x * (CELL_SIZE + OUTLINE_SIZE);
                uint16_t top    = y * (CELL_SIZE + OUTLINE_SIZE);
                uint16_t right  = left + CELL_SIZE + OUTLINE_SIZE;
                uint16_t bottom = top + CELL_SIZE + OUTLINE_SIZE;

                // Draw the outline
                qp_rect(lcd_surface, left, top, right, bottom, hue, sat, val_dead, true);

                // Draw the filled cell inside the outline if it's alive
                if (grid[y][x]) {
                    switch (color_value) {
                        case 0:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_0, true);
                            break;
                        case 1:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_1, true);
                            break;
                        case 2:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_2, true);
                            break;
                        case 3:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_3, true);
                            break;
                        case 4:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_4, true);
                            break;
                        case 5:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_5, true);
                            break;
                        case 6:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_6, true);
                            break;
                        case 7:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_7, true);
                            break;
                        default:
                            qp_rect(lcd_surface, left + OUTLINE_SIZE, top + OUTLINE_SIZE, right - OUTLINE_SIZE, bottom - OUTLINE_SIZE, HSV_LAYER_UNDEF, true);
                    }
                }
            }
        }
    }
}

void update_grid() {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int alive_neighbors = 0;

            // Count alive neighbors
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dy == 0 && dx == 0) continue; // Skip the current cell
                    int ny = y + dy;
                    int nx = x + dx;
                    if (ny >= 0 && ny < GRID_HEIGHT && nx >= 0 && nx < GRID_WIDTH) {
                        alive_neighbors += grid[ny][nx];
                    }
                }
            }

            // Apply the rules of the Game of Life
            if (grid[y][x]) {
                // Any live cell with two or three live neighbours survives.
                new_grid[y][x] = (alive_neighbors == 2 || alive_neighbors == 3);
            } else {
                // Any dead cell with exactly three live neighbours becomes a live cell.
                new_grid[y][x] = (alive_neighbors == 3);
            }

            // Track changed cells
            changed_grid[y][x] = (grid[y][x] != new_grid[y][x]);
        }
    }

    // Copy new grid state to current grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x] = new_grid[y][x];
        }
    }
}

// Function to add a cluster of cells at a random position
void add_cell_cluster() {
    int cluster_size = 3; // Size of the cluster (3x3)
    int x            = rand() % (GRID_WIDTH - cluster_size);
    int y            = rand() % (GRID_HEIGHT - cluster_size);

    for (int dy = 0; dy < cluster_size; dy++) {
        for (int dx = 0; dx < cluster_size; dx++) {
            bool is_alive                = rand() % 2; // Randomly choose between 0 and 1
            grid[y + dy][x + dx]         = is_alive;   // Set the cell to be alive
            changed_grid[y + dy][x + dx] = true;       // Mark the cell as changed
        }
    }
}

static void draw_alien(uint8_t frame, uint8_t hue, uint8_t sat, uint8_t val) {
    uint16_t right  = ALIEN_X + ALIEN_WIDTH * ALIEN_SCALE;
    uint16_t bottom = ALIEN_Y + ALIEN_HEIGHT * ALIEN_SCALE;
    qp_rect(lcd_surface, ALIEN_X - 4, ALIEN_Y - 4, right + 4, bottom + 4, HSV_BLACK, true);

    for (uint8_t row = 0; row < ALIEN_HEIGHT; row++) {
        for (uint8_t col = 0; col < ALIEN_WIDTH; col++) {
            if (alien_frames[frame][row] & (1U << (ALIEN_WIDTH - 1 - col))) {
                uint16_t x = ALIEN_X + col * ALIEN_SCALE;
                uint16_t y = ALIEN_Y + row * ALIEN_SCALE;
                qp_rect(lcd_surface, x, y, x + ALIEN_SCALE - 1, y + ALIEN_SCALE - 1, hue, sat, val, true);
            }
        }
    }
}

static void draw_caps_lock(bool enabled) {
    uint8_t hue = enabled ? 23 : 0;
    uint8_t sat = enabled ? 220 : 0;
    uint8_t val = enabled ? 255 : 110;
    const uint8_t x = 58;
    const uint8_t y = 198;

    // Clear the whole icon first so the open shackle cannot leave colored pixels behind.
    qp_rect(lcd_surface, x - 2, y, x + 22, y + 30, HSV_BLACK, true);

    // Shackle: closed when Caps Lock is on, open otherwise.
    qp_rect(lcd_surface, x + 3, y + 4, x + 6, y + 12, hue, sat, val, true);
    qp_rect(lcd_surface, x + 14, y + (enabled ? 4 : 9), x + 17, y + 12, hue, sat, val, true);
    qp_rect(lcd_surface, x + 3, y + 2, x + 17, y + 5, hue, sat, val, true);
    if (!enabled) {
        qp_rect(lcd_surface, x + 13, y + 2, x + 17, y + 7, HSV_BLACK, true);
    }

    // Lock body and keyhole.
    qp_rect(lcd_surface, x, y + 12, x + 20, y + 28, hue, sat, val, true);
    qp_rect(lcd_surface, x + 9, y + 17, x + 11, y + 22, HSV_BLACK, true);
}

void update_display(void) {
    static bool     first_run_led        = false;
    static bool     first_run_layer      = false;
    static uint8_t  last_displayed_layer = UINT8_MAX;
    static uint8_t  last_alien_frame     = UINT8_MAX;
    static uint32_t last_alien_animation = 0;
    static uint32_t layer_animation_end = 0;

    led_t led_usb_state = host_keyboard_led_state();
    if (last_led_usb_state.raw != led_usb_state.raw || !first_run_led) {
        draw_caps_lock(led_usb_state.caps_lock);
        last_led_usb_state = led_usb_state;
        first_run_led      = true;
    }

    uint8_t active_layer = get_highest_layer(layer_state | default_layer_state);
    // Older Vial tap-dance settings can still request the pre-compaction layer numbers.
    if (active_layer == 4) active_layer = 1;
    if (active_layer == 6) active_layer = 2;
    const char *layer_name;
    uint8_t hue, sat, val;
    switch (active_layer) {
        case 0:
            layer_name = "ABC";
            hue        = 170; // Royal blue
            sat        = 255;
            val        = 255;
            break;
        case 1:
            layer_name = "123";
            hue        = 213; // Vivid magenta
            sat        = 255;
            val        = 255;
            break;
        case 2:
            layer_name = "CFG";
            hue        = 75; // Bright lime
            sat        = 255;
            val        = 255;
            break;
        default:
            layer_name = "CFG";
            hue        = 75;
            sat        = 255;
            val        = 255;
    }

    uint32_t now = timer_read32();
    bool had_displayed_layer = last_displayed_layer != UINT8_MAX;
    bool layer_changed = active_layer != last_displayed_layer || !first_run_layer;
    if (layer_changed) {
        qp_rect(lcd_surface, 0, 0, LCD_WIDTH - 1, 50, HSV_BLACK, true);
        draw_layer_label(layer_name, hue, sat, val);
        last_displayed_layer = active_layer;
        first_run_layer      = true;
        if (had_displayed_layer) {
            layer_animation_end = now + 700;
        }
    }

    uint8_t alien_frame = 0;
    if (layer_animation_end && (int32_t)(layer_animation_end - now) > 0) {
        alien_frame = 2;
    } else if (last_input_activity_elapsed() < 1200) {
        if (timer_elapsed32(last_alien_animation) >= 350) {
            alien_frame = last_alien_frame == 1 ? 0 : 1;
            last_alien_animation = now;
        } else if (last_alien_frame <= 1) {
            alien_frame = last_alien_frame;
        }
    }

    if (alien_frame != last_alien_frame || layer_changed) {
        draw_alien(alien_frame, hue, sat, val);
        last_alien_frame = alien_frame;
    }
}

// Called from halcyon.c
void module_suspend_power_down_kb(void) {
    qp_power(lcd, false);
}

// Called from halcyon.c
void module_suspend_wakeup_init_kb(void) {
    qp_power(lcd, true);
}

// Called from halcyon.c
bool module_post_init_kb(void) {
    // Turn on backlight
    backlight_enable();

    // Make the devices
    lcd         = qp_st7789_make_spi_device(LCD_WIDTH, LCD_HEIGHT, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, LCD_SPI_DIVISOR, LCD_SPI_MODE);
    lcd_surface = qp_make_rgb565_surface(LCD_WIDTH, LCD_HEIGHT, lcd_surface_fb);

    // Initialise the LCD
    qp_init(lcd, LCD_ROTATION);
    qp_set_viewport_offsets(lcd, LCD_OFFSET_X, LCD_OFFSET_Y);
    qp_clear(lcd);
    qp_rect(lcd, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);
    qp_power(lcd, true);
    qp_flush(lcd);

    // Initialise the LCD surface
    qp_init(lcd_surface, LCD_ROTATION);
    qp_rect(lcd_surface, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);
    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    if (!module_post_init_user()) {
        return false;
    }

    return true;
}

// Called from halcyon.c
bool display_module_housekeeping_task_kb(bool second_display) {
    if (!display_module_housekeeping_task_user(second_display)) {
        return false;
    }

    if (second_display) {
        static uint32_t last_draw                     = 0;
        static bool     second_display_set            = false;
        static uint32_t previous_matrix_activity_time = 0;

        if (!second_display_set) {
            srand(get_random_32bit());
            init_grid();
            color_value        = rand() % 8;
            second_display_set = true;
        }

        if (timer_elapsed32(last_draw) >= 100) { // Throttle to 10 fps
            draw_grid();
            update_grid();

            if (previous_matrix_activity_time != last_matrix_activity_time()) {
                color_value = rand() % 8;
                add_cell_cluster();
                previous_matrix_activity_time = last_matrix_activity_time();
            }

            last_draw = timer_read32();
        }
    }

    // Update display information (layers, numlock, etc.)
    if (!second_display) {
        update_display();
    }

    // Move surface to lcd
    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    return true;
}
