/*
 * FreeRTOS port for TMS9900
 *
 * The TMS9900's workspace pointer architecture makes it uniquely suited
 * for RTOS: context switch = change one 16-bit register (WP).
 * Each task's registers live in its own workspace RAM.
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
    extern "C" {
#endif

/*-----------------------------------------------------------
 * Port specific definitions.
 *-----------------------------------------------------------*/

/* Type definitions. */
#define portCHAR          char
#define portFLOAT         float
#define portDOUBLE        double
#define portLONG          long
#define portSHORT         int
#define portSTACK_TYPE    unsigned int
#define portBASE_TYPE     int
#define portPOINTER_SIZE_TYPE unsigned int

typedef portSTACK_TYPE   StackType_t;
typedef int              BaseType_t;
typedef unsigned int     UBaseType_t;

#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
    typedef unsigned int     TickType_t;
    #define portMAX_DELAY    ( TickType_t ) 0xffff
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    typedef unsigned long    TickType_t;
    #define portMAX_DELAY    ( TickType_t ) ( 0xFFFFFFFFUL )
#else
    #error configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width.
#endif

/*-----------------------------------------------------------
 * Interrupt control macros.
 *
 * TMS9900 interrupt mask is in ST bits 0-3 (TI numbering: bits 12-15).
 * LIMI 0 disables all maskable interrupts.
 * LIMI 2 enables interrupts at levels 0-2 (tick is level 1).
 *-----------------------------------------------------------*/
#define portDISABLE_INTERRUPTS()    __asm__ volatile ( "LIMI 0" )
#define portENABLE_INTERRUPTS()     __asm__ volatile ( "LIMI 2" )

/*-----------------------------------------------------------
 * Critical section control macros.
 *
 * Uses nesting counter since TMS9900 has no push/pop SR idiom.
 *-----------------------------------------------------------*/
#define portNO_CRITICAL_SECTION_NESTING    ( ( unsigned int ) 0 )

#define portENTER_CRITICAL()                                                  \
    {                                                                         \
        extern volatile unsigned int usCriticalNesting;                       \
        portDISABLE_INTERRUPTS();                                             \
        usCriticalNesting++;                                                  \
    }

#define portEXIT_CRITICAL()                                                   \
    {                                                                         \
        extern volatile unsigned int usCriticalNesting;                       \
        if( usCriticalNesting > portNO_CRITICAL_SECTION_NESTING )             \
        {                                                                     \
            usCriticalNesting--;                                              \
            if( usCriticalNesting == portNO_CRITICAL_SECTION_NESTING )        \
            {                                                                 \
                portENABLE_INTERRUPTS();                                      \
            }                                                                 \
        }                                                                     \
    }

/*-----------------------------------------------------------
 * Task utilities.
 *-----------------------------------------------------------*/
extern void vPortYield( void ) __attribute__( ( naked ) );
#define portYIELD()    vPortYield()
#define portNOP()      __asm__ volatile ( "NOP" )

/*-----------------------------------------------------------
 * Hardware specifics.
 *-----------------------------------------------------------*/
#define portBYTE_ALIGNMENT    2
#define portSTACK_GROWTH      ( -1 )
#define portTICK_PERIOD_MS    ( ( TickType_t ) 1000 / configTICK_RATE_HZ )

/*-----------------------------------------------------------
 * Task function macros.
 *-----------------------------------------------------------*/
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )

#ifdef __cplusplus
    }
#endif

#endif /* PORTMACRO_H */
