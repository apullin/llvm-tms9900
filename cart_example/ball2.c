/*
 * TI-99/4A Bouncing Ball Demo v2 (BOUNCEHIT)
 * Ball bounces around a border and increments edge characters on contact.
 */

#include <stdint.h>

__attribute__((section(".cart_header")))
const uint8_t cart_header[] = {
    0xAA, 0x01, 0x00, 0x00, 0x00, 0x00, 0x60, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x60, 0x24,
    9, 'B','O','U','N','C','E','H','I','T', 0x00, 0x00
};

/* _start is provided by crt0.s */

#define SCREEN_COLS 32
#define SCREEN_ROWS 24
#define CENTER_ROW (SCREEN_ROWS / 2)
#define CENTER_COL (SCREEN_COLS / 2)

#define BORDER_TOP_LEN SCREEN_COLS
#define BORDER_SIDE_LEN (SCREEN_ROWS - 2)
#define BORDER_TOTAL (BORDER_TOP_LEN * 2 + BORDER_SIDE_LEN * 2)

#define FP_SHIFT 8
#define FP_ONE ((int16_t)1 << FP_SHIFT)
#define FP_FROM_INT(x) ((int16_t)((x) << FP_SHIFT))

#define BALL2_VX_INIT FP_ONE
#define BALL2_VY_INIT FP_ONE
#define BALL2_JITTER_PCT 10
#define BALL2_JITTER_DELTA_Q8 ((FP_ONE + 5) / 10)
#define BALL2_ENERGY_SHIFT 2
#define BALL2_MIN_AXIS_S 0
#define BALL2_DELAY_MS 40

#define BALL2_JITTER_THRESHOLD ((uint16_t)(((uint32_t)BALL2_JITTER_PCT * (uint32_t)INT16_MAX) / 100u))
#if BALL2_ENERGY_SHIFT > 0
#define BALL2_DELTA_S ((uint16_t)((BALL2_JITTER_DELTA_Q8 + (1u << (BALL2_ENERGY_SHIFT - 1))) >> BALL2_ENERGY_SHIFT))
#else
#define BALL2_DELTA_S ((uint16_t)(BALL2_JITTER_DELTA_Q8))
#endif

#define DELAY_1MS_COUNT 8

/* ===== VDP ACCESS ===== */
#define VDP_CMD_PORT   0x8C02
#define VDP_DATA_PORT  0x8C00
#define GROM_ADDR_PORT 0x9C02
#define GROM_DATA_PORT 0x9800

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

static void grom_set_addr(uint16_t addr) {
    VDP_OUT(GROM_ADDR_PORT, (addr >> 8) & 0xFF);
    VDP_OUT(GROM_ADDR_PORT, addr & 0xFF);
}

static uint8_t grom_read(void) {
    uint16_t out;
    asm volatile("MOVB @%1, %0" : "=r"(out) : "i"(GROM_DATA_PORT) : "memory", "cc");
    return (uint8_t)(out >> 8);
}

static void vdp_set_text_mode_defaults(void) {
    vdp_set_register(0, 0x00);
    vdp_set_register(1, 0xE0);
    vdp_set_register(2, 0x00);
    vdp_set_register(3, 0x0E);
    vdp_set_register(4, 0x01);
    vdp_set_register(5, 0x06);
    vdp_set_register(6, 0x00);
}

static void vdp_load_stdchr(void) {
    uint16_t i;
    uint16_t j;

    /* Blank first 32 patterns so ASCII 0x20 maps to space. */
    vdp_set_write_addr(0x0800);
    for (i = 0; i < 32 * 8; i++) {
        vdp_data(0);
    }

    /* Load 96 characters from GROM at 0x06B4 into pattern index 32. */
    vdp_set_write_addr(0x0900);
    grom_set_addr(0x06B4);
    for (i = 0; i < 96; i++) {
        vdp_data(0);
        for (j = 0; j < 7; j++) {
            vdp_data(grom_read());
        }
    }
}

/* ===== RNG / MATH ===== */
static uint16_t rng_state = 0xACE1u;

static uint16_t rng_next(void) {
    uint16_t lsb = (uint16_t)(rng_state & 1u);
    rng_state >>= 1;
    if (lsb) {
        rng_state ^= 0xB400u;
    }
    return rng_state;
}

static uint16_t abs_u16(int16_t value) {
    return (uint16_t)((value < 0) ? -value : value);
}

static uint16_t isqrt16(uint16_t value) {
    uint16_t res = 0;
    uint16_t bit = (uint16_t)1u << 14;

    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        uint16_t trial = (uint16_t)(res + bit);
        if (value >= trial) {
            value = (uint16_t)(value - trial);
            res = (uint16_t)((res >> 1) + bit);
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* ===== DELAY ===== */
static volatile uint16_t delay_counter;

static void delay_1ms(void) {
    for (delay_counter = 0; delay_counter < DELAY_1MS_COUNT; delay_counter++) {
    }
}

static void delay_ms(uint16_t ms) {
    while (ms--) {
        delay_1ms();
    }
}

/* ===== SCREEN ===== */
static uint8_t border[BORDER_TOTAL];

static void vdp_clear_screen(void) {
    uint16_t i;

    vdp_set_write_addr(0x0000);
    for (i = 0; i < SCREEN_ROWS * SCREEN_COLS; i++) {
        vdp_data(' ');
    }
}

static void put_char(uint16_t row, uint16_t col, uint8_t c) {
    vdp_set_write_addr(row * SCREEN_COLS + col);
    vdp_data(c);
}

static int16_t border_index(uint16_t row, uint16_t col) {
    if (row == 0) {
        return (int16_t)col;
    }
    if (row == SCREEN_ROWS - 1) {
        return (int16_t)(BORDER_TOP_LEN + BORDER_SIDE_LEN + col);
    }
    if (col == SCREEN_COLS - 1 && row > 0 && row < SCREEN_ROWS - 1) {
        return (int16_t)(BORDER_TOP_LEN + (row - 1));
    }
    if (col == 0 && row > 0 && row < SCREEN_ROWS - 1) {
        return (int16_t)(BORDER_TOP_LEN + BORDER_SIDE_LEN + BORDER_TOP_LEN + (row - 1));
    }
    return -1;
}

static uint8_t border_next(uint8_t v) {
    if (v >= '0' && v <= '8') {
        return (uint8_t)(v + 1);
    }
    if (v == '9') {
        return 'A';
    }
    if (v >= 'A' && v <= 'Y') {
        return (uint8_t)(v + 1);
    }
    return '0';
}

static void init_border(void) {
    uint16_t i;

    for (i = 0; i < BORDER_TOTAL; i++) {
        border[i] = '0';
    }

    vdp_set_write_addr(0);
    for (i = 0; i < SCREEN_COLS; i++) {
        vdp_data('0');
    }

    vdp_set_write_addr((SCREEN_ROWS - 1) * SCREEN_COLS);
    for (i = 0; i < SCREEN_COLS; i++) {
        vdp_data('0');
    }

    for (i = 1; i < SCREEN_ROWS - 1; i++) {
        put_char(i, 0, '0');
        put_char(i, SCREEN_COLS - 1, '0');
    }
}

static void run_ball(void) {
    int16_t pos_x = FP_FROM_INT(CENTER_COL);
    int16_t pos_y = FP_FROM_INT(CENTER_ROW);
    int16_t vel_x = BALL2_VX_INIT;
    int16_t vel_y = BALL2_VY_INIT;
    int16_t sign_x = (vel_x < 0) ? -1 : 1;
    int16_t sign_y = (vel_y < 0) ? -1 : 1;
    const int16_t max_x = FP_FROM_INT(SCREEN_COLS - 1);
    const int16_t max_y = FP_FROM_INT(SCREEN_ROWS - 1);
    const uint16_t min_axis_s = BALL2_MIN_AXIS_S;
    uint16_t delta_s = BALL2_DELTA_S;
    uint16_t vx_s;
    uint16_t vy_s;
    uint16_t speed2_s;
    uint16_t max_axis_s;
    uint16_t ball_x;
    uint16_t ball_y;

    if (delta_s == 0) {
        delta_s = 1;
    }

    vx_s = (uint16_t)(abs_u16(vel_x) >> BALL2_ENERGY_SHIFT);
    vy_s = (uint16_t)(abs_u16(vel_y) >> BALL2_ENERGY_SHIFT);
    if (vx_s < min_axis_s) {
        vx_s = min_axis_s;
    }
    if (vy_s < min_axis_s) {
        vy_s = min_axis_s;
    }
    if (vx_s == 0 && vy_s == 0) {
        vx_s = 1;
        vy_s = 1;
    }

    speed2_s = (uint16_t)(vx_s * vx_s + vy_s * vy_s);
    {
        uint16_t min_sq = (uint16_t)(min_axis_s * min_axis_s);
        if (speed2_s < min_sq) {
            speed2_s = min_sq;
        }
        max_axis_s = isqrt16((uint16_t)(speed2_s - min_sq));
    }

    vel_x = (sign_x < 0) ? (int16_t)-(vx_s << BALL2_ENERGY_SHIFT)
                         : (int16_t)(vx_s << BALL2_ENERGY_SHIFT);
    vel_y = (sign_y < 0) ? (int16_t)-(vy_s << BALL2_ENERGY_SHIFT)
                         : (int16_t)(vy_s << BALL2_ENERGY_SHIFT);

    ball_x = (uint16_t)(pos_x >> FP_SHIFT);
    ball_y = (uint16_t)(pos_y >> FP_SHIFT);
    put_char(ball_y, ball_x, '+');

    for (;;) {
        int16_t idx = border_index(ball_y, ball_x);
        if (idx >= 0) {
            uint8_t next = border_next(border[idx]);
            border[idx] = next;
            put_char(ball_y, ball_x, next);
        } else {
            put_char(ball_y, ball_x, ' ');
        }

        int16_t next_x = (int16_t)(pos_x + vel_x);
        int16_t next_y = (int16_t)(pos_y + vel_y);
        uint8_t bounced_x = 0;
        uint8_t bounced_y = 0;

        if (next_x < 0) {
            next_x = 0;
            sign_x = 1;
            bounced_x = 1;
        } else if (next_x > max_x) {
            next_x = max_x;
            sign_x = -1;
            bounced_x = 1;
        }

        if (next_y < 0) {
            next_y = 0;
            sign_y = 1;
            bounced_y = 1;
        } else if (next_y > max_y) {
            next_y = max_y;
            sign_y = -1;
            bounced_y = 1;
        }

        if (bounced_x || bounced_y) {
            uint8_t adjust_x = bounced_x;
            uint8_t adjust_y = bounced_y;

            vx_s = (uint16_t)(abs_u16(vel_x) >> BALL2_ENERGY_SHIFT);
            vy_s = (uint16_t)(abs_u16(vel_y) >> BALL2_ENERGY_SHIFT);
            if (vx_s < min_axis_s) {
                vx_s = min_axis_s;
            }
            if (vy_s < min_axis_s) {
                vy_s = min_axis_s;
            }

            if ((uint16_t)(rng_next() >> 1) < BALL2_JITTER_THRESHOLD) {
                int16_t delta = (rng_next() & 0x8000u) ? (int16_t)delta_s
                                                      : (int16_t)-delta_s;

                if (adjust_x && adjust_y) {
                    if (rng_next() & 0x8000u) {
                        adjust_x = 0;
                    } else {
                        adjust_y = 0;
                    }
                }

                if (adjust_x) {
                    int16_t tmp = (int16_t)vx_s + delta;
                    if (tmp < (int16_t)min_axis_s) {
                        tmp = (int16_t)min_axis_s;
                    } else if ((uint16_t)tmp > max_axis_s) {
                        tmp = (int16_t)max_axis_s;
                    }
                    vx_s = (uint16_t)tmp;
                    vy_s = isqrt16((uint16_t)(speed2_s - (uint16_t)(vx_s * vx_s)));
                } else if (adjust_y) {
                    int16_t tmp = (int16_t)vy_s + delta;
                    if (tmp < (int16_t)min_axis_s) {
                        tmp = (int16_t)min_axis_s;
                    } else if ((uint16_t)tmp > max_axis_s) {
                        tmp = (int16_t)max_axis_s;
                    }
                    vy_s = (uint16_t)tmp;
                    vx_s = isqrt16((uint16_t)(speed2_s - (uint16_t)(vy_s * vy_s)));
                }
            }

            vel_x = (sign_x < 0) ? (int16_t)-(vx_s << BALL2_ENERGY_SHIFT)
                                 : (int16_t)(vx_s << BALL2_ENERGY_SHIFT);
            vel_y = (sign_y < 0) ? (int16_t)-(vy_s << BALL2_ENERGY_SHIFT)
                                 : (int16_t)(vy_s << BALL2_ENERGY_SHIFT);
        }

        pos_x = next_x;
        pos_y = next_y;
        ball_x = (uint16_t)(pos_x >> FP_SHIFT);
        ball_y = (uint16_t)(pos_y >> FP_SHIFT);

        put_char(ball_y, ball_x, '+');
        delay_ms(BALL2_DELAY_MS);
    }
}

void main(void) {
    vdp_set_register(7, 0xF4);
    vdp_set_text_mode_defaults();
    vdp_load_stdchr();
    vdp_clear_screen();
    init_border();
    run_ball();

    for (;;) { }
}
