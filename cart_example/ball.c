/*
 * TI-99/4A Bouncing Ball Demo
 * Compiled with LLVM TMS9900 backend
 * Uses character graphics for a simple bouncing ball animation
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
    12, 'B','O','U','N','C','I','N','G','B','A','L','L',
    0x00                                  /* Pad to align */
};

/* _start is provided by crt0.s */

/* ===== VDP ACCESS ===== */
static void vdp_write_addr(uint16_t val_shifted) {
    asm volatile("MOVB %0, @%1" : : "r"(val_shifted), "i"(0x8C02) : "memory", "cc");
}

static void vdp_write_data(uint16_t val_shifted) {
    asm volatile("MOVB %0, @%1" : : "r"(val_shifted), "i"(0x8C00) : "memory", "cc");
}

static void vdp_set_register(uint8_t reg, uint8_t val) {
    vdp_write_addr((uint16_t)val << 8);
    vdp_write_addr((uint16_t)(reg | 0x80) << 8);
}

static void vdp_set_write_addr(uint16_t addr) {
    vdp_write_addr((uint16_t)(addr & 0xFF) << 8);
    vdp_write_addr((uint16_t)(((addr >> 8) & 0x3F) | 0x40) << 8);
}

/* ===== DELAY FUNCTION ===== */
static volatile uint16_t delay_counter;  /* Global to avoid stack issues */

static void delay(uint16_t count) {
    for (delay_counter = 0; delay_counter < count; delay_counter++) {
        /* Busy wait */
    }
}

/* ===== SCREEN FUNCTIONS ===== */
static uint16_t screen_counter;  /* Global loop counter */

static void clear_screen(void) {
    vdp_set_write_addr(0x0000);
    for (screen_counter = 0; screen_counter < 768; screen_counter++) {
        vdp_write_data((uint16_t)' ' << 8);
    }
}

static void put_char(uint16_t row, uint16_t col, uint16_t c) {
    uint16_t addr = row * 32 + col;
    vdp_set_write_addr(addr);
    vdp_write_data(c << 8);
}

/* ===== BALL CHARACTER PATTERN ===== */
/* Define a ball pattern at character 128 (0x80) */
static const uint8_t ball_pattern[] = {
    0x3C,  /* ..XXXX.. */
    0x7E,  /* .XXXXXX. */
    0xFF,  /* XXXXXXXX */
    0xFF,  /* XXXXXXXX */
    0xFF,  /* XXXXXXXX */
    0xFF,  /* XXXXXXXX */
    0x7E,  /* .XXXXXX. */
    0x3C   /* ..XXXX.. */
};

static uint16_t pattern_counter;  /* Global loop counter */

static void define_ball_char(void) {
    /* Pattern table is at 0x0800 in default setup */
    /* Character 128 pattern starts at 0x0800 + 128*8 = 0x0C00 */
    vdp_set_write_addr(0x0C00);
    for (pattern_counter = 0; pattern_counter < 8; pattern_counter++) {
        vdp_write_data((uint16_t)ball_pattern[pattern_counter] << 8);
    }
}

/* ===== GLOBAL STATE ===== */
static int16_t ball_x, ball_y;   /* Ball position */
static int16_t ball_dx, ball_dy; /* Ball velocity */

/* ===== MAIN PROGRAM ===== */

void main(void) {
    /* Set colors: white on dark blue (F=white, 4=dark blue) */
    vdp_set_register(7, 0xF4);

    /* Clear screen */
    clear_screen();

    /* Define ball character */
    define_ball_char();

    /* No character border - use full screen, VDP border is visible */

    /* Initialize ball position and velocity */
    ball_x = 15;
    ball_y = 11;
    ball_dx = 1;
    ball_dy = 1;

    /* Animation loop */
    for (;;) {
        /* Clear old ball position */
        put_char(ball_y, ball_x, ' ');

        /* Update position */
        ball_x += ball_dx;
        ball_y += ball_dy;

        /* Bounce off screen edges (0-31 x 0-23) */
        if (ball_x <= 0 || ball_x >= 31) {
            ball_dx = -ball_dx;
        }
        if (ball_y <= 0 || ball_y >= 23) {
            ball_dy = -ball_dy;
        }

        /* Draw ball at new position - use 'O' as ball character */
        put_char(ball_y, ball_x, 'O');

        /* Delay for visibility */
        delay(2000);
    }
}
