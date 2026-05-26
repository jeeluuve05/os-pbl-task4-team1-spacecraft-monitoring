/* ==========================================================
 * watchdog.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Nur (Logging & Watchdog Developer)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * Layer 3 safety tasks:
 *   vWatchdogTask       — checks all tasks alive, recovers frozen
 *   vRecoveryManagerTask — handles escalated failures
 * ========================================================== */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "FreeRTOS.h"
#include "task.h"

/* ----------------------------------------------------------
 * Watchdog kick — call periodically from any monitored task
 * to signal "I am alive". Indexed by WD_TASK_* constants.
 * ---------------------------------------------------------- */
#define WD_TASK_LOGGER      0
#define WD_TASK_SIMULATOR   1
#define WD_TASK_GROUND_STN  2
#define WD_TASK_COUNT       3

extern volatile uint32_t g_wd_heartbeat_ms[WD_TASK_COUNT];

static inline void watchdog_kick(int task_id)
{
    if (task_id >= 0 && task_id < WD_TASK_COUNT) {
        g_wd_heartbeat_ms[task_id] =
            (uint32_t)(xTaskGetTickCount() * (TickType_t)portTICK_PERIOD_MS);
    }
}

/* ----------------------------------------------------------
 * Task functions
 * ---------------------------------------------------------- */
void vWatchdogTask       (void *pvParameters);
void vRecoveryManagerTask(void *pvParameters);

#endif /* WATCHDOG_H */
