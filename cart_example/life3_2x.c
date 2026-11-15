/*
 * TI-99/4A Game of Life (Graphics II mode) - Assembly-optimized version
 * 2x scaled rendering from a 128x96 cell grid.
 *
 * Based on life2_2x.c with the inner row-processing loop replaced by
 * a hand-coded assembly routine (life3_2x_step.S) for optimal register
 * allocation and reduced stack spills.
 */

#include <stdint.h>

#define LIFE_TITLE "GAME OF LIFE"
#define LIFE_VERSION "0.3"
#define LIFE_DATE "NOV 2026"
#define LIFE_MENU_TITLE LIFE_TITLE " " LIFE_VERSION

/* ===== CARTRIDGE HEADER ===== */
/* The program entry is a linker relocation, allowing the name to grow without
 * hard-coding a startup address that overlaps the header. */
extern void _start(void);
struct __attribute__((packed)) life_cart_header {
    uint8_t header[16];
    uint16_t next_program;
    void (*entry)(void);
    uint8_t name_length;
    char name[sizeof(LIFE_MENU_TITLE)];
};
_Static_assert(__builtin_offsetof(struct life_cart_header, entry) == 0x12,
               "Cartridge program entry must be at header offset 0x12.");
__attribute__((section(".cart_header")))
const struct life_cart_header cart_header = {
    {0xAA, 0x01, 0x00, 0x00, 0x00, 0x00, 0x60, 0x10,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    0, _start, sizeof(LIFE_MENU_TITLE) - 1, LIFE_MENU_TITLE
};

/* _start is provided by crt0.s */

#define SCREEN_W 256
#define SCREEN_H 192
#define SCALE 2
#define SCREEN_CELL_W (SCREEN_W / SCALE)
#define SCREEN_CELL_H (SCREEN_H / SCALE)
#define CELL_W SCREEN_CELL_W
#define CELL_H SCREEN_CELL_H
#define CELL_ROW_BYTES (CELL_W / 8)
#define CELL_WORDS (CELL_ROW_BYTES / 2)
#define CHAR_COLS 32
#define CHAR_ROWS 24

#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00
#define PSG_PORT       0x8400

extern uint16_t kbd_scan(void);

#define COLOR_FG 0x07 /* cyan */
#define COLOR_BG 0x01 /* black */
#define COLOR_BORDER 0x0B /* light yellow */
#define COLOR_DIRTY 0x09 /* light red */
#define COLOR_CLEAN 0x0D /* purple */

#define COLOR_TABLE_ADDR 0x2000
#define PATTERN_TABLE_ADDR 0x0000
#define NAME_TABLE_ADDR 0x1800
#define SPRITE_ATTR_ADDR 0x1B00

#define LIFE_REGION_W (CELL_W * SCALE)
#define LIFE_REGION_H (CELL_H * SCALE)
/* Align to tile boundaries (8px) and 16px for word-wide updates. */
#define LIFE_REGION_X0 (((SCREEN_W - LIFE_REGION_W) / 2) & ~15)
#define LIFE_REGION_Y0 (((SCREEN_H - LIFE_REGION_H) / 2) & ~7)
#define LIFE_REGION_X1 (LIFE_REGION_X0 + LIFE_REGION_W - 1)
#define LIFE_REGION_Y1 (LIFE_REGION_Y0 + LIFE_REGION_H - 1)

#define LIFE_BORDER_X (LIFE_REGION_W < SCREEN_W)
#define LIFE_BORDER_Y (LIFE_REGION_H < SCREEN_H)
#ifndef LIFE_DRAW_BORDER
#define LIFE_DRAW_BORDER (LIFE_BORDER_X || LIFE_BORDER_Y)
#endif

#if (CELL_W & 15)
#error "CELL_W must be a multiple of 16 cells for word-wide updates."
#endif
#if (CELL_H & 3)
#error "CELL_H must be a multiple of 4 cells (tile height)."
#endif

/* The assembly processes eight words; the renderer streams full screen rows. */
_Static_assert(CELL_W == 128 && CELL_H == 96 && SCALE == 2 &&
               LIFE_REGION_X0 == 0 && LIFE_REGION_Y0 == 0,
               "Life3 requires a full-screen 128x96 board at 2x scale.");

/* Block acceptance uses an 8-bit threshold, not a percentage: 70/256
 * (about 27%) before pixel dropout. Z/X/C/V/B select 70/60/50/40/30. */
#define LIFE_SEED_THRESHOLD 70
#define LIFE_SEED_BLOCK_SIZE 4
#define LIFE_SEED_EDGE_BLOCKS 1
#define LIFE_SEED_DROPOUT_PCT 20
#define LIFE_SEED_DROPOUT_THRESHOLD ((uint8_t)((LIFE_SEED_DROPOUT_PCT * 256) / 100))
/* N sprinkles about 3% of the board; repeated/occupied picks are harmless. */
#define LIFE_NOISE_PICKS ((CELL_W * CELL_H) / 32)
#define LIFE_BLOB_MIN_RADIUS 12
#define LIFE_BLOB_RADIUS_RANGE 12 /* Radius 12-23: diameter 25-47 cells. */
#define LIFE_GLIDER_HALO 6        /* Dead margin cleared around G/S spawns. */
#define LIFE_SCENE_COUNT 6

#define LIFE_TILES_W (CELL_W / 4)
#define LIFE_TILES_H (CELL_H / 4)

#define LIFE_DEBUG_DIRTY 1

/* ===== VDP & PSG ACCESS ===== */
#define VDP_OUT(port, val) do { \
    uint16_t out = (uint16_t)(val) << 8; \
    asm volatile("MOVB %0, @%1" : : "r"(out), "i"(port) : "memory", "cc"); \
} while (0)

static void __attribute__((cold, noinline, minsize)) psg_mute(void) {
    /* LIMI 0 stops the console ISR, so mute any leftover menu beep tone. */
    VDP_OUT(PSG_PORT, 0x9Fu);
    VDP_OUT(PSG_PORT, 0xBFu);
    VDP_OUT(PSG_PORT, 0xDFu);
    VDP_OUT(PSG_PORT, 0xFFu);
}

static void vdp_cmd(uint8_t val)  { VDP_OUT(VDP_CMD_PORT, val); }
static void vdp_data(uint8_t val) { VDP_OUT(VDP_DATA_PORT, val); }

static void __attribute__((noinline, minsize))
vdp_set_register(uint8_t reg, uint8_t val) {
    vdp_cmd(val);
    vdp_cmd(reg | 0x80);
}

static void __attribute__((noinline, minsize))
vdp_set_write_addr(uint16_t addr) {
    vdp_cmd(addr & 0xFF);
    vdp_cmd(((addr >> 8) & 0x3F) | 0x40);
}

static void __attribute__((cold, noinline, minsize)) vdp_init_gfx2(void) {
    static const uint8_t regs[8] = {0x02, 0x80, 0x06, 0xFF, 0x03, 0x36, 0x07, 0x01};
    /* Preserve exact register write order expected by splash_tests: 1,0,2,3,4,5,6,7 */
    vdp_set_register(1, regs[1]);
    vdp_set_register(0, regs[0]);
    for (uint8_t r = 2; r < 8; ++r) {
        vdp_set_register(r, regs[r]);
    }
}

static void vdp_display_on(void) {
    /* TI console ROM KSCAN (0x000E) writes @0x83D4 to VDP R1 on keypress! */
    *(volatile uint8_t *)0x83D4 = 0xE0u;
    vdp_set_register(1, 0xE0);
}

static void __attribute__((cold, noinline, minsize)) vdp_init_name_table(void) {
    uint16_t i;

    vdp_set_write_addr(NAME_TABLE_ADDR);
    for (i = 0; i < CHAR_ROWS * CHAR_COLS; i++) {
        vdp_data((uint8_t)(i & 0xFF));
    }
}

static void __attribute__((cold, noinline, minsize)) vdp_init_color_table(void) {
    uint16_t i;
    uint8_t color = (uint8_t)((COLOR_FG << 4) | (COLOR_BG & 0x0F));

    vdp_set_write_addr(COLOR_TABLE_ADDR);
    for (i = 0; i < SCREEN_H * CHAR_COLS; i++) {
        vdp_data(color);
    }
}

static void __attribute__((noinline, minsize))
vdp_set_tile_color(uint16_t tile_row, uint16_t tile_col, uint8_t color) {
    uint16_t group = (uint16_t)(tile_row >> 3);
    uint16_t pattern = (uint16_t)(((tile_row & 7u) << 5) | tile_col);
    uint16_t addr = (uint16_t)(COLOR_TABLE_ADDR + group * 0x0800 + (pattern << 3));
    uint16_t i;

    vdp_set_write_addr(addr);
    for (i = 0; i < 8; i++) {
        vdp_data(color);
    }
}

static void vdp_apply_border_colors(void) {
    if (!LIFE_DRAW_BORDER) {
        return;
    }

    uint16_t tile_x_left = 0;
    uint16_t tile_x_right = (uint16_t)(CHAR_COLS - 1);
    uint16_t tile_y_top = 0;
    uint16_t tile_y_bottom = (uint16_t)(CHAR_ROWS - 1);
    uint16_t tile_x;
    uint16_t tile_y;
    uint8_t color = (uint8_t)((COLOR_BORDER << 4) | (COLOR_BG & 0x0F));

    if (LIFE_BORDER_X) {
        tile_x_left = (uint16_t)((LIFE_REGION_X0 - 1) >> 3);
        tile_x_right = (uint16_t)((LIFE_REGION_X1 + 1) >> 3);
    }

    if (LIFE_BORDER_Y) {
        tile_y_top = (uint16_t)((LIFE_REGION_Y0 - 1) >> 3);
        tile_y_bottom = (uint16_t)((LIFE_REGION_Y1 + 1) >> 3);
    }

    if (LIFE_BORDER_Y) {
        for (tile_x = tile_x_left; tile_x <= tile_x_right; tile_x++) {
            vdp_set_tile_color(tile_y_top, tile_x, color);
            vdp_set_tile_color(tile_y_bottom, tile_x, color);
        }
    }

    if (LIFE_BORDER_X) {
        for (tile_y = tile_y_top; tile_y <= tile_y_bottom; tile_y++) {
            vdp_set_tile_color(tile_y, tile_x_left, color);
            vdp_set_tile_color(tile_y, tile_x_right, color);
        }
    }
}

static void __attribute__((cold, noinline, minsize)) vdp_clear_pattern_table(void) {
    uint16_t i;

    vdp_set_write_addr(PATTERN_TABLE_ADDR);
    for (i = 0; i < 0x1800; i++) {
        vdp_data(0);
    }
}

static void vdp_disable_sprites(void) {
    vdp_set_write_addr(SPRITE_ATTR_ADDR);
    vdp_data(0xD0);
}

/* The splash reuses the VDP helpers but keeps its font and layout separate. */
#include "life3_splash.h"

/* ===== RNG ===== */
static uint16_t rng_state = 0x1D2Bu;

static uint16_t __attribute__((noinline, minsize)) rng_next(void) {
    uint16_t lsb = (uint16_t)(rng_state & 1u);
    rng_state >>= 1;
    if (lsb) {
        rng_state ^= 0xB400u;
    }
    return rng_state;
}

/* Rejection sampling works for non-power-of-two dimensions such as 96 rows.
 * Callers supply a nonzero bound no greater than 256. */
static uint16_t __attribute__((noinline, minsize)) rng_below(uint16_t bound) {
    uint16_t mask = 1;
    uint16_t value;
    while (mask < bound) {
        mask <<= 1;
    }
    --mask;
    do {
        value = (uint16_t)(rng_next() & mask);
    } while (value >= bound);
    return value;
}

/* Consecutive LFSR states share seven low-byte bits. Advance a full byte for
 * interactive noise so nearby picks/dropouts do not inherit that overlap. */
static uint8_t __attribute__((noinline, minsize)) rng_byte(void) {
    uint16_t i;
    for (i = 0; i < 8; ++i) {
        rng_next();
    }
    return (uint8_t)rng_state;
}

static uint16_t __attribute__((noinline, minsize)) random_below(uint16_t bound) {
    uint16_t mask = 1;
    uint16_t value;
    while (mask < bound) {
        mask <<= 1;
    }
    --mask;
    do {
        value = (uint16_t)(rng_byte() & mask);
    } while (value >= bound);
    return value;
}

/* ===== BOARD ===== */
static uint8_t board_a[CELL_H][CELL_ROW_BYTES] __attribute__((aligned(2)));
static uint8_t board_b[CELL_H][CELL_ROW_BYTES] __attribute__((aligned(2)));
static uint8_t (*cur)[CELL_ROW_BYTES] = board_a;
static uint8_t (*next)[CELL_ROW_BYTES] = board_b;
typedef uint16_t u16_alias __attribute__((__may_alias__));

/* Updates already cover entire tile rows, so activity needs one byte per row.
 * Dirty flags stay per tile for D/F visual debugging and row writeback. */
uint8_t active_rows[LIFE_TILES_H] __attribute__((aligned(2)));
uint8_t dirty_tiles[LIFE_TILES_H][LIFE_TILES_W] __attribute__((aligned(2)));
#if LIFE_DEBUG_DIRTY
static uint8_t dirty_prev[LIFE_TILES_H][LIFE_TILES_W];
static uint8_t debug_dirty_enabled;
static uint8_t debug_snapshot_pending; /* clear purple after one frame */
static uint8_t debug_key_prev;
static uint16_t entropy;
static uint16_t frame_ticks;
static uint8_t scene_index;
static uint16_t seed_threshold = LIFE_SEED_THRESHOLD;
static volatile uint8_t * const kbd_device = (volatile uint8_t *)0x8374;
#endif

static const uint8_t bit_mask[8] = {
    0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u
};

/* Each entry doubles eight source pixels into two bytes. High byte is the
 * left tile, low byte the right tile; initialized once in `life3_2x_step.S`. */
uint16_t expand8[256];

/* Any live cell can affect its own tile row or a neighboring row next time.
 * Horizontal dilation is implicit: an active row computes all 128 cells. */
static void __attribute__((noinline, minsize))
activate_row_and_neighbors(uint16_t ty) {
    active_rows[ty ? ty - 1 : LIFE_TILES_H - 1] = 1;
    active_rows[ty] = 1;
    active_rows[ty + 1 == LIFE_TILES_H ? 0 : ty + 1] = 1;
}

static void __attribute__((noinline, minsize))
build_active_rows(uint8_t (*src)[CELL_ROW_BYTES]) {
    uint16_t ty;
    for (ty = 0; ty < LIFE_TILES_H; ++ty) {
        active_rows[ty] = 0;
    }
    for (ty = 0; ty < LIFE_TILES_H; ++ty) {
        uint16_t any = 0;
        uint16_t row;
        for (row = 0; row < 4; ++row) {
            u16_alias *words = (u16_alias *)&src[ty * 4 + row][0];
            uint16_t wx;
            for (wx = 0; wx < CELL_WORDS; ++wx) {
                any |= words[wx];
            }
        }
        if (any) {
            activate_row_and_neighbors(ty);
        }
    }
}

static void __attribute__((noinline, minsize))
clear_board(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t y;
    uint16_t x;
    for (y = 0; y < CELL_H; y++) {
        for (x = 0; x < CELL_ROW_BYTES; x++) {
            b[y][x] = 0;
        }
    }
}

static void set_cell(uint8_t (*b)[CELL_ROW_BYTES], uint16_t x, uint16_t y) {
    b[y][x >> 3] |= bit_mask[x & 7];
}

/* Share the cold bounds/dropout code across all block origins to conserve ROM. */
static void __attribute__((noinline, minsize))
seed_block(uint8_t (*b)[CELL_ROW_BYTES], uint16_t x0, uint16_t y0,
                       uint8_t dropout_threshold) {
    uint16_t x;
    uint16_t y;

    for (y = 0; y < LIFE_SEED_BLOCK_SIZE; y++) {
        for (x = 0; x < LIFE_SEED_BLOCK_SIZE; x++) {
            if (dropout_threshold && (uint8_t)rng_next() < dropout_threshold) {
                continue;
            }
            /* Wrap every cell, not just the block's origin. */
            uint16_t cell_x = (uint16_t)(x0 + x);
            uint16_t cell_y = (uint16_t)(y0 + y);
            if (cell_x >= CELL_W) cell_x -= CELL_W;
            if (cell_y >= CELL_H) cell_y -= CELL_H;
            set_cell(b, cell_x, cell_y);
        }
    }
}

/* Assembly-optimized row processor in life3_2x_step.S.
 * Processes one full row (CELL_WORDS words) with sliding window + CSA.
 * Input: pointers to prev/cur/next source rows and destination row.
 */
extern void life_compute_row(uint16_t *row_prev, uint16_t *row,
                              uint16_t *row_next, uint16_t *row_dst);

static void __attribute__((noinline, minsize))
seed_board(uint8_t (*b)[CELL_ROW_BYTES], uint8_t threshold) {
    uint16_t x;
    uint16_t y;
    uint16_t x_offset = (uint16_t)(rng_next() & (LIFE_SEED_BLOCK_SIZE - 1));
    uint16_t y_offset = (uint16_t)(rng_next() & (LIFE_SEED_BLOCK_SIZE - 1));

    clear_board(b);

    for (y = 0; y < CELL_H; y += LIFE_SEED_BLOCK_SIZE) {
        uint16_t y_pos = (uint16_t)(y + y_offset);
        if (y_pos >= CELL_H) {
            y_pos = (uint16_t)(y_pos - CELL_H);
        }
        for (x = 0; x < CELL_W; x += LIFE_SEED_BLOCK_SIZE) {
            uint16_t x_pos = (uint16_t)(x + x_offset);
            if (x_pos >= CELL_W) {
                x_pos = (uint16_t)(x_pos - CELL_W);
            }
            if ((uint8_t)rng_next() < threshold) {
                seed_block(b, x_pos, y_pos, LIFE_SEED_DROPOUT_THRESHOLD);
            }
        }
    }

#if LIFE_SEED_EDGE_BLOCKS
    {
        uint16_t mask = (uint16_t)(~(LIFE_SEED_BLOCK_SIZE - 1));
        uint16_t edge_x = (uint16_t)(rng_below(CELL_W) & mask);
        uint16_t edge_y = (uint16_t)(rng_below(CELL_H) & mask);
        seed_block(b, 0, edge_y, LIFE_SEED_DROPOUT_THRESHOLD);
        seed_block(b, (uint16_t)(CELL_W - LIFE_SEED_BLOCK_SIZE), edge_y,
                   LIFE_SEED_DROPOUT_THRESHOLD);
        seed_block(b, edge_x, 0, LIFE_SEED_DROPOUT_THRESHOLD);
        seed_block(b, edge_x, (uint16_t)(CELL_H - LIFE_SEED_BLOCK_SIZE),
                   LIFE_SEED_DROPOUT_THRESHOLD);
    }
#endif

}

/* Full-board separable 3-row PAD CSA step & VDP streamer in life3_2x_step.S */
extern void step_board(uint8_t (*src)[CELL_ROW_BYTES],
                       uint8_t (*dst)[CELL_ROW_BYTES]);
extern void vdp_write_dirty_patterns(uint8_t (*b)[CELL_ROW_BYTES]);

static void vdp_write_border_tile(uint16_t tile_row, uint16_t tile_col,
                                  uint8_t row_start, uint8_t row_end,
                                  uint8_t col_mask,
                                  uint8_t horiz_full,
                                  uint8_t vert_full) {
    uint16_t group = (uint16_t)(tile_row >> 3);
    uint16_t pattern_base = (uint16_t)((tile_row & 7u) << 5);
    uint16_t pattern = (uint16_t)(pattern_base | tile_col);
    uint16_t addr = (uint16_t)(PATTERN_TABLE_ADDR + (group * 0x0800) + (pattern << 3));
    uint16_t row;

    vdp_set_write_addr(addr);
    for (row = 0; row < 8; row++) {
        uint8_t val = 0x00;
        if (row >= row_start && row <= row_end) {
            val = horiz_full ? 0xFF : col_mask;
        }
        if (vert_full) {
            val |= col_mask;
        }
        vdp_data(val);
    }
}

static void __attribute__((noinline, minsize))
mark_all_dirty(void) {
    uint16_t ty;
    uint16_t tx;

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            dirty_tiles[ty][tx] = 1;
        }
    }
}

static void vdp_write_border_tiles(void) {
    if (!LIFE_DRAW_BORDER) {
        return;
    }

    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_row_end = (uint16_t)(LIFE_REGION_Y1 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t tile_col_end = (uint16_t)(LIFE_REGION_X1 >> 3);
    uint16_t tile_x;
    uint16_t tile_y;

    if (LIFE_BORDER_Y) {
        uint16_t tile_y_top = (uint16_t)(tile_row_start - 1);
        uint16_t tile_y_bottom = (uint16_t)(tile_row_end + 1);
        uint8_t top_row_start = 6;
        uint8_t top_row_end = 7;
        uint8_t bottom_row_start = 0;
        uint8_t bottom_row_end = 1;

        for (tile_x = tile_col_start; tile_x <= tile_col_end; tile_x++) {
            vdp_write_border_tile(tile_y_top, tile_x,
                                  top_row_start, top_row_end,
                                  0, 1, 0);
            vdp_write_border_tile(tile_y_bottom, tile_x,
                                  bottom_row_start, bottom_row_end,
                                  0, 1, 0);
        }

        if (LIFE_BORDER_X) {
            uint16_t tile_x_left = (uint16_t)(tile_col_start - 1);
            uint16_t tile_x_right = (uint16_t)(tile_col_end + 1);

            vdp_write_border_tile(tile_y_top, tile_x_left,
                                  top_row_start, top_row_end,
                                  0x03, 0, 0);
            vdp_write_border_tile(tile_y_top, tile_x_right,
                                  top_row_start, top_row_end,
                                  0xC0, 0, 0);
            vdp_write_border_tile(tile_y_bottom, tile_x_left,
                                  bottom_row_start, bottom_row_end,
                                  0x03, 0, 0);
            vdp_write_border_tile(tile_y_bottom, tile_x_right,
                                  bottom_row_start, bottom_row_end,
                                  0xC0, 0, 0);
        }
    }

    if (LIFE_BORDER_X) {
        uint16_t tile_x_left = (uint16_t)(tile_col_start - 1);
        uint16_t tile_x_right = (uint16_t)(tile_col_end + 1);

        for (tile_y = tile_row_start; tile_y <= tile_row_end; tile_y++) {
            vdp_write_border_tile(tile_y, tile_x_left,
                                  8, 7,
                                  0x03, 0, 1);
            vdp_write_border_tile(tile_y, tile_x_right,
                                  8, 7,
                                  0xC0, 0, 1);
        }
    }
}

#if LIFE_DEBUG_DIRTY
/* Add cells to the current generation without clearing the existing pattern.
 * Update drawing and next-generation activity only where a dead cell changes. */
static void __attribute__((noinline, minsize))
add_live_cell(uint8_t (*b)[CELL_ROW_BYTES],
              uint16_t x, uint16_t y) {
    uint8_t mask = bit_mask[x & 7];
    if (!(b[y][x >> 3] & mask)) {
        b[y][x >> 3] |= mask;
        dirty_tiles[y >> 2][x >> 2] = 1;
        activate_row_and_neighbors(y >> 2);
    }
}

static void __attribute__((noinline, minsize))
add_noise(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t i;
    for (i = 0; i < LIFE_NOISE_PICKS; ++i) {
        uint16_t x = random_below(CELL_W);
        uint16_t y = random_below(CELL_H);
        add_live_cell(b, x, y);
    }
}

/* A large, half-filled disk disturbs a substantial area without imposing the
 * startup block lattice. Squared distances stay below 1059, so 16-bit integer
 * arithmetic suffices; no floating point or square root is needed. */
static void __attribute__((noinline, minsize))
add_blob(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t x0 = random_below(CELL_W);
    uint16_t y0 = random_below(CELL_H);
    int16_t radius = (int16_t)(LIFE_BLOB_MIN_RADIUS + random_below(LIFE_BLOB_RADIUS_RANGE));
    uint16_t radius_squared = (uint16_t)(radius * radius);
    int16_t dy;
    int16_t dx;

    for (dy = -radius; dy <= radius; ++dy) {
        uint16_t dy_squared = (uint16_t)(dy * dy);
        int16_t y = (int16_t)y0 + dy;
        if (y < 0) y += CELL_H;
        if (y >= CELL_H) y -= CELL_H;
        for (dx = -radius; dx <= radius; ++dx) {
            if ((uint16_t)(dx * dx) + dy_squared > radius_squared) {
                continue;
            }
            if (rng_byte() & 0x80u) {
                int16_t x = (int16_t)x0 + dx;
                if (x < 0) x += CELL_W;
                if (x >= CELL_W) x -= CELL_W;
                add_live_cell(b, (uint16_t)x, (uint16_t)y);
            }
        }
    }
}

static void __attribute__((noinline, minsize))
clear_live_cell(uint8_t (*b)[CELL_ROW_BYTES], uint16_t x, uint16_t y) {
    uint8_t mask = bit_mask[x & 7];
    if (b[y][x >> 3] & mask) {
        b[y][x >> 3] &= (uint8_t)~mask;
        dirty_tiles[y >> 2][x >> 2] = 1;
    }
}

enum life_pattern_id {
    PAT_GLIDER = 0,
    PAT_LWSS,
    PAT_MWSS,
    PAT_HWSS,
    PAT_BLOCK,
    PAT_EATER1,
    PAT_GUN_L,
    PAT_GUN_R,
    PAT_BHEPT,
    PAT_PULSAR_Q,
    PAT_GALAXY_BAR,
    PAT_PENTA_H,
    PAT_QBEE,
    PAT_ACORN,
};

/* Flat reloc-free 8-bit pattern dictionary: {w, h, row0..row_{h-1}}. */
static const uint8_t pat_blob[] = {
    3, 3, 0x02, 0x01, 0x07,                                     /* 0: Glider */
    5, 4, 0x09, 0x10, 0x11, 0x1E,                               /* 1: LWSS */
    6, 5, 0x04, 0x11, 0x20, 0x21, 0x3E,                         /* 2: MWSS */
    7, 5, 0x0C, 0x21, 0x40, 0x41, 0x7E,                         /* 3: HWSS */
    2, 2, 0x03, 0x03,                                           /* 4: Block */
    4, 4, 0x0C, 0x0A, 0x02, 0x03,                               /* 5: Eater 1 */
    8, 7, 0x30, 0x44, 0x82, 0x8B, 0x82, 0x44, 0x30,             /* 6: Gosper L */
    5, 7, 0x01, 0x05, 0x18, 0x18, 0x18, 0x05, 0x01,             /* 7: Gosper R */
    4, 3, 0x0B, 0x0E, 0x04,                                     /* 8: B-heptomino */
    6, 6, 0x0E, 0x00, 0x21, 0x21, 0x21, 0x0E,                   /* 9: Pulsar quad */
    6, 2, 0x3F, 0x3F,                                           /* 10: Galaxy 6x2 bar */
    5, 3, 0x04, 0x1B, 0x04,                                     /* 11: Pentadecathlon half */
    5, 7, 0x01, 0x05, 0x0A, 0x12, 0x0A, 0x05, 0x01,             /* 12: Queen Bee */
    7, 3, 0x20, 0x08, 0x67,                                     /* 13: Acorn */
};

static uint16_t __attribute__((noinline, minsize))
wrap_y(int16_t y) {
    if (y < 0) y += CELL_H;
    if (y >= CELL_H) y -= CELL_H;
    return (uint16_t)y;
}

/* Stamp a pattern under any D4 orientation (xform bits: 0=flip_x, 1=flip_y,
 * 2=transpose). If halo > 0, clears a toroidal margin around the bounding box
 * first so newly spawned gliders/spaceships survive even on dense boards. */
static void __attribute__((noinline, minsize))
stamp_pattern(uint8_t (*b)[CELL_ROW_BYTES], uint8_t pat_id,
              uint16_t x0, uint16_t y0, uint8_t xform, int16_t halo) {
    const uint8_t *p = pat_blob;
    while (pat_id--) {
        p += 2u + p[1];
    }
    int16_t w = (int16_t)p[0];
    int16_t h = (int16_t)p[1];
    const uint8_t *rows = p + 2;
    int16_t tw = (xform & 4u) ? h : w;
    int16_t th = (xform & 4u) ? w : h;
    int16_t py;
    int16_t px;

    if (halo > 0) {
        int16_t dy;
        int16_t dx;
        for (dy = (int16_t)-halo; dy < (int16_t)(th + halo); ++dy) {
            uint16_t cy = wrap_y((int16_t)y0 + dy);
            for (dx = (int16_t)-halo; dx < (int16_t)(tw + halo); ++dx) {
                clear_live_cell(b, (uint16_t)((x0 + (uint16_t)dx) & (CELL_W - 1u)), cy);
            }
        }
    }

    for (py = 0; py < h; ++py) {
        uint8_t row = rows[py];
        for (px = 0; px < w; ++px) {
            if (row & (uint8_t)(1u << (w - 1 - px))) {
                int16_t dx = (xform & 1u) ? (int16_t)(w - 1 - px) : px;
                int16_t dy = (xform & 2u) ? (int16_t)(h - 1 - py) : py;
                if (xform & 4u) {
                    int16_t tmp = dx;
                    dx = dy;
                    dy = tmp;
                }
                add_live_cell(b, (uint16_t)((x0 + (uint16_t)dx) & (CELL_W - 1u)),
                              wrap_y((int16_t)y0 + dy));
            }
        }
    }
    if (halo > 0) {
        build_active_rows(b);
    }
}

static void __attribute__((noinline, minsize))
add_glider(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t x0 = random_below(CELL_W);
    uint16_t y0 = random_below(CELL_H);
    uint8_t xform = (uint8_t)(rng_byte() & 7u);
    stamp_pattern(b, PAT_GLIDER, x0, y0, xform, LIFE_GLIDER_HALO);
}

static void __attribute__((noinline, minsize))
add_spaceship(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t x0 = random_below(CELL_W);
    uint16_t y0 = random_below(CELL_H);
    uint8_t kind = (uint8_t)random_below(3); /* 0=LWSS, 1=MWSS, 2=HWSS */
    uint8_t xform = (uint8_t)(rng_byte() & 7u);
    stamp_pattern(b, (uint8_t)(PAT_LWSS + kind), x0, y0, xform, LIFE_GLIDER_HALO);
}

static const uint8_t scene_stream[] = {
    /* Scene 0: Gosper Gun (10,10) + Distant Eater 1 (72,58) */
    PAT_BLOCK, 10, 14, 0,  PAT_GUN_L, 20, 12, 0,
    PAT_GUN_R, 30, 10, 0,  PAT_BLOCK, 44, 12, 0,
    PAT_EATER1, 72, 58, 0,
    0xFFu,

    /* Scene 1: Twin Crossfire Gosper Guns — (12,16) SE vs (73,14) SW */
    PAT_BLOCK, 12, 20, 0,  PAT_GUN_L, 22, 18, 0,
    PAT_GUN_R, 32, 16, 0,  PAT_BLOCK, 46, 18, 0,
    PAT_BLOCK, 107, 18, 1, PAT_GUN_L, 91, 16, 1,
    PAT_GUN_R, 84, 14, 1,  PAT_BLOCK, 73, 16, 1,
    0xFFu,

    /* Scene 2: Conway's B-Heptomino Puffer 1 */
    PAT_HWSS, 16, 40, 0,   PAT_HWSS, 16, 51, 2,   PAT_BHEPT, 13, 45, 5,
    0xFFu,

    /* Scene 3: Toroidal Spaceship Armada */
    PAT_HWSS, 10, 10, 0,   PAT_MWSS, 54, 10, 0,   PAT_LWSS,  96, 10, 0,
    PAT_HWSS, 28, 28, 1,   PAT_MWSS, 72, 28, 1,   PAT_LWSS, 112, 28, 1,
    PAT_HWSS, 18, 46, 0,   PAT_HWSS, 62, 46, 0,   PAT_MWSS, 104, 46, 0,
    PAT_HWSS, 36, 64, 1,   PAT_MWSS, 80, 64, 1,   PAT_LWSS, 116, 64, 1,
    PAT_MWSS, 14, 82, 0,   PAT_LWSS, 58, 82, 0,   PAT_HWSS, 100, 82, 0,
    0xFFu,

    /* Scene 4: Clockwork Oscillator Garden */
    PAT_PULSAR_Q,   16, 14, 0,  PAT_PULSAR_Q,   23, 14, 1,
    PAT_PULSAR_Q,   16, 21, 2,  PAT_PULSAR_Q,   23, 21, 3,
    PAT_PULSAR_Q,   99, 69, 0,  PAT_PULSAR_Q,  106, 69, 1,
    PAT_PULSAR_Q,   99, 76, 2,  PAT_PULSAR_Q,  106, 76, 3,
    PAT_GALAXY_BAR, 104, 16, 0, PAT_GALAXY_BAR, 101, 16, 4,
    PAT_GALAXY_BAR, 101, 23, 0, PAT_GALAXY_BAR, 108, 19, 4,
    PAT_GALAXY_BAR,  21, 71, 0, PAT_GALAXY_BAR,  18, 71, 4,
    PAT_GALAXY_BAR,  18, 78, 0, PAT_GALAXY_BAR,  25, 74, 4,
    PAT_PENTA_H,    59, 17, 0,  PAT_PENTA_H,    64, 17, 1,
    PAT_PENTA_H,    59, 76, 0,  PAT_PENTA_H,    64, 76, 1,
    PAT_BLOCK,      53, 47, 0,  PAT_QBEE,       58, 44, 0,  PAT_BLOCK, 73, 47, 0,
    0xFFu,

    /* Scene 5: Acorn Methuselah */
    PAT_ACORN, 60, 46, 0,
    0xFFu,
};

static void __attribute__((noinline, minsize))
wipe_board(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t ty;
    clear_board(b);
    for (ty = 0; ty < LIFE_TILES_H; ++ty) {
        active_rows[ty] = 0;
    }
    mark_all_dirty();
}

static void __attribute__((noinline, minsize))
load_scene(uint8_t (*b)[CELL_ROW_BYTES], uint8_t target_scene) {
    const uint8_t *p = scene_stream;
    uint8_t cur_scene = 0;

    wipe_board(b);
    while (cur_scene < target_scene) {
        if (*p++ == 0xFFu) {
            ++cur_scene;
        }
    }
    while (*p != 0xFFu) {
        stamp_pattern(b, p[0], p[1], p[2], p[3], 0);
        p += 4;
    }
}

static void __attribute__((noinline, minsize))
next_scene(uint8_t (*b)[CELL_ROW_BYTES]) {
    load_scene(b, scene_index);
    scene_index = (uint8_t)((scene_index + 1u == LIFE_SCENE_COUNT) ? 0u : (scene_index + 1u));
}

static void __attribute__((noinline, minsize))
vdp_debug_dirty_update(void) {
    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t ty;
    uint16_t tx;
    uint8_t color_normal = (uint8_t)((COLOR_FG << 4) | (COLOR_BG & 0x0F));
    uint8_t color_dirty = (uint8_t)((COLOR_DIRTY << 4) | (COLOR_BG & 0x0F));

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            uint8_t was_dirty = dirty_prev[ty][tx];
            uint8_t is_dirty = dirty_tiles[ty][tx];
            if (is_dirty && !was_dirty) {
                vdp_set_tile_color((uint16_t)(tile_row_start + ty),
                                   (uint16_t)(tile_col_start + tx),
                                   color_dirty);
            } else if (!is_dirty && was_dirty) {
                vdp_set_tile_color((uint16_t)(tile_row_start + ty),
                                   (uint16_t)(tile_col_start + tx),
                                   color_normal);
            }
            dirty_prev[ty][tx] = is_dirty;
        }
    }
}

static void __attribute__((noinline, minsize))
vdp_debug_dirty_clear(void) {
    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t ty;
    uint16_t tx;
    uint8_t color_normal = (uint8_t)((COLOR_FG << 4) | (COLOR_BG & 0x0F));

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            if (dirty_prev[ty][tx]) {
                vdp_set_tile_color((uint16_t)(tile_row_start + ty),
                                   (uint16_t)(tile_col_start + tx),
                                   color_normal);
                dirty_prev[ty][tx] = 0;
            }
        }
    }
}

static void __attribute__((noinline, minsize))
vdp_debug_clean_snapshot(void) {
    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t ty;
    uint16_t tx;
    uint8_t color_clean = (uint8_t)((COLOR_CLEAN << 4) | (COLOR_BG & 0x0F));

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            if (dirty_tiles[ty][tx]) {
                continue;
            }
            vdp_set_tile_color((uint16_t)(tile_row_start + ty),
                               (uint16_t)(tile_col_start + tx),
                               color_clean);
        }
    }
    debug_snapshot_pending = 1;
}

static void __attribute__((noinline, minsize))
vdp_debug_clean_snapshot_clear(void) {
    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t ty;
    uint16_t tx;
    uint8_t color_normal = (uint8_t)((COLOR_FG << 4) | (COLOR_BG & 0x0F));

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            vdp_set_tile_color((uint16_t)(tile_row_start + ty),
                               (uint16_t)(tile_col_start + tx),
                               color_normal);
            dirty_prev[ty][tx] = 0;
        }
    }
    debug_snapshot_pending = 0;
}

static void __attribute__((noinline, minsize))
reseed_board(uint8_t (*b)[CELL_ROW_BYTES]) {
    if (entropy != 0) {
        rng_state ^= entropy;
        if (rng_state == 0) {
            rng_state = 0x1D2Bu;
        }
    }
    seed_board(b, (uint8_t)seed_threshold);
    build_active_rows(b);
    mark_all_dirty();
}

static const uint8_t density_keys[5] = {'Z', 'X', 'C', 'V', 'B'};

static void __attribute__((cold, noinline, minsize))
enter_gfx2(uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t ty;
    uint16_t tx;

    vdp_init_gfx2();
    vdp_clear_pattern_table();
    vdp_init_name_table();
    vdp_init_color_table();
    if (LIFE_DRAW_BORDER) {
        vdp_apply_border_colors();
        vdp_write_border_tiles();
    }
    vdp_disable_sprites();
#if LIFE_DEBUG_DIRTY
    for (ty = 0; ty < LIFE_TILES_H; ++ty) {
        for (tx = 0; tx < LIFE_TILES_W; ++tx) {
            dirty_prev[ty][tx] = 0;
        }
    }
    debug_snapshot_pending = 0;
#endif
    mark_all_dirty();
    vdp_write_dirty_patterns(b);
    vdp_display_on();
}

static void __attribute__((cold, noinline, minsize))
open_premade_menu(uint8_t raw_key, uint8_t (*b)[CELL_ROW_BYTES]) {
    uint8_t held = raw_key;
    uint8_t pick = life_show_premade_menu(1, &held);
    if (pick == 'T') {
        pick = life_show_splash(&held);
        if (pick == ' ') {
            seed_board(b, (uint8_t)seed_threshold);
            build_active_rows(b);
        }
    }
    if (pick >= '1' && pick <= '6') {
        scene_index = (uint8_t)(pick - '1');
        next_scene(b);
    }
    debug_key_prev = held;
    enter_gfx2(b);
}

static void __attribute__((cold, noinline, minsize))
handle_key(uint8_t raw_key, uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t i;
    uint8_t key = raw_key;
    if (key >= 'a' && key <= 'z') {
        key = (uint8_t)(key - ('a' - 'A'));
    }
    if (key == 'D') {
        debug_dirty_enabled = (uint8_t)!debug_dirty_enabled;
        if (!debug_dirty_enabled) {
            vdp_debug_dirty_clear();
        }
    } else if (key == 'F') {
        vdp_debug_clean_snapshot();
    } else if (key == 'N') {
        add_noise(b);
    } else if (key == 'M') {
        add_blob(b);
    } else if (key == 'G') {
        add_glider(b);
    } else if (key == 'S') {
        add_spaceship(b);
    } else if (key == 'P') {
        open_premade_menu(raw_key, b);
    } else if (key == 'K') {
        wipe_board(b);
    } else if (key == 'R') {
        reseed_board(b);
    } else if (key >= '1' && key <= '9') {
        uint16_t add = (uint16_t)(key - '0');
        entropy = (uint16_t)(entropy + (add * 0x1111u));
        entropy ^= (uint16_t)((entropy << 5) | (entropy >> 11));
    } else {
        for (i = 0; i < 5; ++i) {
            if (key == density_keys[i]) {
                seed_threshold = (uint16_t)(70u - i * 10u);
                reseed_board(b);
                break;
            }
        }
    }
}
#endif

void __attribute__((minsize)) main(void) {
    uint8_t raw_key = 0;
    uint8_t pick;

    psg_mute();
    pick = life_show_splash(&raw_key);
#if LIFE_DEBUG_DIRTY
    debug_key_prev = raw_key;
    if (pick >= '1' && pick <= '6') {
        scene_index = (uint8_t)(pick - '1');
        next_scene(cur);
    } else
#endif
    {
        seed_board(cur, LIFE_SEED_THRESHOLD);
        build_active_rows(cur);
    }
    enter_gfx2(cur);

    for (;;) {
#if LIFE_DEBUG_DIRTY
        /* The F-key snapshot remains visible until the next generation. */
        if (debug_snapshot_pending) {
            vdp_debug_clean_snapshot_clear();
        }
#endif
        uint8_t (*tmp)[CELL_ROW_BYTES] = cur;
        cur = next;
        next = tmp;
        step_board(next, cur);
#if LIFE_DEBUG_DIRTY
        /* Under LIMI 0, console 0x8379 never ticks; mix generation count instead. */
        entropy = (uint16_t)(((entropy << 5) | (entropy >> 11)) ^ (++frame_ticks));
        *kbd_device = 0;
        {
            uint8_t key = (uint8_t)kbd_scan();
            uint8_t key_pressed = (uint8_t)(key != 0 && key != debug_key_prev);
            debug_key_prev = key;
            if (key_pressed) {
                handle_key(key, cur);
            }
        }
        if (debug_dirty_enabled) {
            vdp_debug_dirty_update();
        }
#endif
        vdp_write_dirty_patterns(cur);
    }
}
