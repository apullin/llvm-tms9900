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

#define COLOR_FG 0x07 /* cyan */
#define COLOR_BG 0x01 /* black */
#define COLOR_BORDER 0x0B /* light yellow */

#define COLOR_TABLE_ADDR 0x2000
#define PATTERN_TABLE_ADDR 0x0000
#define NAME_TABLE_ADDR 0x1800
#define SPRITE_ATTR_ADDR 0x1B00

#define LIFE_REGION_W 224
#define LIFE_REGION_H 160
/* Align to 16 pixels for word-wide updates. */
#define LIFE_REGION_X0 (((SCREEN_W - LIFE_REGION_W) / 2) & ~15)
#define LIFE_REGION_Y0 ((SCREEN_H - LIFE_REGION_H) / 2)
#define LIFE_REGION_X1 (LIFE_REGION_X0 + LIFE_REGION_W - 1)
#define LIFE_REGION_Y1 (LIFE_REGION_Y0 + LIFE_REGION_H - 1)
#define LIFE_REGION_ROW_BYTES (LIFE_REGION_W / 8)
#define LIFE_REGION_BYTE_X0 (LIFE_REGION_X0 / 8)
#define LIFE_REGION_WORDS (LIFE_REGION_ROW_BYTES / 2)

#if (LIFE_REGION_W & 15)
#error "LIFE_REGION_W must be a multiple of 16 pixels for word-wide updates."
#endif

#define LIFE_SEED_DENSITY 40
#define LIFE_SEED_BLOCK_SIZE 4
#define LIFE_SEED_EDGE_BLOCKS 1

#define LIFE_TILES_W (LIFE_REGION_W / 8)
#define LIFE_TILES_H (LIFE_REGION_H / 8)

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
    uint16_t tile_x_left = (uint16_t)((LIFE_REGION_X0 - 1) >> 3);
    uint16_t tile_x_right = (uint16_t)((LIFE_REGION_X1 + 1) >> 3);
    uint16_t tile_y_top = (uint16_t)((LIFE_REGION_Y0 - 1) >> 3);
    uint16_t tile_y_bottom = (uint16_t)((LIFE_REGION_Y1 + 1) >> 3);
    uint16_t tile_x;
    uint16_t tile_y;
    uint8_t color = (uint8_t)((COLOR_BORDER << 4) | (COLOR_BG & 0x0F));

    for (tile_x = tile_x_left; tile_x <= tile_x_right; tile_x++) {
        vdp_set_tile_color(tile_y_top, tile_x, color);
        vdp_set_tile_color(tile_y_bottom, tile_x, color);
    }

    for (tile_y = tile_y_top; tile_y <= tile_y_bottom; tile_y++) {
        vdp_set_tile_color(tile_y, tile_x_left, color);
        vdp_set_tile_color(tile_y, tile_x_right, color);
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
static uint16_t rng_state = 0xACE1u;

static uint16_t rng_next(void) {
    uint16_t lsb = (uint16_t)(rng_state & 1u);
    rng_state >>= 1;
    if (lsb) {
        rng_state ^= 0xB400u;
    }
    return rng_state;
}

/* ===== BOARD ===== */
static uint8_t board_a[SCREEN_H][ROW_BYTES] __attribute__((aligned(2)));
static uint8_t board_b[SCREEN_H][ROW_BYTES] __attribute__((aligned(2)));
static uint8_t (*cur)[ROW_BYTES] = board_a;
static uint8_t (*next)[ROW_BYTES] = board_b;
typedef uint16_t u16_alias __attribute__((__may_alias__));

static uint8_t active_tiles[LIFE_TILES_H][LIFE_TILES_W];
static uint8_t live_tiles[LIFE_TILES_H][LIFE_TILES_W];
static uint8_t dirty_tiles[LIFE_TILES_H][LIFE_TILES_W];

static const uint8_t bit_mask[8] = {
    0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u
};

static void build_active_tiles(uint8_t (*src)[ROW_BYTES]) {
    uint16_t ty;
    uint16_t tx;

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            active_tiles[ty][tx] = 0;
        }
    }

    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        uint16_t y_base = (uint16_t)(LIFE_REGION_Y0 + (ty << 3));
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            uint16_t xbyte = (uint16_t)(LIFE_REGION_BYTE_X0 + tx);
            uint8_t any = 0;
            uint16_t row;

            for (row = 0; row < 8; row++) {
                any |= src[y_base + row][xbyte];
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

static void clear_board(uint8_t (*b)[ROW_BYTES]) {
    uint16_t y;
    uint16_t x;
    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < ROW_BYTES; x++) {
            b[y][x] = 0;
        }
    }
}

static void clear_board_region(uint8_t (*b)[ROW_BYTES]) {
    uint16_t y0 = (uint16_t)(LIFE_REGION_Y0);
    uint16_t y1 = (uint16_t)(LIFE_REGION_Y1);
    uint16_t x0 = (uint16_t)(LIFE_REGION_BYTE_X0);
    uint16_t x1 = (uint16_t)(LIFE_REGION_BYTE_X0 + LIFE_REGION_ROW_BYTES - 1);
    uint16_t y;
    uint16_t x;

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
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

static void draw_border(uint8_t (*b)[ROW_BYTES]) {
    uint16_t x_left = (uint16_t)(LIFE_REGION_X0 - 1);
    uint16_t x_right = (uint16_t)(LIFE_REGION_X1 + 1);
    uint16_t y_top = (uint16_t)(LIFE_REGION_Y0 - 1);
    uint16_t y_bottom = (uint16_t)(LIFE_REGION_Y1 + 1);
    uint16_t x;
    uint16_t y;

    for (x = x_left; x <= x_right; x++) {
        set_cell(b, x, y_top);
        set_cell(b, x, y_bottom);
    }

    for (y = y_top; y <= y_bottom; y++) {
        set_cell(b, x_left, y);
        set_cell(b, x_right, y);
    }
}

static void seed_block(uint8_t (*b)[ROW_BYTES], uint16_t x0, uint16_t y0) {
    uint16_t x;
    uint16_t y;

    for (y = 0; y < LIFE_SEED_BLOCK_SIZE; y++) {
        for (x = 0; x < LIFE_SEED_BLOCK_SIZE; x++) {
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

    uint16_t n2 = (uint16_t)((uint16_t)(~bit0) & bit1 & (uint16_t)(~bit2) & (uint16_t)(~bit3));
    uint16_t n3 = (uint16_t)(bit0 & bit1 & (uint16_t)(~bit2) & (uint16_t)(~bit3));

    return (uint16_t)(n3 | (n2 & mC));
}

static void seed_board(uint8_t (*b)[ROW_BYTES]) {
    uint16_t x;
    uint16_t y;

    clear_board_region(b);

    for (y = LIFE_REGION_Y0; y <= LIFE_REGION_Y1; y += LIFE_SEED_BLOCK_SIZE) {
        for (x = LIFE_REGION_X0; x <= LIFE_REGION_X1; x += LIFE_SEED_BLOCK_SIZE) {
            if ((uint8_t)rng_next() < LIFE_SEED_DENSITY) {
                seed_block(b, x, y);
            }
        }
    }

#if LIFE_SEED_EDGE_BLOCKS
    {
        uint16_t mask = (uint16_t)(~(LIFE_SEED_BLOCK_SIZE - 1));
        uint16_t edge_x = (uint16_t)(LIFE_REGION_X0 +
                                     ((rng_next() & (LIFE_REGION_W - 1)) & mask));
        uint16_t edge_y = (uint16_t)(LIFE_REGION_Y0 +
                                     ((rng_next() & (LIFE_REGION_H - 1)) & mask));
        seed_block(b, LIFE_REGION_X0, edge_y);
        seed_block(b, (uint16_t)(LIFE_REGION_X1 - (LIFE_SEED_BLOCK_SIZE - 1)), edge_y);
        seed_block(b, edge_x, LIFE_REGION_Y0);
        seed_block(b, edge_x, (uint16_t)(LIFE_REGION_Y1 - (LIFE_SEED_BLOCK_SIZE - 1)));
    }
#endif

    draw_border(b);
}

static void __attribute__((noinline)) step_board(uint8_t (*src)[ROW_BYTES],
                                                 uint8_t (*dst)[ROW_BYTES]) {
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

    /* Reset per-frame tile state. */
    clear_board_region(dst);
    for (ty = 0; ty < LIFE_TILES_H; ty++) {
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            live_tiles[ty][tx] = 0;
            dirty_tiles[ty][tx] = 0;
        }
    }

    /* Main forward compute over 16-bit word blocks. */
    for (y = LIFE_REGION_Y0; y <= LIFE_REGION_Y1; y++) {
        uint16_t y_prev = (y == LIFE_REGION_Y0) ? LIFE_REGION_Y1 : (y - 1);
        uint16_t y_next = (y == LIFE_REGION_Y1) ? LIFE_REGION_Y0 : (y + 1);
        uint16_t ty = (uint16_t)((y - LIFE_REGION_Y0) >> 3);
        u16_alias *row_prev = (u16_alias *)&src[y_prev][LIFE_REGION_BYTE_X0];
        u16_alias *row = (u16_alias *)&src[y][LIFE_REGION_BYTE_X0];
        u16_alias *row_next = (u16_alias *)&src[y_next][LIFE_REGION_BYTE_X0];
        u16_alias *row_dst = (u16_alias *)&dst[y][LIFE_REGION_BYTE_X0];

        for (wx = 0; wx < LIFE_REGION_WORDS; wx++) {
            uint16_t tile_x = (uint16_t)(wx << 1);
            /* Skip word if both tiles are inactive. */
            if (!active_tiles[ty][tile_x] && !active_tiles[ty][tile_x + 1]) {
                continue;
            }
            uint16_t wx_left = (wx == 0) ? (LIFE_REGION_WORDS - 1) : (wx - 1);
            uint16_t wx_right = (wx + 1 == LIFE_REGION_WORDS) ? 0 : (wx + 1);

            uint16_t uL = row_prev[wx_left];
            uint16_t uC = row_prev[wx];
            uint16_t uR = row_prev[wx_right];
            uint16_t mL = row[wx_left];
            uint16_t mC = row[wx];
            uint16_t mR = row[wx_right];
            uint16_t dL = row_next[wx_left];
            uint16_t dC = row_next[wx];
            uint16_t dR = row_next[wx_right];

            uint16_t u_left = (uint16_t)((uC >> 1) | (uL << 15));
            uint16_t u_right = (uint16_t)((uC << 1) | (uR >> 15));
            uint16_t m_left = (uint16_t)((mC >> 1) | (mL << 15));
            uint16_t m_right = (uint16_t)((mC << 1) | (mR >> 15));
            uint16_t d_left = (uint16_t)((dC >> 1) | (dL << 15));
            uint16_t d_right = (uint16_t)((dC << 1) | (dR >> 15));

            uint16_t out = life_next_word(u_left, uC, u_right, m_left, mC, m_right,
                                          d_left, dC, d_right);

            /* Update destination and mark dirty/live tiles. */
            row_dst[wx] = out;
            if ((uint16_t)(out ^ mC) & 0xFF00u) {
                dirty_tiles[ty][tile_x] = 1;
            }
            if ((uint16_t)(out ^ mC) & 0x00FFu) {
                dirty_tiles[ty][(uint16_t)(tile_x + 1)] = 1;
            }
            if (out & 0xFF00u) {
                live_tiles[ty][tile_x] = 1;
            }
            if (out & 0x00FFu) {
                live_tiles[ty][(uint16_t)(tile_x + 1)] = 1;
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

static void vdp_write_tile(uint16_t tile_row, uint16_t tile_col,
                           uint8_t (*b)[ROW_BYTES]) {
    uint16_t group = (uint16_t)(tile_row >> 3);
    uint16_t pattern_base = (uint16_t)((tile_row & 7u) << 5);
    uint16_t pattern = (uint16_t)(pattern_base | tile_col);
    uint16_t addr = (uint16_t)(PATTERN_TABLE_ADDR + (group * 0x0800) + (pattern << 3));
    uint16_t row_in_char;
    uint16_t base_y = (uint16_t)(tile_row << 3);

    vdp_set_write_addr(addr);
    for (row_in_char = 0; row_in_char < 8; row_in_char++) {
        vdp_data(b[base_y + row_in_char][tile_col]);
    }
}

static void vdp_write_border_patterns(uint8_t (*b)[ROW_BYTES]) {
    uint16_t tile_row_top = (uint16_t)((LIFE_REGION_Y0 - 1) >> 3);
    uint16_t tile_row_bottom = (uint16_t)((LIFE_REGION_Y1 + 1) >> 3);
    uint16_t tile_col_left = (uint16_t)((LIFE_REGION_X0 - 1) >> 3);
    uint16_t tile_col_right = (uint16_t)((LIFE_REGION_X1 + 1) >> 3);
    uint16_t tile_row;
    uint16_t tile_col;

    if (tile_row_top > 23) tile_row_top = 0;
    if (tile_row_bottom > 23) tile_row_bottom = 23;
    if (tile_col_left > 31) tile_col_left = 0;
    if (tile_col_right > 31) tile_col_right = 31;

    for (tile_col = tile_col_left; tile_col <= tile_col_right; tile_col++) {
        vdp_write_tile(tile_row_top, tile_col, b);
        vdp_write_tile(tile_row_bottom, tile_col, b);
    }

    for (tile_row = tile_row_top; tile_row <= tile_row_bottom; tile_row++) {
        vdp_write_tile(tile_row, tile_col_left, b);
        vdp_write_tile(tile_row, tile_col_right, b);
    }
}

static void vdp_write_patterns(uint8_t (*b)[ROW_BYTES]) {
    uint16_t tile_row_start = (uint16_t)(LIFE_REGION_Y0 >> 3);
    uint16_t tile_row_end = (uint16_t)(LIFE_REGION_Y1 >> 3);
    uint16_t tile_col_start = (uint16_t)(LIFE_REGION_X0 >> 3);
    uint16_t tile_col_end = (uint16_t)(LIFE_REGION_X1 >> 3);
    uint16_t tile_row;
    uint16_t tile_col;

    for (tile_row = tile_row_start; tile_row <= tile_row_end; tile_row++) {
        for (tile_col = tile_col_start; tile_col <= tile_col_end; tile_col++) {
            vdp_write_tile(tile_row, tile_col, b);
        }
    }
}

static void vdp_write_dirty_patterns(uint8_t (*b)[ROW_BYTES]) {
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
        uint16_t tile_row = (uint16_t)(tile_row_start + ty);
        for (tx = 0; tx < LIFE_TILES_W; tx++) {
            if (!dirty_tiles[ty][tx]) {
                continue;
            }
            vdp_write_tile(tile_row, (uint16_t)(tile_col_start + tx), b);
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

void main(void) {
    vdp_init_gfx2();
    vdp_clear_pattern_table();
    vdp_init_name_table();
    vdp_init_color_table();
    vdp_apply_border_colors();
    vdp_disable_sprites();

    seed_board(cur);
    build_active_tiles(cur);
    vdp_write_border_patterns(cur);
    vdp_write_patterns(cur);
    vdp_display_on();

    for (;;) {
        uint8_t (*tmp)[ROW_BYTES] = cur;
        cur = next;
        next = tmp;
        step_board(next, cur);
        vdp_write_dirty_patterns(cur);
    }
}
