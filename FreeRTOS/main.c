/*
 * FreeRTOS TMS9900 test - two tasks writing to memory markers.
 *
 * Run with tms9900-trace:
 *   tms9900-trace --timer=1:50000 -n 500000 -w 0x8300 -l 0x0000 \
 *       -d 0x7F00:8 build/freertos_test.bin
 *
 * Success: both MARKER_A (0x7F00) and MARKER_B (0x7F02) are non-zero
 * in the memory dump, proving both tasks ran under preemptive scheduling.
 */

#include "FreeRTOS.h"
#include "task.h"

/* Memory-mapped markers visible in tms9900-trace dump */
#define MARKER_A    ( *( volatile unsigned int * ) 0x7F00 )
#define MARKER_B    ( *( volatile unsigned int * ) 0x7F02 )
#define MARKER_IDLE ( *( volatile unsigned int * ) 0x7F04 )

/* Task stack + TCB storage (static allocation) */
static StaticTask_t xTaskA_TCB;
static StackType_t  xTaskA_Stack[ 128 ];

static StaticTask_t xTaskB_TCB;
static StackType_t  xTaskB_Stack[ 128 ];

/* Idle task storage (required by configSUPPORT_STATIC_ALLOCATION) */
static StaticTask_t xIdleTaskTCB;
static StackType_t  xIdleTaskStack[ 64 ];

/*-----------------------------------------------------------
 * Task A - increment marker at 0x7F00
 *-----------------------------------------------------------*/
void vTaskA( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        MARKER_A++;

        /* Busy-wait loop to burn some cycles */
        for( volatile int i = 0; i < 50; i++ )
        {
        }
    }
}

/*-----------------------------------------------------------
 * Task B - increment marker at 0x7F02
 *-----------------------------------------------------------*/
void vTaskB( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        MARKER_B++;

        for( volatile int i = 0; i < 50; i++ )
        {
        }
    }
}

/*-----------------------------------------------------------
 * Required by FreeRTOS static allocation
 *-----------------------------------------------------------*/
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE * pulIdleTaskStackSize )
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleTaskStack;
    *pulIdleTaskStackSize = 64;
}

/*-----------------------------------------------------------
 * main
 *-----------------------------------------------------------*/
int main( void )
{
    /* Clear markers */
    MARKER_A = 0;
    MARKER_B = 0;
    MARKER_IDLE = 0;

    /* Create tasks at equal priority (time-sliced) */
    xTaskCreateStatic( vTaskA, "TaskA", 128, ( void * ) 0, 1,
                       xTaskA_Stack, &xTaskA_TCB );

    xTaskCreateStatic( vTaskB, "TaskB", 128, ( void * ) 0, 1,
                       xTaskB_Stack, &xTaskB_TCB );

    /* Start scheduler - never returns */
    vTaskStartScheduler();

    /* Should never get here */
    for( ;; )
    {
    }
}
