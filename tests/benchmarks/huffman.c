/* Huffman codec benchmark
 *
 * Builds a Huffman tree from input data, encodes it into a bitstream,
 * decodes the bitstream back, verifies round-trip correctness, and
 * checksums the encoded bitstream for deterministic verification.
 *
 * Exercises: bit manipulation, byte extraction, conditional branches,
 * array traversal, tree building, and bitstream I/O.
 *
 * 19 unique symbols, 48-byte input, 188-bit encoded bitstream.
 * Expected encoded checksum: 0x7FF4
 */
#include <stdint.h>

/* --- Freestanding helpers --- */
typedef unsigned int size_t;

__attribute__((noinline))
static void *my_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/* --- Halt helpers --- */
__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* --- Configuration --- */
#define INPUT_LEN    48
#define MAX_SYMBOLS  32   /* max unique byte values in input */
#define MAX_NODES    63   /* 2*MAX_SYMBOLS - 1 */
#define MAX_CODE_LEN 16   /* max Huffman code length in bits */
#define BITSTREAM_CAP 64  /* max encoded bytes */

/* --- Input data: 48 bytes with varying frequencies ---
 * Chosen to create an interesting Huffman tree:
 * 'a' appears 8x, 'b' 6x, 'c' 5x, 'd' 4x, 'e' 3x, 'f' 3x,
 * 'g' 2x, 'h' 2x, 'i' 2x, 'j' 2x, 'k' 2x, 'l' 2x,
 * 'm' 1x, 'n' 1x, 'o' 1x, 'p' 1x, 'q' 1x, 'r' 1x, 's' 1x
 */
static const uint8_t input_data[INPUT_LEN] = {
    'a','b','c','d','a','e','f','g',
    'a','b','c','d','h','i','j','k',
    'a','b','c','e','f','l','m','n',
    'a','b','c','d','a','o','p','q',
    'a','b','c','d','e','f','g','r',
    'a','b','c','h','i','j','k','s'
};

/* --- Data structures --- */

/* Huffman tree node */
typedef struct {
    uint16_t freq;      /* frequency count */
    uint8_t  symbol;    /* leaf symbol (valid if left==right==0xFF) */
    uint8_t  left;      /* index of left child (0xFF = none) */
    uint8_t  right;     /* index of right child (0xFF = none) */
    uint8_t  pad;
} huff_node_t;

/* Huffman code for a symbol */
typedef struct {
    uint16_t bits;      /* the code bits (LSB-first packed) */
    uint8_t  len;       /* code length in bits */
    uint8_t  pad;
} huff_code_t;

/* --- Static allocations --- */
static huff_node_t nodes[MAX_NODES];
static huff_code_t codes[MAX_SYMBOLS];
static uint8_t sym_map[MAX_SYMBOLS];       /* symbol index -> byte value */
static uint8_t sym_lookup[256];            /* byte value -> symbol index (0xFF=unused) */
static uint8_t encoded[BITSTREAM_CAP];     /* encoded bitstream */
static uint8_t decoded[INPUT_LEN];         /* decoded output */

/* --- Step 1: Build frequency table --- */
__attribute__((noinline))
static uint8_t build_freq_table(const uint8_t *data, uint8_t len) {
    uint16_t freq[256];
    uint8_t i;
    uint16_t j;

    /* Clear frequency table */
    for (j = 0; j < 256; j++) freq[j] = 0;

    /* Count frequencies */
    for (i = 0; i < len; i++) {
        freq[data[i]]++;
    }

    /* Build symbol map: only include symbols with freq > 0 */
    uint8_t nsym = 0;
    my_memset(sym_lookup, 0xFF, 256);

    for (j = 0; j < 256; j++) {
        if (freq[j] > 0 && nsym < MAX_SYMBOLS) {
            sym_map[nsym] = (uint8_t)j;
            sym_lookup[j] = nsym;

            nodes[nsym].freq = freq[j];
            nodes[nsym].symbol = (uint8_t)j;
            nodes[nsym].left = 0xFF;
            nodes[nsym].right = 0xFF;
            nodes[nsym].pad = 0;
            nsym++;
        }
    }

    return nsym;
}

/* --- Step 2: Build Huffman tree ---
 * Uses a simple O(n^2) approach: repeatedly find two lowest-freq nodes
 * and merge them. Uses freq=0xFFFF to mark consumed nodes.
 */
__attribute__((noinline))
static uint8_t build_tree(uint8_t nsym) {
    uint8_t total = nsym;  /* next free node index */
    uint8_t remaining = nsym;

    while (remaining > 1) {
        /* Find the two nodes with lowest frequency */
        uint8_t min1 = 0xFF, min2 = 0xFF;
        uint16_t f1 = 0xFFFF, f2 = 0xFFFF;
        uint8_t i;

        for (i = 0; i < total; i++) {
            if (nodes[i].freq == 0xFFFF) continue; /* already consumed */
            if (nodes[i].freq < f1) {
                /* New minimum: old min1 becomes min2 */
                min2 = min1;
                f2 = f1;
                min1 = i;
                f1 = nodes[i].freq;
            } else if (nodes[i].freq < f2) {
                min2 = i;
                f2 = nodes[i].freq;
            }
        }

        if (min1 == 0xFF || min2 == 0xFF) break;

        /* Create parent node */
        if (total >= MAX_NODES) break;

        nodes[total].freq = f1 + f2;
        nodes[total].symbol = 0;
        nodes[total].left = min1;
        nodes[total].right = min2;
        nodes[total].pad = 0;

        /* Mark children as consumed */
        nodes[min1].freq = 0xFFFF;
        nodes[min2].freq = 0xFFFF;

        total++;
        remaining--;
    }

    return (uint8_t)(total - 1); /* root index */
}

/* --- Step 3: Generate codes by tree traversal ---
 * Uses bit_mask parameter instead of computing 1<<depth to avoid
 * TMS9900 SLA-by-zero issue (SLA Rw,0 with R0=0 shifts by 16, not 0).
 */
__attribute__((noinline))
static void generate_codes(uint8_t node_idx, uint16_t code,
                           uint16_t bit_mask, uint8_t depth) {
    /* Leaf node? */
    if (nodes[node_idx].left == 0xFF && nodes[node_idx].right == 0xFF) {
        uint8_t si = sym_lookup[nodes[node_idx].symbol];
        if (si < MAX_SYMBOLS) {
            codes[si].bits = code;
            codes[si].len = depth;
        }
        return;
    }

    /* Recurse left (append 0) */
    if (nodes[node_idx].left != 0xFF) {
        generate_codes(nodes[node_idx].left, code,
                       (uint16_t)(bit_mask << 1), depth + 1);
    }

    /* Recurse right (append 1) */
    if (nodes[node_idx].right != 0xFF) {
        generate_codes(nodes[node_idx].right,
                       (uint16_t)(code | bit_mask),
                       (uint16_t)(bit_mask << 1), depth + 1);
    }
}

/* --- Step 4: Encode input data to bitstream ---
 * Uses running masks to avoid variable-shift-by-zero (TMS9900 SLA quirk).
 */
__attribute__((noinline))
static uint16_t encode_data(const uint8_t *data, uint8_t len,
                            uint8_t *out, uint8_t out_cap) {
    uint16_t bit_pos = 0;  /* total bits written */
    uint8_t i;

    my_memset(out, 0, out_cap);

    for (i = 0; i < len; i++) {
        uint8_t si = sym_lookup[data[i]];
        if (si >= MAX_SYMBOLS) return 0; /* error */

        uint16_t cbits = codes[si].bits;
        uint8_t  clen  = codes[si].len;
        uint8_t  b;
        uint16_t code_mask = 1;  /* running mask for code bits */

        /* Write code bits one at a time */
        for (b = 0; b < clen; b++) {
            uint16_t byte_idx = (uint16_t)(bit_pos >> 3);
            uint8_t  bit_off  = (uint8_t)(bit_pos & 7);

            if (byte_idx >= out_cap) return 0; /* overflow */

            if (cbits & code_mask) {
                /* Use lookup for output bit mask to avoid shift-by-zero */
                static const uint8_t bit_masks[8] = {
                    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
                };
                out[byte_idx] |= bit_masks[bit_off];
            }
            code_mask = (uint16_t)(code_mask << 1);
            bit_pos++;
        }
    }

    return bit_pos; /* total bits written */
}

/* --- Step 5: Decode bitstream back to original data ---
 * Uses bit mask lookup to avoid variable-shift-by-zero (TMS9900 SLA quirk).
 */
__attribute__((noinline))
static uint8_t decode_data(const uint8_t *bitstream, uint16_t total_bits,
                           uint8_t root, uint8_t *out, uint8_t out_cap) {
    static const uint8_t bit_masks[8] = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
    };
    uint16_t bit_pos = 0;
    uint8_t out_pos = 0;

    while (bit_pos < total_bits && out_pos < out_cap) {
        uint8_t cur = root;

        /* Walk tree until we reach a leaf */
        while (nodes[cur].left != 0xFF || nodes[cur].right != 0xFF) {
            if (bit_pos >= total_bits) return 0; /* truncated */

            uint16_t byte_idx = (uint16_t)(bit_pos >> 3);
            uint8_t  bit_off  = (uint8_t)(bit_pos & 7);
            uint8_t  bit_val  = bitstream[byte_idx] & bit_masks[bit_off];

            if (bit_val == 0) {
                cur = nodes[cur].left;
            } else {
                cur = nodes[cur].right;
            }
            bit_pos++;

            if (cur == 0xFF) return 0; /* invalid tree */
        }

        out[out_pos++] = nodes[cur].symbol;
    }

    return out_pos;
}

/* --- Step 6: Checksum the encoded bitstream --- */
__attribute__((noinline))
static uint16_t checksum_stream(const uint8_t *data, uint8_t nbytes) {
    /* Simple 16-bit hash: rotate-and-XOR */
    uint16_t h = 0x5A5A;
    uint8_t i;

    for (i = 0; i < nbytes; i++) {
        /* Rotate left by 3 */
        h = (uint16_t)((h << 3) | (h >> 13));
        h ^= (uint16_t)data[i];
        h += (uint16_t)(i + 1);
    }

    return h;
}

/* --- Main --- */
volatile uint16_t result_checksum;
volatile uint16_t result_bits;
volatile uint8_t  result_nsym;

int main(void) {
    /* Step 1: Build frequency table */
    uint8_t nsym = build_freq_table(input_data, INPUT_LEN);
    result_nsym = nsym;

    if (nsym < 2 || nsym > MAX_SYMBOLS)
        fail_loop();

    /* Step 2: Build Huffman tree */
    uint8_t root = build_tree(nsym);

    /* Step 3: Generate Huffman codes */
    my_memset(codes, 0, sizeof(codes));
    generate_codes(root, 0, 1, 0);

    /* Verify all symbols got a code */
    {
        uint8_t i;
        for (i = 0; i < nsym; i++) {
            if (codes[i].len == 0)
                fail_loop();
        }
    }

    /* Step 4: Encode */
    uint16_t total_bits = encode_data(input_data, INPUT_LEN, encoded, BITSTREAM_CAP);
    result_bits = total_bits;

    if (total_bits == 0)
        fail_loop();

    /* Step 5: Decode */
    uint8_t dec_len = decode_data(encoded, total_bits, root, decoded, INPUT_LEN);

    if (dec_len != INPUT_LEN)
        fail_loop();

    /* Step 6: Verify round-trip */
    {
        uint8_t i;
        for (i = 0; i < INPUT_LEN; i++) {
            if (decoded[i] != input_data[i])
                fail_loop();
        }
    }

    /* Step 7: Checksum the encoded bitstream */
    uint8_t encoded_bytes = (uint8_t)((total_bits + 7) >> 3);
    uint16_t checksum = checksum_stream(encoded, encoded_bytes);
    result_checksum = checksum;

    /* Verify checksum matches expected value */
    if (checksum == 0x7FF4u)
        halt_ok();

    fail_loop();
    return 0;
}
