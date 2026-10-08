#include <stdint.h>

extern unsigned byte_gt_one(uint16_t);
extern unsigned byte_lt_128(uint16_t);
extern uint16_t store_byte_return_word(uint16_t, volatile unsigned char *);
extern void store_masked_alias(uint16_t);
extern unsigned shift_carry(uint16_t);
extern unsigned abs_positive(uint16_t);

static volatile unsigned char stored;

int main(void) {
    for (unsigned byte = 0; byte != 256; ++byte) {
        uint16_t input = (uint16_t)(byte << 8);
        if (byte_gt_one(input) != (byte > 1))
            return 1;
        if (byte_lt_128(input) != (byte < 128))
            return 2;
    }

    volatile unsigned char *expected = (volatile unsigned char *)0xa100;
    volatile unsigned char *wrong = (volatile unsigned char *)0xa123;
    *expected = 0;
    *wrong = 0x55;
    store_masked_alias(0xa123);
    if (*expected != 0xa1 || *wrong != 0x55)
        return 3;

    uint16_t value = 0;
    do {
        uint16_t result = store_byte_return_word(value, &stored);
        if (result != (uint16_t)(value << 8) || stored != (unsigned char)value)
            return 4;
        if (shift_carry(value) != ((value >> 7) & 1u))
            return 5;
        if (abs_positive(value) != (value != 0 && value != 0x8000u))
            return 6;
        ++value;
    } while (value != 0);
    return 0;
}
