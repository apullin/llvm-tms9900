/*
 * TI-99/4A Text Demo - "LLVM TMS9900" centered
 * Compiled with LLVM TMS9900 backend
 */

#include <stdint.h>

/* ===== CARTRIDGE HEADER ===== */
/* Header must end before cart_entry at 0x6024 */
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
        /* Fall through to main - linker places .text after .cart_entry */
    );
}

/* ===== VDP ACCESS ===== */
/*
 * TI-99/4A VDP requires "symbolic" addressing mode (absolute address in
 * instruction). The backend doesn't support this yet, so we hand-encode MOVB.
 * TMS9900 MOVB transfers the HIGH byte of a register, hence the << 8 shifts.
 */
#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00

#define VDP_OUT(port, val) do { \
    uint16_t out = (uint16_t)(val) << 8; \
    asm volatile("MOVB %0, @%1" : : "r"(out), "i"(port) : "memory", "cc"); \
} while(0)

static void vdp_cmd(uint8_t val)  { VDP_OUT(VDP_CMD_PORT, val); }
static void vdp_data(uint8_t val) { VDP_OUT(VDP_DATA_PORT, val); }

static void vdp_set_register(uint8_t reg, uint8_t val) {
    vdp_cmd(val);
    vdp_cmd(reg | 0x80);  /* 0x80 = register write mode */
}

static void vdp_set_write_addr(uint16_t addr) {
    vdp_cmd(addr & 0xFF);                    /* low byte */
    vdp_cmd(((addr >> 8) & 0x3F) | 0x40);    /* high 6 bits + write flag */
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

/* ===== MAIN PROGRAM ===== */

void main(void) {
    /* Set colors: white text on dark blue */
    vdp_set_register(7, 0xF4);

    vdp_clear_screen();

    /* Screen is 32 columns x 24 rows */

    /* Top border */
    vdp_write_at(0, 0, "********************************");

    /* ASCII art "LLVM" - rows 2-6 */
    vdp_write_at(2, 4, "L     L     V   V  M   M");
    vdp_write_at(3, 4, "L     L     V   V  MM MM");
    vdp_write_at(4, 4, "L     L     V   V  M M M");
    vdp_write_at(5, 4, "L     L      V V   M   M");
    vdp_write_at(6, 4, "LLLLL LLLLL   V    M   M");

    /* TMS9900 label */
    vdp_write_at(8, 11, "* TMS9900 *");

    /* Divider */
    vdp_write_at(10, 0, "--------------------------------");

    /* Message - skip lines so text doesn't touch */
    vdp_write_at(12, 6, "HELLO FROM C CODE!");

    vdp_write_at(15, 8, "COMPILED USING");
    vdp_write_at(17, 6, "LLVM-TMS9900 BACKEND");

    /* Smiley */
    vdp_write_at(21, 15, ":)");

    /* Bottom border */
    vdp_write_at(23, 0, "********************************");

    for (;;) {}
}
