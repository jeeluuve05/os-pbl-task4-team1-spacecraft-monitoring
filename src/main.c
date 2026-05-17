/* ==========================================================
 * main.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Architect : Khurshid  |  Scheduler : Saida
 * Platform  : FreeRTOS Win32 Simulator (Visual Studio)
 * ========================================================== */

#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* ZeroGravity */
#include "task_scheduler.h"

/* ----------------------------------------------------------
 * vApplicationIdleHook  — called when CPU has nothing to do
 * ---------------------------------------------------------- */
void vApplicationIdleHook(void)
{
    /* Nothing needed here for simulation */
}

/* ----------------------------------------------------------
 * vApplicationMallocFailedHook — called if heap runs out
 * ---------------------------------------------------------- */
void vApplicationMallocFailedHook(void)
{
    printf("[FATAL] Heap allocation failed! System halting.\n");
    for (;;);
}

/* ----------------------------------------------------------
 * vApplicationStackOverflowHook — called on stack overflow
 * ---------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                    char *pcTaskName)
{
    printf("[FATAL] Stack overflow in task: %s\n", pcTaskName);
    for (;;);
}

/* ----------------------------------------------------------
 * main
 * ---------------------------------------------------------- */
int main(void)
{
    printf("========================================\n");
    printf("  ZeroGravity — Spacecraft Monitor\n");
    printf("  FreeRTOS Win32 Simulator\n");
    printf("  Team: Saida (Scheduler) starting...\n");
    printf("========================================\n\n");

    /* Initialize all tasks, queues, and event groups */
    vSchedulerInit();

    printf("[Main] Starting FreeRTOS scheduler...\n\n");

    /* Hand control to FreeRTOS — never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    printf("[Main] ERROR: Scheduler exited unexpectedly!\n");
    return 1;
}
