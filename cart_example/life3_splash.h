/* Startup-only Graphics I title/controls. The font lives in ROM, so this also
 * works in an EA5 program and does not depend on the console's GROM font. */
#ifndef LIFE3_SPLASH_H
#define LIFE3_SPLASH_H

#define SPLASH_FONT_FIRST ' '
#define SPLASH_FONT_LAST 'Z'
#define SPLASH_YELLOW_BASE 0x60u
#define SPLASH_CYAN_BASE   0x70u

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
    [':' - SPLASH_FONT_FIRST] = {8, 4, 28, 0, 0, 0, 0},  /* SE 3x3 glider */
    [';' - SPLASH_FONT_FIRST] = {8, 16, 28, 0, 0, 0, 0}, /* SW 3x3 glider */
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

/* Stamp 5x7 glyphs from `splash_font` into a 64x16 subpixel banner grid
 * (stored as 32x8 Graphics I tile codes on the stack). */
static void __attribute__((cold, noinline, minsize))
splash_stamp_text2x(uint8_t *grid, uint16_t sx, uint16_t sy,
                    const char *text, uint8_t base) {
    while (*text) {
        char ch = *text++;
        if (ch != ' ') {
            uint16_t py;
            uint16_t px;
            for (py = 0; py < 7; ++py) {
                uint8_t bits = splash_font[(uint8_t)(ch - SPLASH_FONT_FIRST)][py];
                for (px = 0; px < 5; ++px) {
                    if (bits & (uint8_t)(0x10u >> px)) {
                        uint16_t x = (uint16_t)(sx + px);
                        uint16_t y = (uint16_t)(sy + py);
                        uint8_t *tile = &grid[(y >> 1) * CHAR_COLS + (x >> 1)];
                        *tile = (uint8_t)((*tile & 15u) | base |
                                          (1u << (((y & 1u) << 1) | (x & 1u))));
                    }
                }
            }
        }
        sx = (uint16_t)(sx + 6u);
    }
}

static const char splash_lines[] =
    "\x0A\x09" "V" LIFE_VERSION "  " LIFE_DATE "\0"
    "\x0C\x03" "ZXCVB   DENSITY\0"
    "\x0D\x05" "R/K   RESTART / CLEAR\0"
    "\x0E\x07" "P   PREMADE SCENES\0"
    "\x0F\x05" "G/S   GLIDER / SPACESHIP\0"
    "\x10\x05" "N/M   ADD NOISE / BLOB\0"
    "\x11\x05" "D/F   DIRTY / CLEAN TILES\0"
    "\x12\x05" "1-9   ADD RANDOM ENTROPY\0"
    "\x15\x05" "SPACE   START RANDOM\0"
    "\x16\x09" "P   START PREMADE\0";

/* Keep startup text/layout code out of the game loop and optimize it for ROM. */
static uint8_t __attribute__((cold, noinline, minsize)) life_show_splash(void) {
    uint16_t i;
    uint16_t row;
    uint8_t key;

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
        uint8_t c = 0xF1; /* White on black for text. */
        if (i == 12 || i == 13) c = 0xB1; /* Light yellow for 0x60..0x6F gliders. */
        if (i == 14 || i == 15) c = 0x71; /* Cyan for 0x70..0x7F banner cells. */
        vdp_data(c);
    }

    vdp_set_write_addr(PATTERN_TABLE_ADDR + SPLASH_FONT_FIRST * 8);
    for (i = 0; i < sizeof(splash_font) / sizeof(splash_font[0]); ++i) {
        for (row = 0; row < 7; ++row) {
            vdp_data((uint8_t)(splash_font[i][row] << 2));
        }
        vdp_data(0); /* Eighth scanline provides text line spacing. */
    }

    /* Upload 16 2x2 cell-grid quadrant tiles twice: 0x60..0x6F (yellow) & 0x70..0x7F (cyan). */
    vdp_set_write_addr(PATTERN_TABLE_ADDR + SPLASH_YELLOW_BASE * 8);
    for (i = 0; i < 32; ++i) {
        uint16_t m = i & 15u;
        uint8_t top = (uint8_t)(((m & 1u) ? 0xE0u : 0u) | ((m & 2u) ? 0x0Eu : 0u));
        uint8_t bot = (uint8_t)(((m & 4u) ? 0xE0u : 0u) | ((m & 8u) ? 0x0Eu : 0u));
        for (row = 0; row < 8; ++row) {
            vdp_data((row == 3 || row == 7) ? 0u : (row < 3 ? top : bot));
        }
    }

    /* Build and stream the 32x8 banner rows (tile rows 1..8) to the Name Table. */
    {
        uint8_t grid[8 * CHAR_COLS];
        for (i = 0; i < sizeof(grid); ++i) {
            grid[i] = ' ';
        }
        splash_stamp_text2x(grid, 11, 0, "GAME OF", SPLASH_CYAN_BASE);
        splash_stamp_text2x(grid, 20, 9, "LIFE", SPLASH_CYAN_BASE);
        splash_stamp_text2x(grid, 10, 10, ":", SPLASH_YELLOW_BASE);
        splash_stamp_text2x(grid, 50, 10, ";", SPLASH_YELLOW_BASE);
        vdp_set_write_addr(NAME_TABLE_ADDR + 1 * CHAR_COLS);
        for (i = 0; i < sizeof(grid); ++i) {
            vdp_data(grid[i]);
        }
    }

    {
        const uint8_t *p = (const uint8_t *)splash_lines;
        while (*p) {
            uint16_t r = p[0];
            uint16_t c = p[1];
            p += 2;
            vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + r * CHAR_COLS + c));
            while (*p) {
                vdp_data(*p++);
            }
            ++p;
        }
    }
    vdp_display_on();

    /* Named checkpoints let the CPU tests inspect the finished screen and
     * prove that keyboard gating completes before any Life update. */
    asm volatile(".globl life_splash_ready\nlife_splash_ready:\n" ::: "memory");
    do {
        *(volatile uint8_t *)0x8374 = 0; /* Full keyboard scan, like the game. */
        key = (uint8_t)kbd_scan();
    } while (key != ' ' && key != 'P' && key != 'p');
    asm volatile(".globl life_splash_end\nlife_splash_end:\n" ::: "memory");
    return key;
}

#endif
