/* C++ runtime stubs for freestanding TMS9900.
 * Provides operator new/delete, __cxa_* stubs, and __dso_handle.
 */

typedef __SIZE_TYPE__ size_t;

extern "C" void *malloc(size_t);
extern "C" void free(void *);

void *operator new(size_t size) { return malloc(size); }
void *operator new(size_t, void *p) noexcept { return p; }  /* placement */
void operator delete(void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }

void *operator new[](size_t size) { return malloc(size); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }

/* Aligned new/delete (C++17) — needed by STL containers at -O0 */
namespace std { enum class align_val_t : size_t {}; }
void *operator new(size_t size, std::align_val_t) { return malloc(size); }
void *operator new[](size_t size, std::align_val_t) { return malloc(size); }
void operator delete(void *p, std::align_val_t) noexcept { free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { free(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { free(p); }

extern "C" void abort() { for (;;) {} }
extern "C" void __cxa_pure_virtual() { for (;;) {} }
extern "C" int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
extern "C" void *__dso_handle = 0;
