#include <stddef.h>
#include <stdint.h>

extern void *memset(void *, int, size_t);

/* Keep the call out of Clang's builtin expansion: exercise the runtime ABI,
 * whose second argument is a complete int rather than a pre-truncated byte. */
static void *(*volatile fill)(void *, int, size_t) = memset;
static unsigned char buffer[24];

int main(void) {
    static const uint16_t values[] = {
        0, 1, 0x7f, 0x80, 0xff, 0x100, 0x1234, 0xffff
    };

    for (unsigned v = 0; v < sizeof(values) / sizeof(values[0]); ++v) {
        for (unsigned offset = 0; offset != 2; ++offset) {
            for (unsigned n = 0; n != 18; ++n) {
                for (unsigned i = 0; i < sizeof(buffer); ++i)
                    buffer[i] = 0xa5;
                if (fill(buffer + offset, (int)values[v], n) != buffer + offset)
                    return 1;
                for (unsigned i = 0; i < sizeof(buffer); ++i) {
                    unsigned char expected = i >= offset && i < offset + n
                        ? (unsigned char)values[v] : 0xa5;
                    if (buffer[i] != expected)
                        return 2;
                }
            }
        }
    }

    /* Every bit pattern of the target's 16-bit int must undergo the same
     * unsigned-char conversion, including negative values. */
    uint16_t value = 0;
    do {
        if (fill(buffer, (int)value, 4) != buffer)
            return 3;
        for (unsigned i = 0; i != 4; ++i)
            if (buffer[i] != (unsigned char)value)
                return 4;
        ++value;
    } while (value != 0);
    return 0;
}
