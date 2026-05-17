/* ==========================================================
 * monitors.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Designer : Saida (Task & Scheduler Designer)
 *
 * Contains the 3 always-on Layer 1 monitor tasks:
 *   - vCPUMonitorTask
 *   - vMemoryMonitorTask
 *   - vSensorMonitorTask
 *
 * Each monitor:
 *   1. Reads simulated sensor values
 *   2. Sends data to its queue for logging
 *   3. Checks against thresholds
 *   4. Spawns / kills the matching responder as needed
 *   5. Sets event group bits for the safety layer
 * ========================================================== */

#include "monitors.h"
#include "task_scheduler.h"
#include "responders.h"
#include "simulation.h"   /* sim_get_cpu_load(), sim_get_heap(), etc. */
#include <stdio.h>

/* ----------------------------------------------------------
 * vCPUMonitorTask  (period: 1000ms)
 * ---------------------------------------------------------- */
void vCPUMonitorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_CPU_MONITOR_MS);

    CPUData_t data;

    for (;;)
    {
        /* 1. Read simulated CPU load */
        data.cpu_load_pct   = sim_get_cpu_load();
        data.timestamp_ms   = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* 2. Send to queue (non-blocking — drop if full) */
        xQueueSend(xCPUDataQueue, &data, 0);

        /* 3. Threshold check */
        if (data.cpu_load_pct > THRESH_CPU_CRITICAL_PCT)
        {
            printf("[CPUMonitor] CRITICAL: CPU load %u%%\n", (unsigned)data.cpu_load_pct);
            xEventGroupSetBits(xSystemEventGroup, EVT_CPU_CRITICAL);
            vSpawnResponder(vCPUOverloadResponderTask,
                            "CPUResponder",
                            &xCPUResponderHandle);
        }
        else
        {
            xEventGroupClearBits(xSystemEventGroup, EVT_CPU_CRITICAL);
            vKillResponder(&xCPUResponderHandle);
        }

        /* 4. Stack self-check */
        UBaseType_t uxHW = uxTaskGetStackHighWaterMark(NULL);
        if (uxHW < (STACK_MONITOR * THRESH_STACK_WARN_PCT / 100))
        {
            printf("[CPUMonitor] WARN: Stack high-water = %u words\n", uxHW);
        }

        /* 5. Wait until next period */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ----------------------------------------------------------
 * vMemoryMonitorTask  (period: 1000ms)
 * ---------------------------------------------------------- */
void vMemoryMonitorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_MEMORY_MONITOR_MS);

    MemoryData_t data;

    for (;;)
    {
        /* 1. Read heap stats */
        data.heap_free_bytes  = xPortGetFreeHeapSize();
        data.heap_total_bytes = configTOTAL_HEAP_SIZE;
        data.heap_free_pct    = (data.heap_free_bytes * 100) / data.heap_total_bytes;
        data.stack_high_water = uxTaskGetStackHighWaterMark(NULL);
        data.timestamp_ms     = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* 2. Send to queue */
        xQueueSend(xMemoryDataQueue, &data, 0);

        /* 3. Threshold check */
        if (data.heap_free_pct < THRESH_HEAP_CRITICAL_PCT)
        {
            printf("[MemMonitor] CRITICAL: Heap free %u%% (%u bytes)\n",
                   (unsigned)data.heap_free_pct, (unsigned)data.heap_free_bytes);
            xEventGroupSetBits(xSystemEventGroup, EVT_MEMORY_CRITICAL);
            vSpawnResponder(vLowMemoryResponderTask,
                            "MemResponder",
                            &xLowMemoryResponderHandle);
        }
        else
        {
            xEventGroupClearBits(xSystemEventGroup, EVT_MEMORY_CRITICAL);
            vKillResponder(&xLowMemoryResponderHandle);
        }

        /* 4. Stack self-check */
        UBaseType_t uxHW = uxTaskGetStackHighWaterMark(NULL);
        if (uxHW < (STACK_MONITOR * THRESH_STACK_WARN_PCT / 100))
        {
            printf("[MemMonitor] WARN: Stack high-water = %u words\n", uxHW);
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ----------------------------------------------------------
 * vSensorMonitorTask  (period: 2000ms)
 * Handles: temperature, battery voltage, comm heartbeat
 * ---------------------------------------------------------- */
void vSensorMonitorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_SENSOR_MONITOR_MS);

    SensorData_t data;

    for (;;)
    {
        /* 1. Read simulated sensor values */
        data.temperature_c      = sim_get_temperature();
        data.battery_mv         = sim_get_battery_mv();
        data.comm_last_reply_ms = sim_get_comm_gap_ms();
        data.timestamp_ms       = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* 2. Send to queue */
        xQueueSend(xSensorDataQueue, &data, 0);

        /* --- Temperature check --- */
        if (data.temperature_c > THRESH_TEMP_CRITICAL_C)
        {
            printf("[SensorMonitor] CRITICAL: Temp %d C\n", (int)data.temperature_c);
            xEventGroupSetBits(xSystemEventGroup, EVT_THERMAL_CRITICAL);
            vSpawnResponder(vThermalResponderTask,
                            "ThermalResponder",
                            &xThermalResponderHandle);
        }
        else
        {
            xEventGroupClearBits(xSystemEventGroup, EVT_THERMAL_CRITICAL);
            vKillResponder(&xThermalResponderHandle);
        }

        /* --- Battery check --- */
        if (data.battery_mv < THRESH_BATTERY_CRITICAL_V)
        {
            printf("[SensorMonitor] CRITICAL: Battery %umV\n", (unsigned)data.battery_mv);
            xEventGroupSetBits(xSystemEventGroup, EVT_BATTERY_CRITICAL);
            vSpawnResponder(vBatteryResponderTask,
                            "BatteryResponder",
                            &xBatteryResponderHandle);
        }
        else
        {
            xEventGroupClearBits(xSystemEventGroup, EVT_BATTERY_CRITICAL);
            vKillResponder(&xBatteryResponderHandle);
        }

        /* --- Comm timeout check --- */
        if (data.comm_last_reply_ms > THRESH_COMM_TIMEOUT_MS)
        {
            printf("[SensorMonitor] CRITICAL: No comm for %ums\n",
                   (unsigned)data.comm_last_reply_ms);
            xEventGroupSetBits(xSystemEventGroup, EVT_COMM_TIMEOUT);
            vSpawnResponder(vCommTimeoutResponderTask,
                            "CommResponder",
                            &xCommTimeoutResponderHandle);
        }
        else
        {
            xEventGroupClearBits(xSystemEventGroup, EVT_COMM_TIMEOUT);
            vKillResponder(&xCommTimeoutResponderHandle);
        }

        /* Stack self-check */
        UBaseType_t uxHW = uxTaskGetStackHighWaterMark(NULL);
        if (uxHW < (STACK_MONITOR * THRESH_STACK_WARN_PCT / 100))
        {
            printf("[SensorMonitor] WARN: Stack high-water = %u words\n", uxHW);
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
