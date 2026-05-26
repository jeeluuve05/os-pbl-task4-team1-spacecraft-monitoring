/* ==========================================================
 * logger.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Nur (Logging & Watchdog Developer)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * Event logger: receives formatted strings via a queue and
 * writes them to both stdout and logs/system.log.
 * A mutex guards all printf calls across the system.
 * ========================================================== */

#ifndef LOGGER_H
#define LOGGER_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Shared log queue and print mutex — defined in logger.c */
extern QueueHandle_t    xLogQueue;
extern SemaphoreHandle_t xPrintMutex;

/**
 * @brief  Create the log queue, print mutex, and open
 *         logs/system.log for writing. Call before
 *         vTaskStartScheduler().
 */
void logger_init(void);

/**
 * @brief  FreeRTOS task: drains xLogQueue and writes each
 *         message to stdout + logs/system.log.
 */
void vLoggerTask(void *pvParameters);

/**
 * @brief  Format a log message and enqueue it. Thread-safe.
 *         Drops silently if the queue is full.
 */
void log_event(const char *fmt, ...);

/**
 * @brief  Print the per-tick consolidated status line.
 *         Called by vSimulatorTask every 1000 ms.
 */
void log_status_line(uint32_t elapsed_s,
                     float cpu_pct, float temp_c,
                     float bat_v,  float comm_s,
                     uint32_t heap_pct);

#endif /* LOGGER_H */
