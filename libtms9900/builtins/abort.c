/* Minimal freestanding abort() fallback for __builtin_trap() and checked
 * arithmetic builtins when no hosted/C++ runtime provides abort().
 */
__attribute__((weak, noreturn))
void abort(void) {
    for (;;) {
        __asm__ volatile("" ::: "memory");
    }
}
