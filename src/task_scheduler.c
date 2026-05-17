/* ==========================================================
 * task_scheduler.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Designer : Saida (Task & Scheduler Designer)
 * ========================================================== */

#include "task_scheduler.h"
#include "monitors.h"      /* CPUMonitor, MemoryMonitor, SensorMonitor */
#include "responders.h"    /* all dynamic responder task functions      */
#include <stdio.h>

/* ----------------------------------------------------------
 * HANDLE DEFINITIONS
 * ---------------------------------------------------------- */

/* Always-on monitors */
TaskHandle_t xCPUMonitorHandle      = NULL;
TaskHandle_t xMemoryMonitorHandle   = NULL;
TaskHandle_t xSensorMonitorHandle   = NULL;

/* Dynamic responders — start as NULL (not running) */
TaskHandle_t xCPUResponderHandle        = NULL;
TaskHandle_t xLowMemoryResponderHandle  = NULL;
TaskHandle_t xThermalResponderHandle    = NULL;
TaskHandle_t xBatteryResponderHandle    = NULL;
TaskHandle_t xCommTimeoutResponderHandle = NULL;

/* Queues */
QueueHandle_t xCPUDataQueue     = NULL;
QueueHandle_t xMemoryDataQueue  = NULL;
QueueHandle_t xSensorDataQueue  = NULL;

/* Event group */
EventGroupHandle_t xSystemEventGroup = NULL;

/* ----------------------------------------------------------
 * vSchedulerInit
 * Call this once in main() before vTaskStartScheduler()
 * ---------------------------------------------------------- */
void vSchedulerInit(void)
{
    /* --- Create shared RTOS objects --- */
    xCPUDataQueue    = xQueueCreate(5, sizeof(CPUData_t));
    xMemoryDataQueue = xQueueCreate(5, sizeof(MemoryData_t));
    xSensorDataQueue = xQueueCreate(5, sizeof(SensorData_t));
    xSystemEventGroup = xEventGroupCreate();

    /* Safety check */
    configASSERT(xCPUDataQueue    != NULL);
    configASSERT(xMemoryDataQueue != NULL);
    configASSERT(xSensorDataQueue != NULL);
    configASSERT(xSystemEventGroup != NULL);

    /* --- Create always-on Layer 1 monitor tasks --- */
    xTaskCreate(vCPUMonitorTask,
                "CPUMonitor",
                STACK_MONITOR,
                NULL,
                PRIORITY_CORE_MONITOR,
                &xCPUMonitorHandle);

    xTaskCreate(vMemoryMonitorTask,
                "MemMonitor",
                STACK_MONITOR,
                NULL,
                PRIORITY_CORE_MONITOR,
                &xMemoryMonitorHandle);

    xTaskCreate(vSensorMonitorTask,
                "SensorMonitor",
                STACK_MONITOR,
                NULL,
                PRIORITY_CORE_MONITOR,
                &xSensorMonitorHandle);

    /* NOTE: Watchdog, Logger, RecoveryManager tasks are created by Nur.
     * This file only owns Layer 1 monitors and the spawn/kill helpers. */

    printf("[Scheduler] All core monitor tasks created.\n");
}

/* ----------------------------------------------------------
 * vSpawnResponder
 * Safely creates a dynamic responder task if not running.
 * ---------------------------------------------------------- */
void vSpawnResponder(TaskFunction_t pvTask,
                     const char * const pcName,
                     TaskHandle_t *pxHandle)
{
    if (*pxHandle == NULL)   /* not already running */
    {
        BaseType_t xResult = xTaskCreate(pvTask,
                                         pcName,
                                         STACK_RESPONDER,
                                         NULL,
                                         PRIORITY_RESPONDER,
                                         pxHandle);
        if (xResult == pdPASS)
        {
            printf("[Scheduler] SPAWNED responder: %s\n", pcName);
        }
        else
        {
            printf("[Scheduler] ERROR: Could not spawn %s (heap too low?)\n", pcName);
        }
    }
}

/* ----------------------------------------------------------
 * vKillResponder
 * Deletes a responder task and clears the handle.
 * Call from the MONITOR task after signal returns to normal.
 * ---------------------------------------------------------- */
void vKillResponder(TaskHandle_t *pxHandle)
{
    if (*pxHandle != NULL)
    {
        printf("[Scheduler] KILLING responder (handle=%p)\n", (void*)*pxHandle);
        vTaskDelete(*pxHandle);
        *pxHandle = NULL;
    }
}
