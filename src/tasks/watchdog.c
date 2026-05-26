/* ==========================================================
 * watchdog.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Nur (Logging & Watchdog Developer)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * Layer 3 — System Safety (highest priority)
 *
 * vWatchdogTask (period 500 ms, PRIORITY_WATCHDOG=10):
 *   1. Checks that all mandatory Layer-1 monitor handles are non-NULL
 *      and the task is in a valid state (not deleted).
 *   2. Checks own-tasks (logger, simulator, ground-station) via
 *      g_wd_heartbeat_ms[]. If gap > WATCHDOG_TIMEOUT_MS → recover.
 *   3. Checks dynamic responders: if a responder handle is non-NULL
 *      and it has been alive longer than WATCHDOG_RESPONDER_TTL_MS
 *      without the condition resolving → log warning (escalate).
 *
 * vRecoveryManagerTask (event-driven, PRIORITY_RECOVERY=8):
 *   Waits on EVT_TASK_FROZEN. When set, logs the escalated failure
 *   and attempts to restart the affected mandatory task.
 * ========================================================== */

#include "tasks/watchdog.h"
#include "task_scheduler.h"
#include "monitors.h"
#include "logger.h"
#include "config.h"
#include "sensor_simulator.h"

#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------
 * Heartbeat array — written by watched tasks, read by watchdog
 * ---------------------------------------------------------- */
volatile uint32_t g_wd_heartbeat_ms[WD_TASK_COUNT] = {0};

/* ----------------------------------------------------------
 * Spawn timestamps for dynamic responders
 * ---------------------------------------------------------- */
static uint32_t s_thermal_spawn_ms   = 0;
static uint32_t s_mem_spawn_ms       = 0;
static uint32_t s_battery_spawn_ms   = 0;
static uint32_t s_comm_spawn_ms      = 0;

/* ----------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------- */
static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* Returns 1 if the task handle is non-NULL and not deleted */
static int task_alive(TaskHandle_t h)
{
    if (h == NULL) return 0;
    eTaskState s = eTaskGetState(h);
    return (s != eDeleted && s != eInvalid);
}

/* Log + enqueue the watchdog alert */
static void wd_alert(const char *task_name, uint32_t elapsed_s)
{
    log_event("[T=%03us] !!! WATCHDOG: task '%s' unresponsive — recovering !!!",
              (unsigned)elapsed_s, task_name);
}

/* ----------------------------------------------------------
 * Track responder spawn times (call when handle goes non-NULL)
 * ---------------------------------------------------------- */
static void track_spawn(TaskHandle_t *pHandle, uint32_t *pSpawnMs)
{
    if (*pHandle != NULL && *pSpawnMs == 0)
        *pSpawnMs = now_ms();
    if (*pHandle == NULL)
        *pSpawnMs = 0;
}

/* ----------------------------------------------------------
 * vWatchdogTask  — PRIORITY_WATCHDOG (10), period 500 ms
 * ---------------------------------------------------------- */
void vWatchdogTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(500);

    printf("[Watchdog] Task started (priority %d, period 500ms)\n",
           PRIORITY_WATCHDOG);

    for (;;)
    {
        uint32_t t_ms       = now_ms();
        uint32_t elapsed_s  = g_sim.elapsed_s;

        /* -------- Layer-1 mandatory monitors -------- */
        const char *mon_names[] = { "CPUMonitor", "MemMonitor", "SensorMonitor" };
        TaskHandle_t *mon_handles[] = {
            &xCPUMonitorHandle,
            &xMemoryMonitorHandle,
            &xSensorMonitorHandle
        };
        TaskFunction_t mon_funcs[] = {
            vCPUMonitorTask,
            vMemoryMonitorTask,
            vSensorMonitorTask
        };
        UBaseType_t mon_prio[] = {
            PRIORITY_CORE_MONITOR,
            PRIORITY_CORE_MONITOR,
            PRIORITY_CORE_MONITOR
        };

        for (int i = 0; i < 3; i++) {
            if (!task_alive(*mon_handles[i])) {
                wd_alert(mon_names[i], elapsed_s);
                xEventGroupSetBits(xSystemEventGroup, EVT_TASK_FROZEN);
                /* Attempt immediate recreation */
                xTaskCreate(mon_funcs[i],
                            mon_names[i],
                            STACK_MONITOR,
                            NULL,
                            mon_prio[i],
                            mon_handles[i]);
            }
        }

        /* -------- Own-task heartbeat check -------- */
        for (int i = 0; i < WD_TASK_COUNT; i++) {
            uint32_t gap = (g_wd_heartbeat_ms[i] > 0) ?
                           (t_ms - g_wd_heartbeat_ms[i]) : 0;
            if (gap > WATCHDOG_TIMEOUT_MS && g_wd_heartbeat_ms[i] > 0) {
                const char *names[] = { "logger_task", "sim_task", "ground_station" };
                wd_alert(names[i], elapsed_s);
                xEventGroupSetBits(xSystemEventGroup, EVT_TASK_FROZEN);
                g_wd_heartbeat_ms[i] = t_ms;   /* reset to avoid repeat spam */
            }
        }

        /* -------- Dynamic responder staleness check -------- */
        track_spawn(&xThermalResponderHandle,    &s_thermal_spawn_ms);
        track_spawn(&xLowMemoryResponderHandle,  &s_mem_spawn_ms);
        track_spawn(&xBatteryResponderHandle,    &s_battery_spawn_ms);
        track_spawn(&xCommTimeoutResponderHandle,&s_comm_spawn_ms);

        struct { TaskHandle_t *h; uint32_t *spawn; const char *name; } resp[] = {
            { &xThermalResponderHandle,     &s_thermal_spawn_ms,  "thermal_responder"  },
            { &xLowMemoryResponderHandle,   &s_mem_spawn_ms,      "low_memory_responder"},
            { &xBatteryResponderHandle,     &s_battery_spawn_ms,  "battery_responder"  },
            { &xCommTimeoutResponderHandle, &s_comm_spawn_ms,     "comm_timeout_responder"},
        };
        int nresp = (int)(sizeof(resp) / sizeof(resp[0]));

        for (int i = 0; i < nresp; i++) {
            if (*resp[i].h != NULL && *resp[i].spawn != 0) {
                uint32_t alive_ms = t_ms - *resp[i].spawn;
                if (alive_ms > WATCHDOG_RESPONDER_TTL_MS) {
                    wd_alert(resp[i].name, elapsed_s);
                    xEventGroupSetBits(xSystemEventGroup, EVT_TASK_FROZEN);
                    /* Kill the stuck responder; monitor will respawn if needed */
                    vKillResponder(resp[i].h);
                    *resp[i].spawn = 0;
                }
            }
        }

        vTaskDelayUntil(&xLastWake, xPeriod);
    }
}

/* ----------------------------------------------------------
 * vRecoveryManagerTask — PRIORITY_RECOVERY (8), event-driven
 * ---------------------------------------------------------- */
void vRecoveryManagerTask(void *pvParameters)
{
    (void)pvParameters;
    printf("[RecoveryMgr] Task started (priority %d)\n", PRIORITY_RECOVERY);

    const EventBits_t kWatchBits = EVT_TASK_FROZEN |
                                   EVT_CPU_CRITICAL |
                                   EVT_MEMORY_CRITICAL |
                                   EVT_THERMAL_CRITICAL |
                                   EVT_BATTERY_CRITICAL |
                                   EVT_COMM_TIMEOUT;

    for (;;)
    {
        EventBits_t bits = xEventGroupWaitBits(
            xSystemEventGroup,
            kWatchBits,
            pdTRUE,     /* clear on exit */
            pdFALSE,    /* any bit */
            portMAX_DELAY);

        uint32_t elapsed_s = g_sim.elapsed_s;

        if (bits & EVT_TASK_FROZEN) {
            log_event("[T=%03us] [RecoveryMgr] ESCALATED: frozen task recovered by watchdog.",
                      (unsigned)elapsed_s);
        }
        if (bits & EVT_CPU_CRITICAL) {
            log_event("[T=%03us] [RecoveryMgr] CPU critical — load shedding initiated.",
                      (unsigned)elapsed_s);
        }
        if (bits & EVT_THERMAL_CRITICAL) {
            log_event("[T=%03us] [RecoveryMgr] Thermal critical — heaters off, fans max.",
                      (unsigned)elapsed_s);
        }
        if (bits & EVT_MEMORY_CRITICAL) {
            log_event("[T=%03us] [RecoveryMgr] Memory critical — non-essential tasks suspended.",
                      (unsigned)elapsed_s);
        }
        if (bits & EVT_BATTERY_CRITICAL) {
            log_event("[T=%03us] [RecoveryMgr] Battery critical — entering low-power mode.",
                      (unsigned)elapsed_s);
        }
        if (bits & EVT_COMM_TIMEOUT) {
            log_event("[T=%03us] [RecoveryMgr] Comm timeout — re-link sequence started.",
                      (unsigned)elapsed_s);
        }
    }
}
