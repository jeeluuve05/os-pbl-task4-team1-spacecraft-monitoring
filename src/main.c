/* ==========================================================
 * main.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Architect : Khurshid  |  Scheduler : Saida
 * Simulator : Shokhrukh |  Logger/WD : Nur
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 * ========================================================== */

#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* ZeroGravity */
#include "task_scheduler.h"
#include "sensor_simulator.h"
#include "logger.h"
#include "tasks/watchdog.h"
#include "config.h"

/* ----------------------------------------------------------
 * vSimulatorTask — PRIORITY_SIMULATOR (1), period 500 ms
 * Advances the sensor state machine and prints the status line
 * every second (every other tick).
 * ---------------------------------------------------------- */
static void vSimulatorTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(500);
    uint32_t tick_count = 0;

    for (;;) {
        sim_tick();
        tick_count++;

        /* Print consolidated status line every 1000 ms (every 2 ticks) */
        if (tick_count % 2 == 0) {
            uint32_t heap_pct = (g_sim.heap_total_bytes > 0) ?
                (g_sim.heap_free_bytes * 100 / g_sim.heap_total_bytes) : 0;

            log_status_line(g_sim.elapsed_s,
                            g_sim.cpu_load_pct,
                            g_sim.temperature_c,
                            g_sim.battery_v,
                            g_sim.comm_gap_ms / 1000.0f,
                            heap_pct);
        }

        /* Print responder spawn alert once per thermal event */
        {
            static int thermal_alerted = 0;
            if (g_sim.temperature_c > CFG_TEMP_CRITICAL && xThermalResponderHandle != NULL) {
                if (!thermal_alerted) {
                    log_event("[T=%03us] *** CRITICAL: TEMP=%.1f\xc2\xb0""C"
                              " -- THERMAL RESPONDER CREATED (priority %d) ***",
                              (unsigned)g_sim.elapsed_s, g_sim.temperature_c,
                              PRIORITY_RESPONDER);
                    thermal_alerted = 1;
                }
            } else {
                thermal_alerted = 0;
            }
        }

        watchdog_kick(WD_TASK_SIMULATOR);
        vTaskDelayUntil(&xLastWake, xPeriod);
    }
}

/* ----------------------------------------------------------
 * vGroundStationTask — sends comm heartbeat every 800 ms
 * Skips heartbeat during comm blackout phase.
 * ---------------------------------------------------------- */
static void vGroundStationTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(SIM_HEARTBEAT_PERIOD_MS);

    for (;;) {
        if (!sim_is_blackout()) {
            sim_heartbeat();
        }
        watchdog_kick(WD_TASK_GROUND_STN);
        vTaskDelayUntil(&xLastWake, xPeriod);
    }
}

/* ----------------------------------------------------------
 * FreeRTOS application hooks
 * ---------------------------------------------------------- */
void vApplicationIdleHook(void)
{
    /* Nothing needed for POSIX simulation */
}

void vApplicationMallocFailedHook(void)
{
    printf("[FATAL] Heap allocation failed! System halting.\n");
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("[FATAL] Stack overflow in task: %s\n", pcTaskName);
    for (;;);
}

/* ----------------------------------------------------------
 * main
 * ---------------------------------------------------------- */
int main(void)
{
    printf("============================================\n");
    printf("  ZeroGravity — Spacecraft Monitor\n");
    printf("  FreeRTOS POSIX Simulator (macOS/Linux)\n");
    printf("  Team: Khurshid | Saida | Shokhrukh | Nur\n");
    printf("============================================\n\n");

    /* 1. Init sensor simulator (sets baseline state) */
    sim_sensor_init();

    /* 2. Init logger (creates queue + mutex + opens log file) */
    logger_init();

    /* 3. Init Layer-1 monitors + shared RTOS objects */
    vSchedulerInit();

    /* 4. Layer-3: Logger task */
    xTaskCreate(vLoggerTask,
                "LoggerTask",
                STACK_SAFETY,
                NULL,
                PRIORITY_SAFETY_LOGGER,
                NULL);

    /* 5. Layer-3: Watchdog task */
    xTaskCreate(vWatchdogTask,
                "WatchdogTask",
                STACK_SAFETY,
                NULL,
                PRIORITY_SAFETY_WATCHDOG,
                NULL);

    /* 6. Layer-3: Recovery manager task */
    xTaskCreate(vRecoveryManagerTask,
                "RecoveryMgr",
                STACK_SAFETY,
                NULL,
                PRIORITY_SAFETY_RECOVERY,
                NULL);

    /* 7. Ground station comm task */
    xTaskCreate(vGroundStationTask,
                "GroundStation",
                STACK_MONITOR,
                NULL,
                PRIORITY_CORE_MONITOR,
                NULL);

    /* 8. Simulator tick task (lowest priority) */
    xTaskCreate(vSimulatorTask,
                "SimTask",
                STACK_MONITOR,
                NULL,
                PRIORITY_SIMULATOR,
                NULL);

    printf("[Main] All tasks created. Starting FreeRTOS scheduler...\n\n");

    vTaskStartScheduler();

    printf("[Main] ERROR: Scheduler exited unexpectedly!\n");
    return 1;
}
