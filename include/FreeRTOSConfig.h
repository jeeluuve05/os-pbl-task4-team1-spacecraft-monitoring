/* ==========================================================
 * FreeRTOSConfig.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 *
 * Supports two build targets:
 *   macOS / Linux : FreeRTOS POSIX port, GCC (via Makefile)
 *   Windows       : FreeRTOS Win32 port, MSVC / VS2022
 *
 * The Win32 port fires the tick via a Windows multimedia timer.
 * The POSIX port uses SIGALRM. Both expose the same FreeRTOS API.
 * ========================================================== */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Scheduler ---- */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configMAX_PRIORITIES                    15
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0

/* ---- Platform-specific timing & stack ----
 * Win32 multimedia timer runs reliably at 100 Hz.
 * POSIX setitimer supports 1000 Hz on macOS/Linux.
 * portTICK_PERIOD_MS = 1000 / configTICK_RATE_HZ, so all
 * pdMS_TO_TICKS() and tick×portTICK_PERIOD_MS calculations
 * remain correct on both ports without any source changes. */
#ifdef _WIN32
#  define configTICK_RATE_HZ            ( ( TickType_t ) 100  )
#  define configMINIMAL_STACK_SIZE      ( ( unsigned short ) 64 )
#  define configCPU_CLOCK_HZ            ( ( unsigned long ) 20000000UL )
#else
#  define configTICK_RATE_HZ            ( ( TickType_t ) 1000 )
#  define configMINIMAL_STACK_SIZE      ( ( unsigned short ) 512 )
#  define configCPU_CLOCK_HZ            ( ( unsigned long ) 60000000UL )
#endif

/* ---- Memory ---- */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( ( size_t )( 262144 ) )  /* 256 KB */
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ---- Hooks ---- */
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2

/* Win32 port startup hook (required by some Win32 demo templates) */
#ifdef _WIN32
#  define configUSE_DAEMON_TASK_STARTUP_HOOK    0
#endif

/* ---- Debug / Stats ---- */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configGENERATE_RUN_TIME_STATS           0

/* ---- Co-routines ---- */
#define configUSE_CO_ROUTINES                   0

/* ---- Software Timers ---- */
#define configUSE_TIMERS                        0

/* ---- Assert ----
 * On MSVC, assert() from <assert.h> works but pops a dialog in Debug mode.
 * The taskDISABLE_INTERRUPTS form is cleaner for both platforms. */
#ifdef _WIN32
#  define configASSERT( x )  if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;); }
#else
#  include <assert.h>
#  define configASSERT( x )  assert( x )
#endif

/* ---- Optional API ---- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTaskResumeFromISR              1

#endif /* FREERTOS_CONFIG_H */
