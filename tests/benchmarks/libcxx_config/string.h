// Minimal string.h for TMS9900 freestanding libc++
#ifndef _TMS9900_STRING_H
#define _TMS9900_STRING_H
#define _LIBCPP_STRING_H  // satisfy libc++ cstring check

typedef __SIZE_TYPE__ size_t;

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *__dst, const void *__src, size_t __n);
void *memmove(void *__dst, const void *__src, size_t __n);
void *memset(void *__s, int __c, size_t __n);
int memcmp(const void *__s1, const void *__s2, size_t __n);
size_t strlen(const char *__s);
char *strcpy(char *__dst, const char *__src);
char *strncpy(char *__dst, const char *__src, size_t __n);
int strcmp(const char *__s1, const char *__s2);
int strncmp(const char *__s1, const char *__s2, size_t __n);
char *strcat(char *__dst, const char *__src);
char *strncat(char *__dst, const char *__src, size_t __n);
char *strchr(const char *__s, int __c);
char *strrchr(const char *__s, int __c);
char *strstr(const char *__haystack, const char *__needle);
void *memchr(const void *__s, int __c, size_t __n);

#ifdef __cplusplus
}
#endif

#endif // _TMS9900_STRING_H
