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

#define CANVAS_WIDTH  100
#define CANVAS_HEIGHT 200
#define CANVAS_SCALE  108
#define CANVAS_X      ((LCD_WIDTH - CANVAS_SCALE) / 2)
#define CANVAS_Y      (((LCD_HEIGHT - CANVAS_SCALE * 2) / 2) - 13)

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

static void draw_canvas_rect(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t hue, uint8_t sat, uint8_t val) {
    int16_t x0 = CANVAS_X + x * CANVAS_SCALE / CANVAS_WIDTH;
    int16_t x1 = CANVAS_X + (x + width) * CANVAS_SCALE / CANVAS_WIDTH - 1;
    int16_t y0 = CANVAS_Y + y * CANVAS_SCALE / CANVAS_WIDTH;
    int16_t y1 = CANVAS_Y + (y + height) * CANVAS_SCALE / CANVAS_WIDTH - 1;
    if (x1 < 0 || y1 < 0 || x0 >= LCD_WIDTH || y0 >= LCD_HEIGHT) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x0 >= LCD_WIDTH) x0 = LCD_WIDTH - 1;
    if (y0 >= LCD_HEIGHT) y0 = LCD_HEIGHT - 1;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x1 >= LCD_WIDTH) x1 = LCD_WIDTH - 1;
    if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
    qp_rect(lcd_surface, x0, y0, x1, y1, hue, sat, val, true);
}

static void draw_pixel_bubble(int16_t x, int16_t y, uint8_t radius) {
    static const uint8_t widths_r4[9] = {3, 7, 9, 9, 9, 9, 9, 7, 3};
    static const uint8_t widths_r5[11] = {3, 7, 9, 11, 11, 11, 11, 11, 9, 7, 3};
    static const uint8_t widths_r6[13] = {3, 7, 9, 11, 13, 13, 13, 13, 13, 11, 9, 7, 3};
    const uint8_t *widths = radius == 4 ? widths_r4 : (radius == 5 ? widths_r5 : widths_r6);
    x = x < radius + 2 ? radius + 2 : x;
    x = x > CANVAS_WIDTH - radius - 2 ? CANVAS_WIDTH - radius - 2 : x;

    for (int8_t row = -(int8_t)radius; row <= (int8_t)radius; row++) {
        uint8_t width = widths[row + radius];
        int16_t left = x - width / 2;
        draw_canvas_rect(left, y + row, width, 1, 0, 0, 105); // Soft white rim
        if (width > 3) {
            uint8_t inner_width = width - 2;
            draw_canvas_rect(left + 1, y + row, inner_width, 1, 155, 120, 62); // Water-dark center
        }
    }

    draw_canvas_rect(x - 2, y - 2, 2, 2, 0, 0, 255); // Upper-left glint
}

static void draw_axolotl(uint8_t phase, uint8_t hue, uint8_t sat, uint8_t val, bool caps_lock, bool typing) {
    static const int8_t wave[16] = {0, 6, 11, 14, 15, 14, 11, 6, 0, -6, -11, -14, -15, -14, -11, -6};
    int8_t head = wave[phase & 15] * 3 / 30;
    int8_t body = wave[(phase + 4) & 15] * 5 / 30;
    int8_t tail = wave[(phase + 8) & 15] * 8 / 30;
    // HTML palette mapped onto each active layer: base P, shadow S, gills G, cheeks M.
    uint8_t fill_hue = hue + 8;
    uint8_t gill_hue = hue;
    uint8_t shade_hue = hue;
    uint8_t cheek_hue = hue + 12;
    uint8_t fill_sat = sat / 2;
    uint8_t shade_sat = sat;
    uint8_t gill_sat = sat;
    uint8_t cheek_sat = sat;
    uint8_t fill_val = val;
    uint8_t shade_val = val > 28 ? val - 28 : val;
    uint8_t gill_val = val;
    uint8_t cheek_val = val;
    qp_rect(lcd_surface, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);

    // Keep the layer-colored edge; bubbles rise only during recent typing.
    qp_rect(lcd_surface, 0, 0, LCD_WIDTH - 1, 3, hue, sat, 85, true);
    qp_rect(lcd_surface, 0, LCD_HEIGHT - 4, LCD_WIDTH - 1, LCD_HEIGHT - 1, hue, sat, 85, true);
    qp_rect(lcd_surface, 0, 4, 3, LCD_HEIGHT - 5, hue, sat, 85, true);
    qp_rect(lcd_surface, LCD_WIDTH - 4, 4, LCD_WIDTH - 1, LCD_HEIGHT - 5, hue, sat, 85, true);
    if (typing) {
        int16_t b1x = 25 + wave[phase & 15] * 15 / 15;
        int16_t b2x = 75 + wave[(phase + 5) & 15] * 20 / 15;
        int16_t b3x = 50 + wave[(phase + 10) & 15] * 25 / 15;
        int16_t b1y = 230 - (phase * 5) % 250;
        int16_t b2y = 230 - (phase * 7 + 80) % 250;
        int16_t b3y = 230 - (phase * 4 + 160) % 250;
        draw_pixel_bubble(b1x, b1y, 5);
        draw_pixel_bubble(b2x, b2y, 4);
        draw_pixel_bubble(b3x, b3y, 6);
    }

    // Fluffy mirrored gills.
    draw_canvas_rect(18 + head, 45, 12, 5, gill_hue, gill_sat, gill_val); draw_canvas_rect(15 + head, 47, 7, 4, gill_hue, gill_sat, gill_val);
    draw_canvas_rect(15 + head, 55, 18, 6, gill_hue, gill_sat, gill_val); draw_canvas_rect(8 + head, 57, 9, 4, gill_hue, gill_sat, gill_val);
    draw_canvas_rect(19 + head, 67, 11, 5, gill_hue, gill_sat, gill_val); draw_canvas_rect(16 + head, 69, 7, 4, gill_hue, gill_sat, gill_val);
    draw_canvas_rect(70 + head, 45, 12, 5, gill_hue, gill_sat, gill_val); draw_canvas_rect(78 + head, 47, 7, 4, gill_hue, gill_sat, gill_val);
    draw_canvas_rect(71 + head, 55, 18, 6, gill_hue, gill_sat, gill_val); draw_canvas_rect(83 + head, 57, 9, 4, gill_hue, gill_sat, gill_val);
    draw_canvas_rect(70 + head, 67, 11, 5, gill_hue, gill_sat, gill_val); draw_canvas_rect(77 + head, 69, 7, 4, gill_hue, gill_sat, gill_val);

    // Rounded head, shaded rim, and layer-colored face.
    draw_canvas_rect(35 + head, 41, 30, 42, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(32 + head, 43, 36, 38, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(29 + head, 46, 42, 32, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(27 + head, 50, 46, 24, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(36 + head, 42, 28, 40, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(33 + head, 44, 34, 36, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(30 + head, 47, 40, 30, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(28 + head, 51, 44, 22, fill_hue, fill_sat, fill_val);

    draw_canvas_rect(38 + head, 57, 4, 5, 0, 0, 0);
    draw_canvas_rect(58 + head, 57, 4, 5, 0, 0, 0);
    if (caps_lock) {
        draw_canvas_rect(38 + head, 57, 2, 2, 0, 0, 255);
        draw_canvas_rect(58 + head, 57, 2, 2, 0, 0, 255);
    }
    draw_canvas_rect(33 + head, 63, 5, 3, cheek_hue, cheek_sat, cheek_val);
    draw_canvas_rect(62 + head, 63, 5, 3, cheek_hue, cheek_sat, cheek_val);
    draw_canvas_rect(48 + head, 64, 4, 1, cheek_hue, cheek_sat, cheek_val);
    draw_canvas_rect(47 + head, 63, 1, 1, cheek_hue, cheek_sat, cheek_val);
    draw_canvas_rect(52 + head, 63, 1, 1, cheek_hue, cheek_sat, cheek_val);

    // Rounded body and four legs follow the middle wave.
    draw_canvas_rect(40 + body, 83, 20, 4, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(41 + body, 83, 18, 4, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(37 + body, 87, 26, 12, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(36 + body, 91, 28, 20, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(38 + body, 87, 24, 12, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(37 + body, 90, 26, 20, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(44 + body, 94, 12, 12, 0, 0, 255);
    draw_canvas_rect(31 + body, 93, 6, 5, hue, sat, val);
    draw_canvas_rect(63 + body, 93, 6, 5, hue, sat, val);
    draw_canvas_rect(32 + body, 107, 6, 5, hue, sat, val);
    draw_canvas_rect(62 + body, 107, 6, 5, hue, sat, val);

    // The narrow tail swings with a larger, delayed wave.
    draw_canvas_rect(40 + tail, 111, 20, 15, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(41 + tail, 111, 18, 14, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(43 + tail * 11 / 10, 126, 14, 25, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(44 + tail * 11 / 10, 126, 12, 24, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(46 + tail * 13 / 10, 151, 8, 25, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(47 + tail * 13 / 10, 151, 6, 24, fill_hue, fill_sat, fill_val);
    draw_canvas_rect(48 + tail * 15 / 10, 176, 4, 15, shade_hue, shade_sat, shade_val);
    draw_canvas_rect(49 + tail * 15 / 10, 176, 2, 14, fill_hue, fill_sat, fill_val);
}

void update_display(void) {
    static bool     first_run_layer      = false;
    static uint8_t  last_displayed_layer = UINT8_MAX;
    static uint8_t  swim_phase            = 0;
    static uint32_t last_swim_frame       = 0;
    static bool     last_typing           = false;
    static bool     last_caps_lock       = false;
    static uint8_t  last_hue             = UINT8_MAX;

    led_t led_usb_state = host_keyboard_led_state();
    bool caps_changed = last_led_usb_state.raw != led_usb_state.raw;
    last_led_usb_state = led_usb_state;

    uint8_t active_layer = get_highest_layer(layer_state | default_layer_state);
    // Older Vial tap-dance settings can still request the pre-compaction layer numbers.
    if (active_layer == 4) active_layer = 1;
    if (active_layer == 6) active_layer = 2;
    uint8_t hue, sat, val;
    switch (active_layer) {
        case 0:
            hue        = 235; // Soft pink
            sat        = 255;
            val        = 255;
            break;
        case 1:
            hue        = 150; // Blue
            sat        = 255;
            val        = 255;
            break;
        case 2:
            hue        = 0; // Neutral gray
            sat        = 0;
            val        = 185;
            break;
        default:
            hue        = 0;
            sat        = 0;
            val        = 185;
    }

    uint32_t now = timer_read32();
    bool typing = last_input_activity_elapsed() < 1200;
    bool layer_changed = active_layer != last_displayed_layer || !first_run_layer;
    bool redraw = layer_changed || caps_changed || led_usb_state.caps_lock != last_caps_lock || hue != last_hue || typing != last_typing;
    if (timer_elapsed32(last_swim_frame) >= 100) {
        swim_phase++;
        last_swim_frame = now;
        redraw = true;
    }

    if (redraw) {
        draw_axolotl(swim_phase, hue, sat, val, led_usb_state.caps_lock, typing);
        last_displayed_layer = active_layer;
        first_run_layer = true;
        last_typing = typing;
        last_caps_lock = led_usb_state.caps_lock;
        last_hue = hue;
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
