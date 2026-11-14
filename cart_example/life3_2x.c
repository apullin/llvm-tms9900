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
#define LIFE_VERSION "0.2"
#define LIFE_DATE "OCT 2026"
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

#define LIFE_TILES_W (CELL_W / 4)
#define LIFE_TILES_H (CELL_H / 4)

#define LIFE_DEBUG_DIRTY 1

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
    vdp_set_register(1, 0x80); /* Display off, 16K VRAM */
    vdp_set_register(0, 0x02); /* Bitmap graphics mode */
    vdp_set_register(2, 0x06); /* Name table @ 0x1800 */
    vdp_set_register(3, 0xFF); /* Color table @ 0x2000 (bitmap mask) */
    vdp_set_register(4, 0x03); /* Pattern table @ 0x0000 (bitmap mask) */
    vdp_set_register(5, 0x36); /* Sprite attribute @ 0x1B00 */
    vdp_set_register(6, 0x07); /* Sprite pattern @ 0x3800 */
    vdp_set_register(7, 0x01); /* Backdrop black */
}

static void vdp_display_on(void) {
    vdp_set_register(1, 0xE0);
}

static void vdp_init_name_table(void) {
    uint16_t i;

    vdp_set_write_addr(NAME_TABLE_ADDR);
    for (i = 0; i < CHAR_ROWS * CHAR_COLS; i++) {
        vdp_data((uint8_t)(i & 0xFF));
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

static void vdp_set_tile_color(uint16_t tile_row, uint16_t tile_col, uint8_t color) {
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

static void vdp_clear_pattern_table(void) {
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

static uint16_t rng_next(void) {
    uint16_t lsb = (uint16_t)(rng_state & 1u);
    rng_state >>= 1;
    if (lsb) {
        rng_state ^= 0xB400u;
    }
    return rng_state;
}

/* Rejection sampling works for non-power-of-two dimensions such as 96 rows.
 * Callers supply a nonzero bound no greater than 256. */
static uint16_t rng_below(uint16_t bound) {
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
static uint8_t __attribute__((noinline)) rng_byte(void) {
    uint16_t i;
    for (i = 0; i < 8; ++i) {
        rng_next();
    }
    return (uint8_t)rng_state;
}

static uint16_t random_below(uint16_t bound) {
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
static uint8_t active_rows[LIFE_TILES_H];
static uint8_t dirty_tiles[LIFE_TILES_H][LIFE_TILES_W] __attribute__((aligned(2)));
#if LIFE_DEBUG_DIRTY
static uint8_t dirty_prev[LIFE_TILES_H][LIFE_TILES_W];
static uint8_t debug_dirty_enabled;
static uint8_t debug_snapshot_pending; /* clear purple after one frame */
static uint8_t debug_key_prev;
static uint16_t entropy;
static uint8_t seed_threshold = LIFE_SEED_THRESHOLD;
static volatile uint8_t * const kbd_device = (volatile uint8_t *)0x8374;
static volatile uint8_t * const vdp_counter = (volatile uint8_t *)0x8379;
static uint8_t frame_prev;
#endif

static const uint8_t bit_mask[8] = {
    0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u
};

/* Each entry doubles eight source pixels into two bytes. High byte is the
 * left tile, low byte the right tile; each scanline is then written twice. */
static const uint16_t expand8[256] = {
    0x0000, 0x0003, 0x000c, 0x000f, 0x0030, 0x0033, 0x003c, 0x003f,
    0x00c0, 0x00c3, 0x00cc, 0x00cf, 0x00f0, 0x00f3, 0x00fc, 0x00ff,
    0x0300, 0x0303, 0x030c, 0x030f, 0x0330, 0x0333, 0x033c, 0x033f,
    0x03c0, 0x03c3, 0x03cc, 0x03cf, 0x03f0, 0x03f3, 0x03fc, 0x03ff,
    0x0c00, 0x0c03, 0x0c0c, 0x0c0f, 0x0c30, 0x0c33, 0x0c3c, 0x0c3f,
    0x0cc0, 0x0cc3, 0x0ccc, 0x0ccf, 0x0cf0, 0x0cf3, 0x0cfc, 0x0cff,
    0x0f00, 0x0f03, 0x0f0c, 0x0f0f, 0x0f30, 0x0f33, 0x0f3c, 0x0f3f,
    0x0fc0, 0x0fc3, 0x0fcc, 0x0fcf, 0x0ff0, 0x0ff3, 0x0ffc, 0x0fff,
    0x3000, 0x3003, 0x300c, 0x300f, 0x3030, 0x3033, 0x303c, 0x303f,
    0x30c0, 0x30c3, 0x30cc, 0x30cf, 0x30f0, 0x30f3, 0x30fc, 0x30ff,
    0x3300, 0x3303, 0x330c, 0x330f, 0x3330, 0x3333, 0x333c, 0x333f,
    0x33c0, 0x33c3, 0x33cc, 0x33cf, 0x33f0, 0x33f3, 0x33fc, 0x33ff,
    0x3c00, 0x3c03, 0x3c0c, 0x3c0f, 0x3c30, 0x3c33, 0x3c3c, 0x3c3f,
    0x3cc0, 0x3cc3, 0x3ccc, 0x3ccf, 0x3cf0, 0x3cf3, 0x3cfc, 0x3cff,
    0x3f00, 0x3f03, 0x3f0c, 0x3f0f, 0x3f30, 0x3f33, 0x3f3c, 0x3f3f,
    0x3fc0, 0x3fc3, 0x3fcc, 0x3fcf, 0x3ff0, 0x3ff3, 0x3ffc, 0x3fff,
    0xc000, 0xc003, 0xc00c, 0xc00f, 0xc030, 0xc033, 0xc03c, 0xc03f,
    0xc0c0, 0xc0c3, 0xc0cc, 0xc0cf, 0xc0f0, 0xc0f3, 0xc0fc, 0xc0ff,
    0xc300, 0xc303, 0xc30c, 0xc30f, 0xc330, 0xc333, 0xc33c, 0xc33f,
    0xc3c0, 0xc3c3, 0xc3cc, 0xc3cf, 0xc3f0, 0xc3f3, 0xc3fc, 0xc3ff,
    0xcc00, 0xcc03, 0xcc0c, 0xcc0f, 0xcc30, 0xcc33, 0xcc3c, 0xcc3f,
    0xccc0, 0xccc3, 0xcccc, 0xcccf, 0xccf0, 0xccf3, 0xccfc, 0xccff,
    0xcf00, 0xcf03, 0xcf0c, 0xcf0f, 0xcf30, 0xcf33, 0xcf3c, 0xcf3f,
    0xcfc0, 0xcfc3, 0xcfcc, 0xcfcf, 0xcff0, 0xcff3, 0xcffc, 0xcfff,
    0xf000, 0xf003, 0xf00c, 0xf00f, 0xf030, 0xf033, 0xf03c, 0xf03f,
    0xf0c0, 0xf0c3, 0xf0cc, 0xf0cf, 0xf0f0, 0xf0f3, 0xf0fc, 0xf0ff,
    0xf300, 0xf303, 0xf30c, 0xf30f, 0xf330, 0xf333, 0xf33c, 0xf33f,
    0xf3c0, 0xf3c3, 0xf3cc, 0xf3cf, 0xf3f0, 0xf3f3, 0xf3fc, 0xf3ff,
    0xfc00, 0xfc03, 0xfc0c, 0xfc0f, 0xfc30, 0xfc33, 0xfc3c, 0xfc3f,
    0xfcc0, 0xfcc3, 0xfccc, 0xfccf, 0xfcf0, 0xfcf3, 0xfcfc, 0xfcff,
    0xff00, 0xff03, 0xff0c, 0xff0f, 0xff30, 0xff33, 0xff3c, 0xff3f,
    0xffc0, 0xffc3, 0xffcc, 0xffcf, 0xfff0, 0xfff3, 0xfffc, 0xffff,
};

static const uint8_t expand4[16] = {
    0x00, 0x03, 0x0C, 0x0F,
    0x30, 0x33, 0x3C, 0x3F,
    0xC0, 0xC3, 0xCC, 0xCF,
    0xF0, 0xF3, 0xFC, 0xFF
};

/* Any live cell can affect its own tile row or a neighboring row next time.
 * Horizontal dilation is implicit: an active row computes all 128 cells. */
static void activate_row_and_neighbors(uint16_t ty) {
    active_rows[ty ? ty - 1 : LIFE_TILES_H - 1] = 1;
    active_rows[ty] = 1;
    active_rows[ty + 1 == LIFE_TILES_H ? 0 : ty + 1] = 1;
}

static void build_active_rows(uint8_t (*src)[CELL_ROW_BYTES]) {
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

static void clear_board(uint8_t (*b)[CELL_ROW_BYTES]) {
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
static void __attribute__((noinline)) seed_block(uint8_t (*b)[CELL_ROW_BYTES], uint16_t x0, uint16_t y0,
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

static void seed_board(uint8_t (*b)[CELL_ROW_BYTES], uint8_t threshold) {
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

static void __attribute__((noinline)) step_board(uint8_t (*src)[CELL_ROW_BYTES],
                                                 uint8_t (*dst)[CELL_ROW_BYTES]) {
    /* Tracepoint label for step timing (tms9900-trace). */
    asm volatile(
        ".globl life_step_begin\n"
        "life_step_begin:\n"
        :
        :
        : "memory");
    uint16_t y;
    uint16_t ty;

    /* Clear dirty flags; each active destination row is fully overwritten. */
    {
        u16_alias *dp = (u16_alias *)&dirty_tiles[0][0];
        uint16_t n = (uint16_t)((LIFE_TILES_H * LIFE_TILES_W) / 2);
        uint16_t i;
        for (i = 0; i < n; i++) {
            dp[i] = 0;
        }
    }

    /* Compute four cell rows per active tile row. Clear skipped rows because
     * the other board buffer may still contain an older generation. */
    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        if (!active_rows[ty]) {
            u16_alias *words = (u16_alias *)&dst[ty * 4][0];
            uint16_t wx;
            for (wx = 0; wx < 4 * CELL_WORDS; ++wx) {
                words[wx] = 0;
            }
            continue;
        }

        uint16_t y_base = (uint16_t)(ty << 2);
        uint16_t dy;
        for (dy = 0; dy < 4; dy++) {
            y = y_base + dy;
            uint16_t y_prev = (y == 0) ? (CELL_H - 1) : (y - 1);
            uint16_t y_next = (y + 1 == CELL_H) ? 0 : (y + 1);
            life_compute_row((uint16_t *)&src[y_prev][0],
                             (uint16_t *)&src[y][0],
                             (uint16_t *)&src[y_next][0],
                             (uint16_t *)&dst[y][0]);
        }
    }

    /* Compare generations to build the dirty tile table. At the same time,
     * accumulate live words to prepare activity for the next generation. */
    {
        uint16_t ty2, wx2, row;
        for (ty2 = 0; ty2 < LIFE_TILES_H; ++ty2) {
            active_rows[ty2] = 0;
        }
        for (ty2 = 0; ty2 < LIFE_TILES_H; ty2++) {
            uint16_t y_base = (uint16_t)(ty2 << 2);
            uint16_t row_live = 0;
            for (wx2 = 0; wx2 < CELL_WORDS; wx2++) {
                uint16_t tile_x2 = (uint16_t)(wx2 << 2);
                uint16_t live_acc = 0;
                uint16_t dirty_acc = 0;
                for (row = 0; row < 4; row++) {
                    u16_alias *dw = (u16_alias *)&dst[y_base + row][0];
                    u16_alias *sw = (u16_alias *)&src[y_base + row][0];
                    live_acc |= dw[wx2];
                    dirty_acc |= (uint16_t)(dw[wx2] ^ sw[wx2]);
                }
                row_live |= live_acc;
                if (dirty_acc & 0xF000u)
                    dirty_tiles[ty2][tile_x2] = 1;
                if (dirty_acc & 0x0F00u)
                    dirty_tiles[ty2][(uint16_t)(tile_x2 + 1)] = 1;
                if (dirty_acc & 0x00F0u)
                    dirty_tiles[ty2][(uint16_t)(tile_x2 + 2)] = 1;
                if (dirty_acc & 0x000Fu)
                    dirty_tiles[ty2][(uint16_t)(tile_x2 + 3)] = 1;
            }
            if (row_live) {
                activate_row_and_neighbors(ty2);
            }
        }
    }

    /* Tracepoint label for step timing (tms9900-trace). */
    asm volatile(
        ".globl life_step_end\n"
        "life_step_end:\n"
        :
        :
        : "memory");
}

static void vdp_write_tile_scaled(uint16_t ty, uint16_t tx,
                                  uint16_t tile_row_start,
                                  uint16_t tile_col_start,
                                  uint8_t (*b)[CELL_ROW_BYTES]) {
    uint16_t tile_row = (uint16_t)(tile_row_start + ty);
    uint16_t tile_col = (uint16_t)(tile_col_start + tx);
    uint16_t group = (uint16_t)(tile_row >> 3);
    uint16_t pattern_base = (uint16_t)((tile_row & 7u) << 5);
    uint16_t pattern = (uint16_t)(pattern_base | tile_col);
    uint16_t addr = (uint16_t)(PATTERN_TABLE_ADDR + (group * 0x0800) + (pattern << 3));
    uint16_t cell_y = (uint16_t)(ty << 2);
    uint16_t cell_x = (uint16_t)(tx << 2);
    uint16_t byte_index = (uint16_t)(cell_x >> 3);
    uint16_t row;

    vdp_set_write_addr(addr);
    for (row = 0; row < 4; row++) {
        uint8_t v = b[cell_y + row][byte_index];
        uint8_t nibble = (cell_x & 4u) ? (uint8_t)(v & 0x0F) : (uint8_t)(v >> 4);
        uint8_t expanded = expand4[nibble];
        vdp_data(expanded);
        vdp_data(expanded);
    }
}

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

static void mark_all_dirty(void) {
    uint16_t ty;
    uint16_t tx;

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            dirty_tiles[ty][tx] = 1;
        }
    }
}

/* Keep the globally named trace labels unique when called from init and main. */
static void __attribute__((noinline)) vdp_write_dirty_patterns(uint8_t (*b)[CELL_ROW_BYTES]) {
    /* Tracepoint label for draw timing (tms9900-trace). */
    asm volatile(
        ".globl life_draw_begin\n"
        "life_draw_begin:\n"
        :
        :
        : "memory");

    /* Row-streaming VDP writes: set address once per character row,
     * stream all 32 tiles (256 contiguous VRAM bytes) sequentially.
     * Dirty check at row level — skip rows with no dirty tiles. */
    uint16_t vdp_addr = PATTERN_TABLE_ADDR;
    uint16_t ty;

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        {
            uint16_t row_dirty = 0;
            uint16_t i;
            for (i = 0; i < LIFE_TILES_W; i += 2) {
                row_dirty |= *(u16_alias *)&dirty_tiles[ty][i];
            }
            if (!row_dirty) {
                vdp_addr += 256;
                continue;
            }
        }

        vdp_set_write_addr(vdp_addr);

        {
            uint16_t cell_y = (uint16_t)(ty << 2);
            uint16_t byte_idx;
            for (byte_idx = 0; byte_idx < CELL_ROW_BYTES; byte_idx++) {
                /* Stream the left tile, then its adjacent right tile. */
                uint16_t e0 = expand8[b[cell_y + 0][byte_idx]];
                uint16_t e1 = expand8[b[cell_y + 1][byte_idx]];
                uint16_t e2 = expand8[b[cell_y + 2][byte_idx]];
                uint16_t e3 = expand8[b[cell_y + 3][byte_idx]];
                vdp_data((uint8_t)(e0 >> 8)); vdp_data((uint8_t)(e0 >> 8));
                vdp_data((uint8_t)(e1 >> 8)); vdp_data((uint8_t)(e1 >> 8));
                vdp_data((uint8_t)(e2 >> 8)); vdp_data((uint8_t)(e2 >> 8));
                vdp_data((uint8_t)(e3 >> 8)); vdp_data((uint8_t)(e3 >> 8));
                vdp_data((uint8_t)e0); vdp_data((uint8_t)e0);
                vdp_data((uint8_t)e1); vdp_data((uint8_t)e1);
                vdp_data((uint8_t)e2); vdp_data((uint8_t)e2);
                vdp_data((uint8_t)e3); vdp_data((uint8_t)e3);

            }
        }
        vdp_addr += 256;
    }

    /* Tracepoint label for draw timing (tms9900-trace). */
    asm volatile(
        ".globl life_draw_end\n"
        "life_draw_end:\n"
        :
        :
        : "memory");
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
static void __attribute__((noinline)) add_live_cell(uint8_t (*b)[CELL_ROW_BYTES],
                                                  uint16_t x, uint16_t y) {
    uint8_t mask = bit_mask[x & 7];
    if (!(b[y][x >> 3] & mask)) {
        b[y][x >> 3] |= mask;
        dirty_tiles[y >> 2][x >> 2] = 1;
        activate_row_and_neighbors(y >> 2);
    }
}

static void __attribute__((noinline)) add_noise(uint8_t (*b)[CELL_ROW_BYTES]) {
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
static void __attribute__((noinline)) add_blob(uint8_t (*b)[CELL_ROW_BYTES]) {
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

static void vdp_debug_dirty_update(void) {
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

static void vdp_debug_dirty_clear(void) {
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

static void vdp_debug_clean_snapshot(void) {
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

static void vdp_debug_clean_snapshot_clear(void) {
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

static void reseed_board(uint8_t (*b)[CELL_ROW_BYTES]) {
    if (entropy != 0) {
        rng_state ^= entropy;
        if (rng_state == 0) {
            rng_state = 0x1D2Bu;
        }
    }
    seed_board(b, seed_threshold);
    build_active_rows(b);
    mark_all_dirty();
}
#endif

void main(void) {
    life_show_splash();
    vdp_init_gfx2();
    vdp_clear_pattern_table();
    vdp_init_name_table();
    vdp_init_color_table();
    if (LIFE_DRAW_BORDER) {
        vdp_apply_border_colors();
    }
    vdp_disable_sprites();

    seed_board(cur, LIFE_SEED_THRESHOLD);
    build_active_rows(cur);
    if (LIFE_DRAW_BORDER) {
        vdp_write_border_tiles();
    }
    mark_all_dirty();
    vdp_write_dirty_patterns(cur);
    vdp_display_on();

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
        {
            uint8_t frame = *vdp_counter;
            if (frame != frame_prev) {
                entropy ^= (uint16_t)((uint16_t)frame << 8) | frame_prev;
                frame_prev = frame;
            }
        }
        *kbd_device = 0;
        {
            uint8_t key = (uint8_t)kbd_scan();
            uint8_t key_pressed = (uint8_t)(key != 0 && key != debug_key_prev);
            debug_key_prev = key;
            if (key_pressed) {
                if (key == 'D' || key == 'd') {
                    debug_dirty_enabled = (uint8_t)!debug_dirty_enabled;
                    if (!debug_dirty_enabled) {
                        vdp_debug_dirty_clear();
                    }
                } else if (key == 'F' || key == 'f') {
                    vdp_debug_clean_snapshot();
                } else if (key == 'N' || key == 'n') {
                    add_noise(cur);
                } else if (key == 'M' || key == 'm') {
                    add_blob(cur);
                } else if (key >= '1' && key <= '9') {
                    uint16_t add = (uint16_t)(key - '0');
                    entropy = (uint16_t)(entropy + (add * 0x1111u));
                    entropy ^= (uint16_t)((entropy << 5) | (entropy >> 11));
                } else if (key == 'Z' || key == 'z') {
                    seed_threshold = 70;
                    reseed_board(cur);
                } else if (key == 'X' || key == 'x') {
                    seed_threshold = 60;
                    reseed_board(cur);
                } else if (key == 'C' || key == 'c') {
                    seed_threshold = 50;
                    reseed_board(cur);
                } else if (key == 'V' || key == 'v') {
                    seed_threshold = 40;
                    reseed_board(cur);
                } else if (key == 'B' || key == 'b') {
                    seed_threshold = 30;
                    reseed_board(cur);
                } else if (key == 'R' || key == 'r') {
                    reseed_board(cur);
                }
            }
        }
        if (debug_dirty_enabled) {
            vdp_debug_dirty_update();
        }
#endif
        vdp_write_dirty_patterns(cur);
    }
}
