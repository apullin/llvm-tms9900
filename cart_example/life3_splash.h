/* Startup-only Graphics I title/controls. The font lives in ROM, so this also
 * works in an EA5 program and does not depend on the console's GROM font. */
#ifndef LIFE3_SPLASH_H
#define LIFE3_SPLASH_H

#define SPLASH_FONT_FIRST ' '
#define SPLASH_FONT_LAST 'Z'
#define SPLASH_BLOCK 0x7Fu

/* Five columns by seven rows, stored in the low five bits of each byte.
 * Unused punctuation is blank; the name table only refers to these glyphs. */
static const uint8_t splash_font[SPLASH_FONT_LAST - SPLASH_FONT_FIRST + 1][7] = {
    ['-' - SPLASH_FONT_FIRST] = {0, 0, 0, 31, 0, 0, 0},
    ['.' - SPLASH_FONT_FIRST] = {0, 0, 0, 0, 0, 12, 12},
    ['/' - SPLASH_FONT_FIRST] = {1, 2, 2, 4, 8, 8, 16},
    ['0' - SPLASH_FONT_FIRST] = {14, 17, 19, 21, 25, 17, 14},
    ['1' - SPLASH_FONT_FIRST] = {4, 12, 4, 4, 4, 4, 14},
    ['2' - SPLASH_FONT_FIRST] = {14, 17, 1, 2, 4, 8, 31},
    ['3' - SPLASH_FONT_FIRST] = {30, 1, 1, 14, 1, 1, 30},
    ['4' - SPLASH_FONT_FIRST] = {2, 6, 10, 18, 31, 2, 2},
    ['5' - SPLASH_FONT_FIRST] = {31, 16, 16, 30, 1, 1, 30},
    ['6' - SPLASH_FONT_FIRST] = {14, 16, 16, 30, 17, 17, 14},
    ['7' - SPLASH_FONT_FIRST] = {31, 1, 2, 4, 8, 8, 8},
    ['8' - SPLASH_FONT_FIRST] = {14, 17, 17, 14, 17, 17, 14},
    ['9' - SPLASH_FONT_FIRST] = {14, 17, 17, 15, 1, 1, 14},
    ['A' - SPLASH_FONT_FIRST] = {14, 17, 17, 31, 17, 17, 17},
    ['B' - SPLASH_FONT_FIRST] = {30, 17, 17, 30, 17, 17, 30},
    ['C' - SPLASH_FONT_FIRST] = {14, 17, 16, 16, 16, 17, 14},
    ['D' - SPLASH_FONT_FIRST] = {30, 17, 17, 17, 17, 17, 30},
    ['E' - SPLASH_FONT_FIRST] = {31, 16, 16, 30, 16, 16, 31},
    ['F' - SPLASH_FONT_FIRST] = {31, 16, 16, 30, 16, 16, 16},
    ['G' - SPLASH_FONT_FIRST] = {14, 17, 16, 23, 17, 17, 15},
    ['H' - SPLASH_FONT_FIRST] = {17, 17, 17, 31, 17, 17, 17},
    ['I' - SPLASH_FONT_FIRST] = {14, 4, 4, 4, 4, 4, 14},
    ['J' - SPLASH_FONT_FIRST] = {7, 2, 2, 2, 2, 18, 12},
    ['K' - SPLASH_FONT_FIRST] = {17, 18, 20, 24, 20, 18, 17},
    ['L' - SPLASH_FONT_FIRST] = {16, 16, 16, 16, 16, 16, 31},
    ['M' - SPLASH_FONT_FIRST] = {17, 27, 21, 21, 17, 17, 17},
    ['N' - SPLASH_FONT_FIRST] = {17, 25, 25, 21, 19, 19, 17},
    ['O' - SPLASH_FONT_FIRST] = {14, 17, 17, 17, 17, 17, 14},
    ['P' - SPLASH_FONT_FIRST] = {30, 17, 17, 30, 16, 16, 16},
    ['Q' - SPLASH_FONT_FIRST] = {14, 17, 17, 17, 21, 18, 13},
    ['R' - SPLASH_FONT_FIRST] = {30, 17, 17, 30, 20, 18, 17},
    ['S' - SPLASH_FONT_FIRST] = {15, 16, 16, 14, 1, 1, 30},
    ['T' - SPLASH_FONT_FIRST] = {31, 4, 4, 4, 4, 4, 4},
    ['U' - SPLASH_FONT_FIRST] = {17, 17, 17, 17, 17, 17, 14},
    ['V' - SPLASH_FONT_FIRST] = {17, 17, 17, 17, 17, 10, 4},
    ['W' - SPLASH_FONT_FIRST] = {17, 17, 17, 21, 21, 21, 10},
    ['X' - SPLASH_FONT_FIRST] = {17, 17, 10, 4, 10, 17, 17},
    ['Y' - SPLASH_FONT_FIRST] = {17, 17, 10, 4, 4, 4, 4},
    ['Z' - SPLASH_FONT_FIRST] = {31, 1, 2, 4, 8, 16, 31},
};

/* The heading uses filled 8x8 tiles as pixels. Most letters are 3x5;
 * M has an extra column to keep its two stems distinct. */
static const struct {
    char letter;
    uint8_t columns;
    uint8_t rows[5];
} splash_heading_font[] = {
    {'A', 3, {2, 5, 7, 5, 5}},
    {'E', 3, {7, 4, 6, 4, 7}},
    {'F', 3, {7, 4, 6, 4, 4}},
    {'G', 3, {3, 4, 5, 5, 3}},
    {'I', 3, {7, 2, 2, 2, 7}},
    {'L', 3, {4, 4, 4, 4, 7}},
    {'M', 4, {9, 15, 15, 9, 9}},
    {'O', 3, {2, 5, 5, 5, 2}},
};

/* These shared helpers run only during startup; conserve ROM at every level. */
static void __attribute__((cold, noinline, minsize))
splash_write_text(uint16_t row, uint16_t col, const char *text) {
    vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + row * CHAR_COLS + col));
    while (*text) {
        vdp_data((uint8_t)*text++);
    }
}

static void __attribute__((cold, noinline, minsize))
splash_write_heading(uint16_t row, uint16_t col, const char *text) {
    while (*text) {
        uint16_t glyph;
        uint16_t columns = 3; /* Spaces have the standard letter width. */
        for (glyph = 0; glyph < sizeof(splash_heading_font) / sizeof(splash_heading_font[0]); ++glyph) {
            if (splash_heading_font[glyph].letter == *text) {
                uint16_t y;
                columns = splash_heading_font[glyph].columns;
                for (y = 0; y < 5; ++y) {
                    uint8_t mask = (uint8_t)(1u << (columns - 1));
                    vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + (row + y) * CHAR_COLS + col));
                    while (mask) {
                        vdp_data((splash_heading_font[glyph].rows[y] & mask) ? SPLASH_BLOCK : ' ');
                        mask >>= 1;
                    }
                }
                break;
            }
        }
        col += columns + 1;
        ++text;
    }
}

/* Keep startup text/layout code out of the game loop and optimize it for ROM. */
static void __attribute__((cold, noinline, minsize)) life_show_splash(void) {
    uint16_t i;
    uint16_t row;

    vdp_set_register(1, 0x80); /* Display off before changing modes. */
    vdp_set_register(0, 0x00); /* Graphics I: 32x24 characters. */
    vdp_set_register(2, 0x06); /* Name table @ 0x1800. */
    vdp_set_register(3, 0x80); /* 32-byte color table @ 0x2000. */
    vdp_set_register(4, 0x00); /* Pattern table @ 0x0000. */
    vdp_set_register(5, 0x36);
    vdp_set_register(6, 0x07);
    vdp_set_register(7, 0x01); /* Black backdrop. */
    vdp_disable_sprites();

    vdp_set_write_addr(NAME_TABLE_ADDR);
    for (i = 0; i < CHAR_ROWS * CHAR_COLS; ++i) {
        vdp_data(' ');
    }
    vdp_set_write_addr(COLOR_TABLE_ADDR);
    for (i = 0; i < 32; ++i) {
        vdp_data(0xF1); /* White on black. */
    }
    vdp_set_write_addr(COLOR_TABLE_ADDR + SPLASH_BLOCK / 8);
    vdp_data(0x71); /* Cyan filled tiles for the heading. */

    vdp_set_write_addr(PATTERN_TABLE_ADDR + SPLASH_FONT_FIRST * 8);
    for (i = 0; i < sizeof(splash_font) / sizeof(splash_font[0]); ++i) {
        for (row = 0; row < 7; ++row) {
            vdp_data((uint8_t)(splash_font[i][row] << 2));
        }
        vdp_data(0); /* Eighth scanline provides text line spacing. */
    }
    vdp_set_write_addr(PATTERN_TABLE_ADDR + SPLASH_BLOCK * 8);
    for (row = 0; row < 8; ++row) {
        vdp_data(0xFF);
    }

    splash_write_heading(1, 2, "GAME OF");
    splash_write_heading(7, 8, "LIFE");
    splash_write_text(13, 9, "V" LIFE_VERSION "  " LIFE_DATE);
    /* Key groups end at column 7; descriptions start at column 11.
     * The five-key density group hangs left of the single/paired controls. */
    splash_write_text(15, 3, "ZXCVB   DENSITY");
    splash_write_text(16, 5, "R/K   RESTART / CLEAR");
    splash_write_text(17, 7, "P   PRE-BAKED SCENES");
    splash_write_text(18, 5, "G/S   GLIDER / SPACESHIP");
    splash_write_text(19, 5, "N/M   ADD NOISE / BLOB");
    splash_write_text(20, 5, "D/F   DIRTY / CLEAN TILES");
    splash_write_text(21, 5, "1-9   ADD RANDOM ENTROPY");
    splash_write_text(23, 9, "SPACE TO START");
    vdp_display_on();

    /* Named checkpoints let the CPU tests inspect the finished screen and
     * prove that keyboard gating completes before any Life update. */
    asm volatile(".globl life_splash_ready\nlife_splash_ready:\n" ::: "memory");
    do {
        *(volatile uint8_t *)0x8374 = 0; /* Full keyboard scan, like the game. */
    } while ((uint8_t)kbd_scan() != ' ');
    asm volatile(".globl life_splash_end\nlife_splash_end:\n" ::: "memory");
}

#endif
