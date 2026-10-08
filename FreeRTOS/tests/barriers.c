#include "FreeRTOS.h"

/* Ordinary memory must stay on its own side of either interrupt-mask change.
   Inspect IR clobbers as well as compiling the asm for the actual target. */
unsigned shared;
unsigned protected_access(void)
{
    portDISABLE_INTERRUPTS();
    unsigned value = shared;
    shared = value + 1;
    portENABLE_INTERRUPTS();
    return value;
}
