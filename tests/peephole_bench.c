#include <stdint.h>

volatile uint16_t sink16;
volatile uint8_t sink8;

__attribute__((noinline))
uint16_t sum_words_explicit(uint16_t *p, int n) {
  uint16_t acc = 0;
  for (int i = 0; i < n; ++i) {
    uint16_t v = *p;
    p = p + 1;
    acc += v;
  }
  sink16 = acc;
  return acc;
}

__attribute__((noinline))
uint16_t sum_words_post(uint16_t *p, int n) {
  uint16_t acc = 0;
  for (int i = 0; i < n; ++i) {
    acc += *p++;
  }
  sink16 = acc;
  return acc;
}

__attribute__((noinline))
void copy_words_explicit(uint16_t *dst, const uint16_t *src, int n) {
  for (int i = 0; i < n; ++i) {
    uint16_t v = *src;
    src = src + 1;
    *dst = v;
    dst = dst + 1;
  }
}

__attribute__((noinline))
void copy_words_post(uint16_t *dst, const uint16_t *src, int n) {
  for (int i = 0; i < n; ++i) {
    *dst++ = *src++;
  }
}

__attribute__((noinline))
uint8_t sum_bytes_explicit(uint8_t *p, int n) {
  uint8_t acc = 0;
  for (int i = 0; i < n; ++i) {
    uint8_t v = *p;
    p = p + 1;
    acc = (uint8_t)(acc + v);
  }
  sink8 = acc;
  return acc;
}

__attribute__((noinline))
uint8_t sum_bytes_post(uint8_t *p, int n) {
  uint8_t acc = 0;
  for (int i = 0; i < n; ++i) {
    acc = (uint8_t)(acc + *p++);
  }
  sink8 = acc;
  return acc;
}

__attribute__((noinline))
void bump_accum(volatile uint16_t *p, int n) {
  uint16_t v = *p;
  for (int i = 0; i < n; ++i) {
    v = (uint16_t)(v + 1);
    v = (uint16_t)(v + 2);
    v = (uint16_t)(v - 1);
    v = (uint16_t)(v - 2);
  }
  *p = v;
}
