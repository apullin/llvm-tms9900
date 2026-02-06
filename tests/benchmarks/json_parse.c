/* JSON parse benchmark (ported from i8085 project)
 *
 * Parses a small JSON string using a minimal tokenizer.
 * Exercises string processing, byte scanning, comparisons,
 * control flow, and pointer arithmetic.
 *
 * Test string: {"name":"TMS9900","bits":16,"year":1979}
 * Expected tokens: 7  (1 object + 3 key strings + 2 primitives + 1 value string)
 * Expected "bits" value: 16
 */
#include <stdint.h>

#define MAX_TOKENS   16
#define INPUT_MAX    48

#define EXPECTED_TOKEN_COUNT 7
#define EXPECTED_BITS_VALUE  16

/* ---- Minimal JSON tokenizer ---- */

#define TOK_OBJECT    1
#define TOK_STRING    3
#define TOK_PRIMITIVE 4

typedef struct {
    uint8_t  type;
    uint8_t  start;
    uint8_t  end;      /* exclusive */
    uint8_t  pad;
} tok_t;

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* Scan past a quoted string. pos points to the opening quote.
 * Returns position of closing quote, or 0 on error. */
__attribute__((noinline))
static uint8_t scan_string(const char *js, uint8_t len, uint8_t pos) {
    uint8_t p = (uint8_t)(pos + 1);
    while (p < len) {
        if (js[p] == '\\') {
            p = (uint8_t)(p + 2);
            if (p > len) return 0;
            continue;
        }
        if (js[p] == '"') {
            return p;
        }
        p++;
    }
    return 0;
}

/* Scan past a primitive. pos points to the first char.
 * Returns position one past the last char. */
__attribute__((noinline))
static uint8_t scan_primitive(const char *js, uint8_t len, uint8_t pos) {
    while (pos < len) {
        uint8_t c = (uint8_t)js[pos];
        if (c == ',' || c == '}' || c == ']' || c == ':' || c == ' ') {
            return pos;
        }
        pos++;
    }
    return pos;
}

/* Tokenize JSON. Returns number of tokens, or 0 on error. */
__attribute__((noinline))
static uint8_t json_tokenize(const char *js, uint8_t len,
                              tok_t *tokens, uint8_t max_tok) {
    uint8_t pos = 0;
    uint8_t ntok = 0;

    while (pos < len) {
        uint8_t c = (uint8_t)js[pos];

        if (c == 0) {
            break;
        } else if (c == '{' || c == '}' || c == ' ' || c == ':'
                   || c == '\t' || c == '\n' || c == '\r') {
            if (c == '{') {
                if (ntok >= max_tok) return 0;
                tokens[ntok].type  = TOK_OBJECT;
                tokens[ntok].start = pos;
                tokens[ntok].end   = 0;
                tokens[ntok].pad   = 0;
                ntok++;
            } else if (c == '}') {
                uint8_t k = ntok;
                while (k > 0) {
                    k--;
                    if (tokens[k].type == TOK_OBJECT && tokens[k].end == 0) {
                        tokens[k].end = (uint8_t)(pos + 1);
                        break;
                    }
                }
            }
            pos++;
        } else if (c == ',') {
            pos++;
        } else if (c == '"') {
            uint8_t end = scan_string(js, len, pos);
            if (end == 0) return 0;
            if (ntok >= max_tok) return 0;
            tokens[ntok].type  = TOK_STRING;
            tokens[ntok].start = (uint8_t)(pos + 1);
            tokens[ntok].end   = end;
            tokens[ntok].pad   = 0;
            ntok++;
            pos = (uint8_t)(end + 1);
        } else {
            uint8_t end = scan_primitive(js, len, pos);
            if (ntok >= max_tok) return 0;
            tokens[ntok].type  = TOK_PRIMITIVE;
            tokens[ntok].start = pos;
            tokens[ntok].end   = end;
            tokens[ntok].pad   = 0;
            ntok++;
            pos = end;
        }
    }

    return ntok;
}

/* ---- Helpers ---- */

__attribute__((noinline))
static uint8_t my_strlen8(const char *s) {
    uint8_t n = 0;
    while (s[n] != '\0' && n < 255) {
        n++;
    }
    return n;
}

/* Check if token matches a key */
__attribute__((noinline))
static uint8_t tok_eq(const char *js, const tok_t *tok, const char *key, uint8_t klen) {
    if (tok->type != TOK_STRING) return 0;
    uint8_t tlen = tok->end - tok->start;
    if (tlen != klen) return 0;
    uint8_t i = 0;
    while (i < tlen) {
        if (js[(uint8_t)(tok->start + i)] != key[i]) return 0;
        i++;
    }
    return 1;
}

/* Parse primitive token as small positive integer */
__attribute__((noinline))
static uint16_t tok_int(const char *js, const tok_t *tok) {
    uint16_t val = 0;
    uint8_t i = tok->start;
    while (i < tok->end) {
        uint8_t c = (uint8_t)js[i];
        if (c >= '0' && c <= '9') {
            val = (uint16_t)(val * 10 + (c - '0'));
        }
        i++;
    }
    return val;
}

/* Embedded JSON test string */
static const char json[] = "{\"name\":\"TMS9900\",\"bits\":16,\"year\":1979}";

volatile uint16_t out_token_count;
volatile uint16_t out_bits_value;

int main(void) {
    uint8_t len = my_strlen8(json);
    if (len == 0 || len >= INPUT_MAX)
        fail_loop();

    tok_t tokens[MAX_TOKENS];
    uint8_t token_count = json_tokenize(json, len, tokens, MAX_TOKENS);

    out_token_count = (uint16_t)token_count;

    if (token_count == 0)
        fail_loop();

    /* Extract "bits" value: look for "bits" key, take next token as value */
    uint16_t bits_val = 0;
    uint8_t i = 1;
    while (i + 1 < token_count) {
        if (tok_eq(json, &tokens[i], "bits", 4)) {
            bits_val = tok_int(json, &tokens[i + 1]);
            break;
        }
        i++;
    }

    out_bits_value = bits_val;

    if (token_count == EXPECTED_TOKEN_COUNT && bits_val == EXPECTED_BITS_VALUE)
        halt_ok();

    fail_loop();
    return 0;
}
