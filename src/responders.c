/* ==========================================================
 * responders.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Designer : Saida (Task & Scheduler Designer)
 *
 * Dynamic responder tasks — created at runtime when a critical
 * threshold is breached, deleted when signal returns to normal.
 *
 * Each responder:
 *   1. Takes corrective/logging action
 *   2. Polls its condition
 *   3. Calls vTaskDelete(NULL) to self-delete when recovered
 * ========================================================== */

#include "responders.h"
#include "task_scheduler.h"
#include "simulation.h"
#include <stdio.h>

/* ----------------------------------------------------------
 * vCPUOverloadResponderTask
 * Triggered when CPU load > 80%
 * ---------------------------------------------------------- */
void vCPUOverloadResponderTask(void *pvParameters)
{
    printf("[CPUResponder] STARTED — shedding non-critical load\n");

    for (;;)
    {
        uint32_t load = sim_get_cpu_load();

        if (load <= THRESH_CPU_CRITICAL_PCT)
        {
            printf("[CPUResponder] CPU recovered (%lu%%) — terminating\n", load);
            /* Clear handle before self-delete */
            xCPUResponderHandle = NULL;
            vTaskDelete(NULL);
        }

        printf("[CPUResponder] CPU still high: %lu%%\n", load);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ----------------------------------------------------------
 * vLowMemoryResponderTask
 * Triggered when heap free < 20%
 * ---------------------------------------------------------- */
void vLowMemoryResponderTask(void *pvParameters)
{
    printf("[MemResponder] STARTED — attempting memory recovery\n");

    for (;;)
    {
        size_t freeHeap  = xPortGetFreeHeapSize();
        size_t totalHeap = configTOTAL_HEAP_SIZE;
        uint32_t pct     = (freeHeap * 100) / totalHeap;

        if (pct >= THRESH_HEAP_CRITICAL_PCT)
        {
            printf("[MemResponder] Heap recovered (%lu%%) — terminating\n",
                   (unsigned long)pct);
            xLowMemoryResponderHandle = NULL;
            vTaskDelete(NULL);
        }

        printf("[MemResponder] Heap still low: %lu%% (%u bytes free)\n",
               (unsigned long)pct, (unsigned)freeHeap);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ----------------------------------------------------------
 * vThermalResponderTask
 * Triggered when temperature > 80°C
 * ---------------------------------------------------------- */
void vThermalResponderTask(void *pvParameters)
{
    printf("[ThermalResponder] STARTED — activating thermal protection\n");

    for (;;)
    {
        int32_t temp = sim_get_temperature();

        if (temp <= THRESH_TEMP_CRITICAL_C)
        {
            printf("[ThermalResponder] Temperature normal (%ld°C) — terminating\n",
                   temp);
            xThermalResponderHandle = NULL;
            vTaskDelete(NULL);
        }

        printf("[ThermalResponder] Temp still high: %ld°C\n", temp);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ----------------------------------------------------------
 * vBatteryResponderTask
 * Triggered when battery voltage < 3.3V (330 in mV units)
 * ---------------------------------------------------------- */
void vBatteryResponderTask(void *pvParameters)
{
    printf("[BatteryResponder] STARTED — entering low-power mode\n");

    for (;;)
    {
        uint32_t mv = sim_get_battery_mv();

        if (mv >= THRESH_BATTERY_CRITICAL_V)
        {
            printf("[BatteryResponder] Battery recovered (%lumV) — terminating\n",
                   mv);
            xBatteryResponderHandle = NULL;
            vTaskDelete(NULL);
        }

        printf("[BatteryResponder] Battery still low: %lumV\n", mv);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ----------------------------------------------------------
 * vCommTimeoutResponderTask
 * Triggered when comm gap > 3000ms
 * ---------------------------------------------------------- */
void vCommTimeoutResponderTask(void *pvParameters)
{
    printf("[CommResponder] STARTED — attempting comm re-link\n");

    for (;;)
    {
        uint32_t gap = sim_get_comm_gap_ms();

        if (gap <= THRESH_COMM_TIMEOUT_MS)
        {
            printf("[CommResponder] Comm restored (gap=%lums) — terminating\n",
                   gap);
            xCommTimeoutResponderHandle = NULL;
            vTaskDelete(NULL);
        }

        printf("[CommResponder] Still no reply — gap: %lums\n", gap);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
