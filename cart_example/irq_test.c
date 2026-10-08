/*
 * TI-99/4A VBlank Counter Test (Graphics I)
 * Polls >8379 and updates a seconds counter (approx 60 Hz).
 */

#include <stdint.h>

__attribute__((section(".cart_header")))
const uint8_t cart_header[] = {
    0xAA, 0x01,
    0x00, 0x00,
    0x00, 0x00,
    0x60, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x60, 0x24,
    8, 'I','R','Q','_','T','E','S','T',
    0x00
};

#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00
#define GROM_ADDR_PORT 0x9C02
#define GROM_DATA_PORT 0x9800

#define COLOR_FG 0x0F
#define COLOR_BG 0x01

#define NAME_TABLE_ADDR    0x0000
#define COLOR_TABLE_ADDR   0x0380
#define PATTERN_TABLE_ADDR 0x0800

#define CHAR_COLS 32

static volatile uint8_t * const vdp_counter = (volatile uint8_t *)0x8379;

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
    vdp_set_register(0, 0x00);
    vdp_set_register(1, 0xE0);
    vdp_set_register(2, 0x00);
    vdp_set_register(3, 0x0E);
    vdp_set_register(4, 0x01);
    vdp_set_register(5, 0x06);
    vdp_set_register(6, 0x00);
    vdp_set_register(7, COLOR_BG);
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

static void vdp_write_char(uint16_t row, uint16_t col, uint8_t ch) {
    vdp_set_write_addr((uint16_t)(NAME_TABLE_ADDR + row * CHAR_COLS + col));
    vdp_data(ch);
}

static void vdp_write_dec3(uint16_t row, uint16_t col, uint16_t val) {
    uint8_t hundreds = 0;
    uint8_t tens = 0;
    uint8_t ones = 0;

    while (val >= 100) {
        val -= 100;
        hundreds++;
    }
    while (val >= 10) {
        val -= 10;
        tens++;
    }
    ones = (uint8_t)val;

    vdp_write_char(row, col, (uint8_t)('0' + hundreds));
    vdp_write_char(row, (uint16_t)(col + 1), (uint8_t)('0' + tens));
    vdp_write_char(row, (uint16_t)(col + 2), (uint8_t)('0' + ones));
}

void main(void) {
    uint8_t last = *vdp_counter;
    uint16_t ticks = 0;
    uint16_t seconds = 0;

    vdp_init_gfx1();
    vdp_load_stdchr();
    vdp_init_color_table();
    vdp_clear_screen();

    vdp_write_at(8, 6, "VBL COUNT:");
    vdp_write_at(10, 6, "SECONDS:");
    vdp_write_dec3(10, 15, 0);

    asm volatile("LIMI 2");

    for (;;) {
        uint8_t cur = *vdp_counter;
        if (cur != last) {
            last = cur;
            ticks++;
            if (ticks >= 60) {
                ticks = 0;
                seconds++;
                if (seconds > 999) {
                    seconds = 0;
                }
                vdp_write_dec3(10, 15, seconds);
            }
        }
    }
}
