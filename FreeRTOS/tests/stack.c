/* Check the workspace/context frame without starting the scheduler. */
#include "FreeRTOS.h"
#include "task.h"

static StackType_t storage[100] __attribute__((aligned(4)));

static void task(void *parameter)
{
    (void)parameter;
}

int main(void)
{
    /* Both possible word-aligned stack tops, with padding/canaries around the
       frame. FreeRTOS itself rounds the highest usable word down to four bytes. */
    for (unsigned last = 32; last < 96; ++last) {
        for (unsigned i = 0; i < 100; ++i)
            storage[i] = 0xa55a;
        StackType_t *frame = pxPortInitialiseStack(&storage[last], task,
                                                   (void *)0x1234);
        StackType_t *workspace = (StackType_t *)(uintptr_t)frame[0];
        if ((uintptr_t)workspace % 4 || (uintptr_t)frame % 4)
            return 1;
        if (frame + 4 != workspace || frame[1] != (uintptr_t)task ||
            frame[2] != 2 || frame[3] != 0)
            return 2;
        if (workspace[0] != 0x1234)
            return 3;
        for (unsigned i = 1; i < 16; ++i)
            if (workspace[i] != 0)
                return 4;
        unsigned begin = frame - storage;
        unsigned end = workspace - storage + 16;
        if (end > last + 1 || begin < last - 20)
            return 5;
        for (unsigned i = 0; i < begin; ++i)
            if (storage[i] != 0xa55a)
                return 6;
        for (unsigned i = end; i < 100; ++i)
            if (storage[i] != 0xa55a)
                return 7;
    }
    return 0;
}
