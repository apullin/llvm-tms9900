/*
 * TI-99/4A Game of Life (Graphics II mode)
 * Full 256x192 toroidal board using bit-packed storage.
 */

#include <stdint.h>

/* ===== CARTRIDGE HEADER ===== */
__attribute__((section(".cart_header")))
const uint8_t cart_header[] = {
    0xAA, 0x01,                           /* 0x00: Cartridge identifier */
    0x00, 0x00,                           /* 0x02: Number of programs */
    0x00, 0x00,                           /* 0x04: Power-up entry (none) */
    0x60, 0x10,                           /* 0x06: Program list at 0x6010 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x08: Reserved */
    /* Program list entry at 0x6010 */
    0x00, 0x00,                           /* 0x10: Next entry (none) */
    0x60, 0x24,                           /* 0x12: Entry point at 0x6024 */
    12, 'G','A','M','E',' ','O','F',' ','L','I','F','E',
    0x00                                  /* Pad to align */
};

/* _start is provided by crt0.s */

#define SCREEN_W 256
#define SCREEN_H 192
#define ROW_BYTES (SCREEN_W / 8)
#define CHAR_COLS 32
#define CHAR_ROWS 24

#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00

#define COLOR_FG 0x0E /* cyan */
#define COLOR_BG 0x00 /* black */

#define COLOR_TABLE_ADDR 0x2000
#define PATTERN_TABLE_ADDR 0x0000
#define NAME_TABLE_ADDR 0x3800

#define STATIC_DELAY_MS 1200
#define DELAY_1MS_COUNT 8

/* ===== VDP ACCESS ===== */
#define VDP_OUT(port, val) do { \
    uint16_t out = (uint16_t)(val) << 8; \
    asm volatile("MOVB %0, @%1" : : "r"(out), "i"(port) : "memory", "cc"); \
} while (0)

static void vdp_cmd(uint8_t val)  { VDP_OUT(VDP_CMD_PORT, val); }
static void vdp_data(uint8_t val) { VDP_OUT(VDP_DATA_PORT, val); }

static void vdp_set_register(uint8_t reg, uint8_t val) {
    vdp_cmd(val);
    vdp_cmd(reg | 0x80);
}

static void vdp_set_write_addr(uint16_t addr) {
    vdp_cmd(addr & 0xFF);
    vdp_cmd(((addr >> 8) & 0x3F) | 0x40);
}

static void vdp_init_gfx2(void) {
    vdp_set_register(0, 0x02); /* M3=1 -> Graphics II */
    vdp_set_register(1, 0xE0); /* Display on, 16K VRAM */
    vdp_set_register(2, 0x0E); /* Name table @ 0x3800 */
    vdp_set_register(3, 0x80); /* Color table @ 0x2000 */
    vdp_set_register(4, 0x00); /* Pattern table @ 0x0000 */
    vdp_set_register(5, 0x76); /* Sprite attribute @ 0x3B00 */
    vdp_set_register(6, 0x03); /* Sprite pattern @ 0x1800 */
    vdp_set_register(7, 0x00); /* Border black */
}

static void vdp_init_name_table(void) {
    uint16_t row;
    uint16_t col;

    vdp_set_write_addr(NAME_TABLE_ADDR);
    for (row = 0; row < CHAR_ROWS; row++) {
        uint8_t base = (uint8_t)((row & 7u) * CHAR_COLS);
        for (col = 0; col < CHAR_COLS; col++) {
            vdp_data((uint8_t)(base + col));
        }
    }
}

static void vdp_init_color_table(void) {
    uint16_t i;
    uint8_t color = (uint8_t)((COLOR_FG << 4) | (COLOR_BG & 0x0F));

    vdp_set_write_addr(COLOR_TABLE_ADDR);
    for (i = 0; i < SCREEN_H * CHAR_COLS; i++) {
        vdp_data(color);
    }
}

/* ===== RNG ===== */
static uint16_t rng_state = 0xACE1u;

static uint16_t rng_next(void) {
    uint16_t lsb = (uint16_t)(rng_state & 1u);
    rng_state >>= 1;
    if (lsb) {
        rng_state ^= 0xB400u;
    }
    return rng_state;
}

/* ===== DELAY ===== */
static volatile uint16_t delay_counter;

static void delay_1ms(void) {
    for (delay_counter = 0; delay_counter < DELAY_1MS_COUNT; delay_counter++) {
    }
}

static void delay_ms(uint16_t ms) {
    while (ms--) {
        delay_1ms();
    }
}

/* ===== BOARD ===== */
static uint8_t board_a[SCREEN_H][ROW_BYTES];
static uint8_t board_b[SCREEN_H][ROW_BYTES];
static uint8_t (*cur)[ROW_BYTES] = board_a;
static uint8_t (*next)[ROW_BYTES] = board_b;

static const uint8_t bit_mask[8] = {
    0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u
};

static void clear_board(uint8_t (*b)[ROW_BYTES]) {
    uint16_t y;
    uint16_t x;
    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < ROW_BYTES; x++) {
            b[y][x] = 0;
        }
    }
}

static uint8_t get_cell(uint8_t (*b)[ROW_BYTES], uint16_t x, uint16_t y) {
    return (uint8_t)((b[y][x >> 3] & bit_mask[x & 7]) != 0);
}

static void set_cell(uint8_t (*b)[ROW_BYTES], uint16_t x, uint16_t y) {
    b[y][x >> 3] |= bit_mask[x & 7];
}

static void seed_board(uint8_t (*b)[ROW_BYTES]) {
    uint16_t y;
    uint16_t x;

    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < ROW_BYTES; x++) {
            uint8_t r1 = (uint8_t)rng_next();
            uint8_t r2 = (uint8_t)(rng_next() >> 8);
            uint8_t v = (uint8_t)(r1 & r2);
            v |= (uint8_t)((v << 1) | (v >> 1));
            b[y][x] = v;
        }
    }
}

static uint8_t step_board(uint8_t (*src)[ROW_BYTES], uint8_t (*dst)[ROW_BYTES]) {
    uint16_t y;
    uint16_t x;
    uint8_t changed = 0;

    clear_board(dst);

    for (y = 0; y < SCREEN_H; y++) {
        uint16_t y_prev = (y == 0) ? (SCREEN_H - 1) : (y - 1);
        uint16_t y_next = (y == (SCREEN_H - 1)) ? 0 : (y + 1);

        for (x = 0; x < SCREEN_W; x++) {
            uint16_t x_prev = (x == 0) ? (SCREEN_W - 1) : (x - 1);
            uint16_t x_next = (x == (SCREEN_W - 1)) ? 0 : (x + 1);
            uint8_t n = 0;
            uint8_t cur_alive;
            uint8_t next_alive;

            n += get_cell(src, x_prev, y_prev);
            n += get_cell(src, x, y_prev);
            n += get_cell(src, x_next, y_prev);
            n += get_cell(src, x_prev, y);
            n += get_cell(src, x_next, y);
            n += get_cell(src, x_prev, y_next);
            n += get_cell(src, x, y_next);
            n += get_cell(src, x_next, y_next);

            cur_alive = get_cell(src, x, y);
            next_alive = (uint8_t)((n == 3) || (cur_alive && n == 2));
            if (next_alive) {
                set_cell(dst, x, y);
            }
            if (next_alive != cur_alive) {
                changed = 1;
            }
        }
    }

    return changed;
}

static void vdp_write_patterns(uint8_t (*b)[ROW_BYTES]) {
    uint16_t group;
    uint16_t pattern;
    uint16_t row_in_char;

    vdp_set_write_addr(PATTERN_TABLE_ADDR);
    for (group = 0; group < 3; group++) {
        for (pattern = 0; pattern < 256; pattern++) {
            uint16_t char_row = (uint16_t)(pattern >> 5);
            uint16_t col = (uint16_t)(pattern & 0x1F);
            uint16_t base_y = (uint16_t)(group * 64 + (char_row << 3));
            for (row_in_char = 0; row_in_char < 8; row_in_char++) {
                vdp_data(b[base_y + row_in_char][col]);
            }
        }
    }
}

void main(void) {
    vdp_init_gfx2();
    vdp_init_name_table();
    vdp_init_color_table();

    seed_board(cur);
    vdp_write_patterns(cur);

    for (;;) {
        uint8_t changed = step_board(cur, next);
        if (!changed) {
            delay_ms(STATIC_DELAY_MS);
            seed_board(cur);
        } else {
            uint8_t (*tmp)[ROW_BYTES] = cur;
            cur = next;
            next = tmp;
        }
        vdp_write_patterns(cur);
    }
}
