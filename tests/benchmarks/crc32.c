/* CRC32 benchmark (ported from i8085 project)
 *
 * Computes CRC32 (ISO 3309 / ITU-T V.42) over a 32-byte buffer.
 * Uses the bit-by-bit algorithm (no lookup table) to keep code size
 * small while exercising 32-bit shifts, XOR, and byte processing.
 *
 * Expected CRC32 for the test vector: 0x116656A0
 */
#include <stdint.h>

/* Standard CRC32 polynomial (reversed / reflected) */
#define CRC32_POLY   0xEDB88320u

/* Expected CRC32 for bytes 0x00..0x1F */
#define EXPECTED_CRC 0x91267E8Au

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static const uint8_t input[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

__attribute__((noinline))
static uint32_t crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= (uint32_t)byte;
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (crc & 1u) {
            crc = (crc >> 1) ^ CRC32_POLY;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

volatile uint32_t result;

int main(void) {
    uint32_t crc = 0xFFFFFFFFu;

    for (uint8_t i = 0; i < 32; i++) {
        crc = crc32_byte(crc, input[i]);
    }

    crc ^= 0xFFFFFFFFu;
    result = crc;

    if (crc == EXPECTED_CRC)
        halt_ok();

    fail_loop();
    return 0;
}
