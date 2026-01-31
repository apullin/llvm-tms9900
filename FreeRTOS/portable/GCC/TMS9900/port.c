/*
 * FreeRTOS port for TMS9900
 *
 * Context switch design:
 *
 * Each task owns a permanent workspace (32 bytes = 16 registers in RAM).
 * The FreeRTOS-allocated "stack" is split: top 16 words = workspace,
 * remainder = data stack (pointed to by R10 in the workspace).
 *
 * When a task is suspended, a 4-word context frame is pushed onto its
 * data stack:
 *     [WP] [PC] [ST] [usCriticalNesting]
 *      ^--- pxTopOfStack points here
 *
 * Context restore pops these into R13/R14/R15 (for RTWP) and the
 * critical nesting variable.  R10 in the new workspace is updated
 * to the data stack pointer after the pop.
 *
 * This means context switch never copies the 16-word register file:
 * each task's registers stay in place.  Only 4 words move.
 */

#include "FreeRTOS.h"
#include "task.h"

/*-----------------------------------------------------------
 * Port constants.
 *-----------------------------------------------------------*/

/* ISR workspace address - must match startup.S vector table */
#define portISR_WP_ADDR         ( ( unsigned int ) 0x0040 )

/* Initial ST: interrupt mask = 2 (enables tick at level 1) */
#define portINITIAL_STATUS      ( ( StackType_t ) 0x0002 )

/* Critical nesting initial value - set high, scheduler start resets to 0 */
#define portINITIAL_CRITICAL_NESTING    ( ( unsigned int ) 10 )

/*-----------------------------------------------------------
 * External references.
 *-----------------------------------------------------------*/

typedef void TCB_t;
extern volatile TCB_t * volatile pxCurrentTCB;

/* Assembly functions in portasm.S */
extern void vPortStartFirstTask( void );
extern void vPortTickISR( void );

/*-----------------------------------------------------------
 * Critical section nesting counter.
 *
 * Starts non-zero so early code runs with interrupts disabled.
 * xPortStartScheduler sets it to 0 before starting the first task.
 *-----------------------------------------------------------*/
volatile unsigned int usCriticalNesting = portINITIAL_CRITICAL_NESTING;

/*-----------------------------------------------------------
 * pxPortInitialiseStack
 *
 * Stack layout after initialization (low addr → high addr):
 *
 *   pxTopOfStack → [WP]
 *                  [PC]          = pxCode (task entry point)
 *                  [ST]          = 0x0002 (interrupts enabled)
 *                  [critNesting] = 0
 *                  [R0]          ← workspace starts here
 *                  [R1]          = pvParameters
 *                  [R2] ... [R9] = 0
 *                  [R10]         = (don't care, set by restore)
 *                  [R11] ... [R15] = 0
 *
 * The context restore code pops 4 words, then writes the resulting
 * pointer into workspace[10] (R10 = data stack pointer).
 *-----------------------------------------------------------*/
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    StackType_t * pxWorkspace;
    StackType_t * pxSP;
    int i;

    /* Workspace occupies the top 16 words of the allocation.
     * pxTopOfStack points to the highest usable address. */
    pxWorkspace = pxTopOfStack - 15;

    /* Clear workspace (all registers = 0) */
    for( i = 0; i < 16; i++ )
    {
        pxWorkspace[ i ] = 0;
    }

    /* R1 = first argument to task function */
    pxWorkspace[ 1 ] = ( StackType_t )( unsigned int ) pvParameters;

    /* Data stack starts just below workspace, grows downward */
    pxSP = pxWorkspace;

    /* Push initial context frame (order must match portasm.S restore) */
    *( --pxSP ) = ( StackType_t ) 0;                         /* usCriticalNesting */
    *( --pxSP ) = portINITIAL_STATUS;                         /* ST */
    *( --pxSP ) = ( StackType_t )( unsigned int ) pxCode;     /* PC */
    *( --pxSP ) = ( StackType_t )( unsigned int ) pxWorkspace; /* WP */

    return pxSP;
}

/*-----------------------------------------------------------
 * xPortStartScheduler
 *-----------------------------------------------------------*/
BaseType_t xPortStartScheduler( void )
{
    /* Reset critical nesting for the first task */
    usCriticalNesting = 0;

    /* Write the tick ISR vector (level 1):
     *   0x0004 = ISR workspace pointer
     *   0x0006 = ISR entry point (PC)
     */
    *( volatile unsigned int * ) 0x0004 = portISR_WP_ADDR;
    *( volatile unsigned int * ) 0x0006 = ( unsigned int ) vPortTickISR;

    /* Initialize ISR workspace R10 (stack pointer).
     * ISR stack: 0x0060-0x00FF, R10 starts at 0x0100. */
    *( volatile unsigned int * )( portISR_WP_ADDR + 20 ) = 0x0100;

    /* Start the first task - never returns */
    vPortStartFirstTask();

    /* Should not reach here */
    return pdTRUE;
}

/*-----------------------------------------------------------
 * vPortEndScheduler
 *-----------------------------------------------------------*/
void vPortEndScheduler( void )
{
    /* Not implemented - scheduler runs forever */
}
