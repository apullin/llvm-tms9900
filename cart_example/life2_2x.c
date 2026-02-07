/*
 * TI-99/4A Game of Life (Graphics II mode) - Optimized version
 * 2x scaled rendering from a 128x96 cell grid.
 *
 * Optimizations over life2x.c:
 *   1. Simplified Life rule: bit1 & ~bit2 & ~bit3 & (bit0 | mC)
 *   2. Word-level active_tiles check (two uint16_t loads)
 *   3. Sliding window row access (3 loads/word instead of 9)
 *   4. Word-level tile array clears
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

#define LIFE_SEED_DENSITY 70
#define LIFE_SEED_BLOCK_SIZE 4
#define LIFE_SEED_EDGE_BLOCKS 1
#define LIFE_SEED_DROPOUT_PCT 20
#define LIFE_SEED_DROPOUT_THRESHOLD ((uint8_t)((LIFE_SEED_DROPOUT_PCT * 256) / 100))

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
    vdp_set_register(0, 0x02); /* Bitmap graphics mode */
    vdp_set_register(1, 0x80); /* Display off, 16K VRAM */
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

/* ===== BOARD ===== */
static uint8_t board_a[CELL_H][CELL_ROW_BYTES] __attribute__((aligned(2)));
static uint8_t board_b[CELL_H][CELL_ROW_BYTES] __attribute__((aligned(2)));
static uint8_t (*cur)[CELL_ROW_BYTES] = board_a;
static uint8_t (*next)[CELL_ROW_BYTES] = board_b;
typedef uint16_t u16_alias __attribute__((__may_alias__));

/* OPT 2: aligned(2) so word-level access is safe. */
static uint8_t active_tiles[LIFE_TILES_H][LIFE_TILES_W] __attribute__((aligned(2)));
static uint8_t live_tiles[LIFE_TILES_H][LIFE_TILES_W] __attribute__((aligned(2)));
static uint8_t dirty_tiles[LIFE_TILES_H][LIFE_TILES_W] __attribute__((aligned(2)));
#if LIFE_DEBUG_DIRTY
static uint8_t dirty_prev[LIFE_TILES_H][LIFE_TILES_W];
static uint8_t debug_dirty_enabled;
static uint8_t debug_key_prev;
static uint16_t entropy;
static uint8_t seed_density = LIFE_SEED_DENSITY;
static volatile uint8_t * const kbd_device = (volatile uint8_t *)0x8374;
static volatile uint8_t * const vdp_counter = (volatile uint8_t *)0x8379;
static uint8_t frame_prev;
extern uint16_t kbd_scan(void);
#endif

static const uint8_t bit_mask[8] = {
    0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u
};

static const uint8_t expand4[16] = {
    0x00, 0x03, 0x0C, 0x0F,
    0x30, 0x33, 0x3C, 0x3F,
    0xC0, 0xC3, 0xCC, 0xCF,
    0xF0, 0xF3, 0xFC, 0xFF
};

static void build_active_tiles(uint8_t (*src)[CELL_ROW_BYTES]) {
    uint16_t ty;
    uint16_t tx;

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            active_tiles[ty][tx] = 0;
        }
    }

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        uint16_t y_base = (uint16_t)(ty << 2);
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            uint16_t cell_x = (uint16_t)(tx << 2);
            uint16_t byte_index = (uint16_t)(cell_x >> 3);
            uint8_t any = 0;
            uint16_t row;

            for (row = 0; row < 4; row++) {
                uint8_t v = src[y_base + row][byte_index];
                uint8_t nibble = (cell_x & 4u) ? (uint8_t)(v & 0x0F) : (uint8_t)(v >> 4);
                any |= nibble;
            }

            if (any) {
                uint16_t ty_prev = (ty == 0) ? (LIFE_TILES_H - 1) : (ty - 1);
                uint16_t ty_next = (ty + 1 == LIFE_TILES_H) ? 0 : (ty + 1);
                uint16_t tx_prev = (tx == 0) ? (LIFE_TILES_W - 1) : (tx - 1);
                uint16_t tx_next = (tx + 1 == LIFE_TILES_W) ? 0 : (tx + 1);

                active_tiles[ty_prev][tx_prev] = 1;
                active_tiles[ty_prev][tx] = 1;
                active_tiles[ty_prev][tx_next] = 1;
                active_tiles[ty][tx_prev] = 1;
                active_tiles[ty][tx] = 1;
                active_tiles[ty][tx_next] = 1;
                active_tiles[ty_next][tx_prev] = 1;
                active_tiles[ty_next][tx] = 1;
                active_tiles[ty_next][tx_next] = 1;
            }
        }
    }
}

static void build_active_from_live(void) {
    uint16_t ty;
    uint16_t tx;

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            active_tiles[ty][tx] = 0;
        }
    }

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            if (!live_tiles[ty][tx]) {
                continue;
            }
            {
                uint16_t ty_prev = (ty == 0) ? (LIFE_TILES_H - 1) : (ty - 1);
                uint16_t ty_next = (ty + 1 == LIFE_TILES_H) ? 0 : (ty + 1);
                uint16_t tx_prev = (tx == 0) ? (LIFE_TILES_W - 1) : (tx - 1);
                uint16_t tx_next = (tx + 1 == LIFE_TILES_W) ? 0 : (tx + 1);

                active_tiles[ty_prev][tx_prev] = 1;
                active_tiles[ty_prev][tx] = 1;
                active_tiles[ty_prev][tx_next] = 1;
                active_tiles[ty][tx_prev] = 1;
                active_tiles[ty][tx] = 1;
                active_tiles[ty][tx_next] = 1;
                active_tiles[ty_next][tx_prev] = 1;
                active_tiles[ty_next][tx] = 1;
                active_tiles[ty_next][tx_next] = 1;
            }
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

static void seed_block(uint8_t (*b)[CELL_ROW_BYTES], uint16_t x0, uint16_t y0,
                       uint8_t dropout_threshold) {
    uint16_t x;
    uint16_t y;

    for (y = 0; y < LIFE_SEED_BLOCK_SIZE; y++) {
        for (x = 0; x < LIFE_SEED_BLOCK_SIZE; x++) {
            if (dropout_threshold && (uint8_t)rng_next() < dropout_threshold) {
                continue;
            }
            set_cell(b, (uint16_t)(x0 + x), (uint16_t)(y0 + y));
        }
    }
}

static uint16_t life_next_word(uint16_t u_left, uint16_t uC, uint16_t u_right,
                               uint16_t m_left, uint16_t mC, uint16_t m_right,
                               uint16_t d_left, uint16_t dC, uint16_t d_right) {
    /* Bit-parallel neighbor count over 16 lanes. */
    uint16_t sum1 = (uint16_t)(u_left ^ uC ^ u_right);
    uint16_t carry1 = (uint16_t)((u_left & uC) | (u_left & u_right) | (uC & u_right));
    uint16_t sum2 = (uint16_t)(m_left ^ m_right ^ d_left);
    uint16_t carry2 = (uint16_t)((m_left & m_right) | (m_left & d_left) | (m_right & d_left));
    uint16_t sum3 = (uint16_t)(dC ^ d_right);
    uint16_t carry3 = (uint16_t)(dC & d_right);

    uint16_t sum4 = (uint16_t)(sum1 ^ sum2 ^ sum3);
    uint16_t carry4 = (uint16_t)((sum1 & sum2) | (sum1 & sum3) | (sum2 & sum3));

    uint16_t t_sum = (uint16_t)(carry1 ^ carry2 ^ carry3);
    uint16_t t_carry = (uint16_t)((carry1 & carry2) | (carry1 & carry3) | (carry2 & carry3));
    uint16_t c0 = (uint16_t)(t_sum ^ carry4);
    uint16_t c1 = (uint16_t)(t_sum & carry4);
    uint16_t c1_sum = (uint16_t)(t_carry ^ c1);
    uint16_t c2 = (uint16_t)(t_carry & c1);

    uint16_t bit0 = sum4;
    uint16_t bit1 = c0;
    uint16_t bit2 = c1_sum;
    uint16_t bit3 = c2;

    /* OPT 1: Simplified rule (6 ops instead of 11). */
    return (uint16_t)(bit1 & (uint16_t)(~bit2) & (uint16_t)(~bit3) &
                      (uint16_t)(bit0 | mC));
}

static void seed_board(uint8_t (*b)[CELL_ROW_BYTES], uint8_t density) {
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
            if ((uint8_t)rng_next() < density) {
                seed_block(b, x_pos, y_pos, LIFE_SEED_DROPOUT_THRESHOLD);
            }
        }
    }

#if LIFE_SEED_EDGE_BLOCKS
    {
        uint16_t mask = (uint16_t)(~(LIFE_SEED_BLOCK_SIZE - 1));
        uint16_t edge_x = (uint16_t)((rng_next() & (CELL_W - 1)) & mask);
        uint16_t edge_y = (uint16_t)((rng_next() & (CELL_H - 1)) & mask);
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
    uint16_t wx;
    uint16_t ty;
    uint16_t tx;

    /* OPT 4: Word-level tile array clears. */
    clear_board(dst);
    {
        u16_alias *lp = (u16_alias *)&live_tiles[0][0];
        u16_alias *dp = (u16_alias *)&dirty_tiles[0][0];
        uint16_t n = (uint16_t)((LIFE_TILES_H * LIFE_TILES_W) / 2);
        uint16_t i;
        for (i = 0; i < n; i++) {
            lp[i] = 0;
            dp[i] = 0;
        }
    }

    /* OPT 6: Tile-row-level active check + unconditional sliding window.
     * Instead of checking active_tiles per-word (~15 instructions overhead
     * per word), check once per tile-row (4 cell-rows). When a tile-row
     * is active, process all words without per-word checks, allowing the
     * sliding window to always slide (no need_reload). */
    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        /* Check if any tile in this tile-row is active. */
        {
            uint16_t row_active = 0;
            uint16_t i;
            for (i = 0; i < LIFE_TILES_W; i += 2) {
                row_active |= *(u16_alias *)&active_tiles[ty][i];
            }
            if (!row_active) continue;
        }

        uint16_t y_base = (uint16_t)(ty << 2);
        uint16_t dy;
        for (dy = 0; dy < 4; dy++) {
            y = y_base + dy;
            uint16_t y_prev = (y == 0) ? (CELL_H - 1) : (y - 1);
            uint16_t y_next = (y + 1 == CELL_H) ? 0 : (y + 1);
            u16_alias *row_prev = (u16_alias *)&src[y_prev][0];
            u16_alias *row = (u16_alias *)&src[y][0];
            u16_alias *row_next = (u16_alias *)&src[y_next][0];
            u16_alias *row_dst = (u16_alias *)&dst[y][0];

            uint16_t uC, uR, mC, mR, dC, dR;

            /* Word 0: left wraps to last word. */
            {
                uint16_t uL = row_prev[CELL_WORDS - 1];
                uint16_t mL = row[CELL_WORDS - 1];
                uint16_t dL = row_next[CELL_WORDS - 1];
                uC = row_prev[0]; mC = row[0]; dC = row_next[0];
                uR = row_prev[1]; mR = row[1]; dR = row_next[1];

                uint16_t u_left = (uint16_t)((uC >> 1) | (uL << 15));
                uint16_t u_right = (uint16_t)((uC << 1) | (uR >> 15));
                uint16_t m_left = (uint16_t)((mC >> 1) | (mL << 15));
                uint16_t m_right = (uint16_t)((mC << 1) | (mR >> 15));
                uint16_t d_left = (uint16_t)((dC >> 1) | (dL << 15));
                uint16_t d_right = (uint16_t)((dC << 1) | (dR >> 15));
                row_dst[0] = life_next_word(u_left, uC, u_right, m_left, mC,
                                            m_right, d_left, dC, d_right);
            }

            /* Words 1 to CELL_WORDS-2: always slide, no wrap check. */
            for (wx = 1; wx < CELL_WORDS - 1; wx++) {
                uint16_t uL = uC; uC = uR; uR = row_prev[wx + 1];
                uint16_t mL = mC; mC = mR; mR = row[wx + 1];
                uint16_t dL = dC; dC = dR; dR = row_next[wx + 1];

                uint16_t u_left = (uint16_t)((uC >> 1) | (uL << 15));
                uint16_t u_right = (uint16_t)((uC << 1) | (uR >> 15));
                uint16_t m_left = (uint16_t)((mC >> 1) | (mL << 15));
                uint16_t m_right = (uint16_t)((mC << 1) | (mR >> 15));
                uint16_t d_left = (uint16_t)((dC >> 1) | (dL << 15));
                uint16_t d_right = (uint16_t)((dC << 1) | (dR >> 15));
                row_dst[wx] = life_next_word(u_left, uC, u_right, m_left, mC,
                                             m_right, d_left, dC, d_right);
            }

            /* Last word: right wraps to word 0. */
            {
                uint16_t uL = uC; uC = uR; uR = row_prev[0];
                uint16_t mL = mC; mC = mR; mR = row[0];
                uint16_t dL = dC; dC = dR; dR = row_next[0];

                uint16_t u_left = (uint16_t)((uC >> 1) | (uL << 15));
                uint16_t u_right = (uint16_t)((uC << 1) | (uR >> 15));
                uint16_t m_left = (uint16_t)((mC >> 1) | (mL << 15));
                uint16_t m_right = (uint16_t)((mC << 1) | (mR >> 15));
                uint16_t d_left = (uint16_t)((dC >> 1) | (dL << 15));
                uint16_t d_right = (uint16_t)((dC << 1) | (dR >> 15));
                row_dst[CELL_WORDS - 1] = life_next_word(u_left, uC, u_right,
                                                          m_left, mC, m_right,
                                                          d_left, dC, d_right);
            }
        }
    }

    /* OPT 5: Build dirty_tiles and live_tiles in a separate pass.
     * Accumulates 4 cell-rows per tile-row, reducing branch overhead
     * vs the per-word nibble checks that were in the inner loop. */
    {
        uint16_t ty2, wx2, row;
        for (ty2 = 0; ty2 < LIFE_TILES_H; ty2++) {
            uint16_t y_base = (uint16_t)(ty2 << 2);
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
                if (live_acc & 0xF000u)
                    live_tiles[ty2][tile_x2] = 1;
                if (live_acc & 0x0F00u)
                    live_tiles[ty2][(uint16_t)(tile_x2 + 1)] = 1;
                if (live_acc & 0x00F0u)
                    live_tiles[ty2][(uint16_t)(tile_x2 + 2)] = 1;
                if (live_acc & 0x000Fu)
                    live_tiles[ty2][(uint16_t)(tile_x2 + 3)] = 1;
                if (dirty_acc & 0xF000u)
                    dirty_tiles[ty2][tile_x2] = 1;
                if (dirty_acc & 0x0F00u)
                    dirty_tiles[ty2][(uint16_t)(tile_x2 + 1)] = 1;
                if (dirty_acc & 0x00F0u)
                    dirty_tiles[ty2][(uint16_t)(tile_x2 + 2)] = 1;
                if (dirty_acc & 0x000Fu)
                    dirty_tiles[ty2][(uint16_t)(tile_x2 + 3)] = 1;
            }
        }
    }

    /* Expand active set from new live tiles for next frame. */
    build_active_from_live();

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

static void vdp_write_dirty_patterns(uint8_t (*b)[CELL_ROW_BYTES]) {
    /* Tracepoint label for draw timing (tms9900-trace). */
    asm volatile(
        ".globl life_draw_begin\n"
        "life_draw_begin:\n"
        :
        :
        : "memory");
    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t ty;
    uint16_t tx;

    /* Draw only tiles marked dirty by the last update. */
    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            if (!dirty_tiles[ty][tx]) {
                continue;
            }
            vdp_write_tile_scaled(ty, tx, tile_row_start, tile_col_start, b);
        }
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
}

static void reseed_board(uint8_t (*b)[CELL_ROW_BYTES]) {
    if (entropy != 0) {
        rng_state ^= entropy;
        if (rng_state == 0) {
            rng_state = 0x1D2Bu;
        }
    }
    seed_board(b, seed_density);
    build_active_tiles(b);
    mark_all_dirty();
}
#endif

void main(void) {
    vdp_init_gfx2();
    vdp_clear_pattern_table();
    vdp_init_name_table();
    vdp_init_color_table();
    if (LIFE_DRAW_BORDER) {
        vdp_apply_border_colors();
    }
    vdp_disable_sprites();

    seed_board(cur, LIFE_SEED_DENSITY);
    build_active_tiles(cur);
    if (LIFE_DRAW_BORDER) {
        vdp_write_border_tiles();
    }
    mark_all_dirty();
    vdp_write_dirty_patterns(cur);
    vdp_display_on();

    for (;;) {
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
                } else if (key >= '1' && key <= '9') {
                    uint16_t add = (uint16_t)(key - '0');
                    entropy = (uint16_t)(entropy + (add * 0x1111u));
                    entropy ^= (uint16_t)((entropy << 5) | (entropy >> 11));
                } else if (key == 'Z' || key == 'z') {
                    seed_density = 70;
                    reseed_board(cur);
                } else if (key == 'X' || key == 'x') {
                    seed_density = 60;
                    reseed_board(cur);
                } else if (key == 'C' || key == 'c') {
                    seed_density = 50;
                    reseed_board(cur);
                } else if (key == 'V' || key == 'v') {
                    seed_density = 40;
                    reseed_board(cur);
                } else if (key == 'B' || key == 'b') {
                    seed_density = 30;
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
