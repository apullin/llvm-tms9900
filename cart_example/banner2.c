/*
 * TI-99/4A "banner2" - LLVM TMS9900 banner with TI-99 intro screen style
 * Color bars at top and bottom, LLVM banner text in the middle.
 * Compiled with LLVM TMS9900 backend.
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
    12, 'L','L','V','M',' ','T','M','S','9','9','0','0', /* 0x14: Name */
    0x00                                  /* Pad to align */
};

/* ===== STARTUP CODE ===== */
__attribute__((naked, section(".cart_entry")))
void _start(void) {
    asm volatile(
        "LWPI 0x8300\n\t"       /* Workspace at scratchpad */
        "LIMI 0\n\t"            /* Disable interrupts */
        "LI R10, 0x83FE\n\t"    /* Stack pointer at top of scratchpad */
        /* Fall through to main */
    );
}

/* ===== VDP ACCESS ===== */
#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00
#define GROM_ADDR_PORT 0x9C02
#define GROM_DATA_PORT 0x9800

#define VDP_OUT(port, val) do { \
    uint16_t out = (uint16_t)(val) << 8; \
    asm volatile("MOVB %0, @%1" : : "r"(out), "i"(port) : "memory", "cc"); \
} while(0)

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

static void vdp_write_string(const char *s) {
    while (*s) {
        vdp_data(*s++);
    }
}

static void vdp_clear_screen(void) {
    vdp_set_write_addr(0x0000);
    for (int i = 0; i < 768; i++) {
        vdp_data(' ');
    }
}

static void vdp_write_at(uint16_t row, uint16_t col, const char *s) {
    vdp_set_write_addr(row * 32 + col);
    vdp_write_string(s);
}

/* ===== VDP INIT & FONT SETUP ===== */

/*
 * Graphics Mode I register setup.
 * Must be done before any VRAM writes so addresses are correct.
 */
static void vdp_init_gfx1(void) {
    vdp_set_register(0, 0x00);  /* Mode 0 */
    vdp_set_register(1, 0xE0);  /* Display ON, 16K VRAM, no interrupts */
    vdp_set_register(2, 0x00);  /* Name table at 0x0000 */
    vdp_set_register(3, 0x0E);  /* Color table at 0x0380 */
    vdp_set_register(4, 0x01);  /* Pattern table at 0x0800 */
    vdp_set_register(5, 0x06);  /* Sprite attribute at 0x1800 */
    vdp_set_register(6, 0x00);  /* Sprite pattern at 0x0000 */
}

static uint8_t grom_read(void) {
    uint16_t out;
    asm volatile("MOVB @%1, %0" : "=r"(out) : "i"(GROM_DATA_PORT) : "cc");
    return (uint8_t)(out >> 8);
}

static void grom_addr(uint16_t addr) {
    VDP_OUT(GROM_ADDR_PORT, (addr >> 8) & 0xFF);
    VDP_OUT(GROM_ADDR_PORT, addr & 0xFF);
}

/*
 * Load standard TI-99 font from GROM into VDP pattern table.
 * GROM 0x06B4 has 7-byte patterns for chars 32-127.
 * VDP needs 8 bytes per char: we prepend a blank row.
 */
static void load_font_from_grom(void) {
    grom_addr(0x06B4);
    vdp_set_write_addr(0x0800 + 32 * 8);  /* Pattern table, char 32 */

    for (int i = 0; i < 96; i++) {
        vdp_data(0x00);  /* Blank top row */
        for (int row = 0; row < 7; row++) {
            vdp_data(grom_read());
        }
    }
}

/*
 * Define solid block patterns for chars 128-255 (color bar tiles).
 * Each char is 8 bytes of 0xFF = solid filled rectangle.
 */
static void define_color_blocks(void) {
    vdp_set_write_addr(0x0800 + 128 * 8);
    for (int i = 0; i < 128 * 8; i++) {
        vdp_data(0x00);  /* All background color = solid block */
    }
}

/*
 * TI-99/4A color palette:
 *  0=transparent  1=black      2=med green   3=lt green
 *  4=dk blue      5=lt blue    6=dk red      7=cyan
 *  8=med red      9=lt red     A=dk yellow   B=lt yellow
 *  C=dark green   D=magenta    E=gray        F=white
 *
 * Color bar sequence (from the TI-99 intro screen, left to right):
 * Each block is 2 chars wide, 16 blocks across 32 columns.
 */
/*
 * Exact color sequence from the real TI-99/4A intro screen color table.
 * Dumped from VDP 0x038C-0x039B (groups 12-27 in the original).
 * We put them in groups 16-31 (chars 128-255).
 * Each byte is the color table entry — low nibble is background color,
 * and we use all-zero patterns so background fills the block.
 */
static const uint8_t bar_colors[16] = {
    0x06,  /* block  0: dk red       */
    0x03,  /* block  1: lt green     */
    0x01,  /* block  2: black        */
    0x0B,  /* block  3: lt yellow    */
    0x0C,  /* block  4: dk green     */
    0x0D,  /* block  5: magenta      */
    0x0F,  /* block  6: white        */
    0x04,  /* block  7: dk blue      */
    0x02,  /* block  8: med green    */
    0x0D,  /* block  9: magenta      */
    0x08,  /* block 10: med red      */
    0x0E,  /* block 11: gray         */
    0x05,  /* block 12: lt blue      */
    0x09,  /* block 13: lt red       */
    0x0A,  /* block 14: dk yellow    */
    0x06,  /* block 15: dk red       */
};

/*
 * Set up the VDP color table (32 bytes at 0x0380).
 * Groups 0-3  (chars 0-31):   unused, set to cyan
 * Groups 4-15 (chars 32-127): text = black on cyan (0x17)
 * Groups 16-31 (chars 128-255): solid color blocks for the bars
 */
static void setup_color_table(void) {
    vdp_set_write_addr(0x0380);

    /* Groups 0-3: unused chars, cyan background */
    for (int i = 0; i < 4; i++) {
        vdp_data(0x17);
    }

    /* Groups 4-15: text characters (black on cyan) */
    for (int i = 4; i < 16; i++) {
        vdp_data(0x17);
    }

    /* Groups 16-31: color bar blocks */
    for (int i = 0; i < 16; i++) {
        vdp_data(bar_colors[i]);
    }
}

/*
 * Draw a color bar row. Each of 16 blocks is 2 chars wide.
 * Block N uses a character from group (16+N), i.e. char (128 + N*8).
 */
static void draw_color_bar(uint16_t row) {
    vdp_set_write_addr(row * 32);
    for (int block = 0; block < 16; block++) {
        uint8_t ch = (uint8_t)(128 + block * 8);
        vdp_data(ch);
        vdp_data(ch);
    }
}

/* ===== MAIN PROGRAM ===== */

void main(void) {
    /* Initialize VDP for Graphics Mode I */
    vdp_init_gfx1();

    /* Cyan backdrop (like the TI-99 intro) */
    vdp_set_register(7, 0x07);

    /* Load font and set up graphics */
    load_font_from_grom();
    define_color_blocks();
    setup_color_table();
    vdp_clear_screen();

    /* Color bars - top (3 rows, like the TI-99 intro) */
    draw_color_bar(0);
    draw_color_bar(1);
    draw_color_bar(2);

    /* Color bars - bottom (3 rows) */
    draw_color_bar(21);
    draw_color_bar(22);
    draw_color_bar(23);

    /* ASCII art "LLVM" - rows 4-8 */
    vdp_write_at(4,  4, "L     L     V   V  M   M");
    vdp_write_at(5,  4, "L     L     V   V  MM MM");
    vdp_write_at(6,  4, "L     L     V   V  M M M");
    vdp_write_at(7,  4, "L     L      V V   M   M");
    vdp_write_at(8,  4, "LLLLL LLLLL   V    M   M");

    /* TMS9900 label */
    vdp_write_at(10, 10, "* TMS9900 *");

    /* Messages */
    vdp_write_at(12, 6, "HELLO FROM C CODE!");

    vdp_write_at(14, 8, "COMPILED USING");

    vdp_write_at(16, 5, "LLVM-TMS9900 BACKEND");

    /* Smiley */
    vdp_write_at(19, 14, ":)");

    for (;;) {}
}
