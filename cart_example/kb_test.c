/*
 * TI-99/4A Keyboard Test (Graphics I)
 * Shows raw keycode bytes and a printable ASCII hint.
 */

#include <stdint.h>

/* ===== CARTRIDGE HEADER ===== */
__attribute__((section(".cart_header")))
const uint8_t cart_header[] = {
    0xAA, 0x01,
    0x00, 0x00,
    0x00, 0x00,
    0x60, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x60, 0x24,
    7, 'K','B','_','T','E','S','T',
    0x00
};

/* _start is provided by crt0.s */

#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00
#define GROM_ADDR_PORT 0x9C02
#define GROM_DATA_PORT 0x9800

#define COLOR_FG 0x0F /* white */
#define COLOR_BG 0x01 /* black */
#define COLOR_BORDER_IDLE 0x01
#define COLOR_BORDER_KEY  0x0E

#define NAME_TABLE_ADDR   0x0000
#define COLOR_TABLE_ADDR  0x0380
#define PATTERN_TABLE_ADDR 0x0800

#define CHAR_COLS 32

static volatile uint8_t * const kbd_state = (volatile uint8_t *)0x8374;
extern uint16_t kbd_scan(void);

#define VDP_OUT(port, val) do { \
    uint16_t out = (uint16_t)(val) << 8; \
    asm volatile("MOVB %0, @%1" : : "r"(out), "i"(port) : "memory", "cc"); \
} while (0)

static void vdp_cmd(uint8_t val)  { VDP_OUT(VDP_CMD_PORT, val); }
static void vdp_data(uint8_t val) { VDP_OUT(VDP_DATA_PORT, val); }

static uint8_t grom_read(void) {
    uint16_t out;
    asm volatile("MOVB @%1, %0" : "=r"(out) : "i"(GROM_DATA_PORT) : "cc");
    return (uint8_t)(out >> 8);
}

static void grom_addr(uint16_t addr) {
    VDP_OUT(GROM_ADDR_PORT, (addr >> 8) & 0xFF);
    VDP_OUT(GROM_ADDR_PORT, addr & 0xFF);
}

static void vdp_set_register(uint8_t reg, uint8_t val) {
    vdp_cmd(val);
    vdp_cmd(reg | 0x80);
}

static void vdp_set_write_addr(uint16_t addr) {
    vdp_cmd(addr & 0xFF);
    vdp_cmd(((addr >> 8) & 0x3F) | 0x40);
}

static void vdp_init_gfx1(void) {
    vdp_set_register(0, 0x00); /* Graphics I */
    vdp_set_register(1, 0xE0); /* Display on, 16K */
    vdp_set_register(2, 0x00); /* Name table @ 0x0000 */
    vdp_set_register(3, 0x0E); /* Color table @ 0x0380 */
    vdp_set_register(4, 0x01); /* Pattern table @ 0x0800 */
    vdp_set_register(5, 0x06); /* Sprite attribute @ 0x0300 */
    vdp_set_register(6, 0x00); /* Sprite pattern @ 0x0000 */
    vdp_set_register(7, COLOR_BORDER_IDLE);
}

static void vdp_load_stdchr(void) {
    uint16_t i;

    grom_addr(0x06B4);
    vdp_set_write_addr(PATTERN_TABLE_ADDR + 0x0100);

    for (i = 0; i < 96; i++) {
        uint16_t row;
        vdp_data(0x00);
        for (row = 0; row < 7; row++) {
            vdp_data(grom_read());
        }
    }
}

static void vdp_init_color_table(void) {
    uint16_t i;
    uint8_t color = (uint8_t)((COLOR_FG << 4) | (COLOR_BG & 0x0F));

    vdp_set_write_addr(COLOR_TABLE_ADDR);
    for (i = 0; i < 32; i++) {
        vdp_data(color);
    }
}

static void vdp_clear_screen(void) {
    uint16_t i;
    vdp_set_write_addr(NAME_TABLE_ADDR);
    for (i = 0; i < 768; i++) {
        vdp_data(0x20);
    }
}

static void vdp_write_at(uint16_t row, uint16_t col, const char *s) {
    vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + row * CHAR_COLS + col));
    while (*s) {
        vdp_data((uint8_t)*s++);
    }
}

static void vdp_write_hex(uint16_t row, uint16_t col, uint8_t val) {
    uint8_t hi = (uint8_t)(val >> 4);
    uint8_t lo = (uint8_t)(val & 0x0F);
    vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + row * CHAR_COLS + col));
    vdp_data((uint8_t)(hi < 10 ? ('0' + hi) : ('A' + (hi - 10))));
    vdp_data((uint8_t)(lo < 10 ? ('0' + lo) : ('A' + (lo - 10))));
}

static void vdp_write_char(uint16_t row, uint16_t col, uint8_t ch) {
    vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + row * CHAR_COLS + col));
    vdp_data(ch);
}

void main(void) {
    uint8_t last = 0;

    vdp_init_gfx1();
    vdp_load_stdchr();
    vdp_init_color_table();
    vdp_clear_screen();

    vdp_write_at(2, 4, "RAW:");
    vdp_write_at(3, 4, "LAST:");
    vdp_write_at(4, 4, "ASCII:");
    vdp_write_at(6, 4, "8374:");
    vdp_write_at(7, 4, "8375:");
    vdp_write_at(8, 4, "8376:");
    vdp_write_at(9, 4, "8377:");
    vdp_write_at(10, 4, "837C:");

    for (;;) {
        uint8_t b0;
        uint8_t b1;
        uint8_t b2;
        uint8_t b3;
        uint8_t b8;
        uint8_t code;

        kbd_state[0] = 0;
        code = (uint8_t)kbd_scan();

        if (code != 0) {
            last = code;
        }

        b0 = kbd_state[0];
        b1 = kbd_state[1];
        b2 = kbd_state[2];
        b3 = kbd_state[3];
        b8 = kbd_state[8];

        vdp_write_hex(2, 9, code);
        vdp_write_hex(3, 10, last);
        vdp_write_char(4, 11,
                       (uint8_t)((last >= 0x20 && last <= 0x7E) ? last : '.'));
        vdp_write_hex(6, 10, b0);
        vdp_write_hex(7, 10, b1);
        vdp_write_hex(8, 10, b2);
        vdp_write_hex(9, 10, b3);
        vdp_write_hex(10, 10, b8);

        vdp_set_register(7, code ? COLOR_BORDER_KEY : COLOR_BORDER_IDLE);
    }
}
