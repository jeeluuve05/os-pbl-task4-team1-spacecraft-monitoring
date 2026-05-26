/* ==========================================================
 * logger.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Nur (Logging & Watchdog Developer)
 * Platform  : FreeRTOS POSIX Simulator (macOS/GCC) + Win32 Simulator (MSVC)
 *
 * Provides:
 *   - xLogQueue   : tasks send log strings here
 *   - xPrintMutex : guards all printf / fprintf calls
 *   - vLoggerTask : drains queue → stdout + logs/system.log
 *   - log_event   : variadic helper for any task to log
 *   - log_status_line : pretty per-tick consolidated line
 * ========================================================== */

#include "logger.h"
#include "config.h"
#include "task_scheduler.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Cross-platform directory creation for the logs/ folder */
#ifdef _WIN32
#  include <direct.h>
#  define MAKE_LOGS_DIR()  _mkdir("logs")
#else
#  include <sys/stat.h>
#  define MAKE_LOGS_DIR()  mkdir("logs", 0755)
#endif

/* ----------------------------------------------------------
 * Shared handles — extern in logger.h
 * ---------------------------------------------------------- */
QueueHandle_t     xLogQueue   = NULL;
SemaphoreHandle_t xPrintMutex = NULL;

/* ----------------------------------------------------------
 * Internal file handle
 * ---------------------------------------------------------- */
static FILE *s_log_file = NULL;

/* ----------------------------------------------------------
 * logger_init
 * ---------------------------------------------------------- */
void logger_init(void)
{
    xLogQueue   = xQueueCreate(LOG_QUEUE_DEPTH, LOG_MSG_LEN);
    xPrintMutex = xSemaphoreCreateMutex();

    configASSERT(xLogQueue   != NULL);
    configASSERT(xPrintMutex != NULL);

    MAKE_LOGS_DIR();   /* no-op if directory already exists */
    s_log_file = fopen("logs/system.log", "w");
    if (!s_log_file) {
        printf("[Logger] WARNING: could not open logs/system.log\n");
    }

    printf("[Logger] Initialised. Queue depth=%d, msg_len=%d\n",
           LOG_QUEUE_DEPTH, LOG_MSG_LEN);
}

/* ----------------------------------------------------------
 * Internal: write one message to stdout + log file
 * ---------------------------------------------------------- */
static void write_msg(const char *msg)
{
    if (xSemaphoreTake(xPrintMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        printf("%s\n", msg);
        if (s_log_file) {
            fprintf(s_log_file, "%s\n", msg);
            fflush(s_log_file);
        }
        xSemaphoreGive(xPrintMutex);
    }
}

/* ----------------------------------------------------------
 * vLoggerTask — PRIORITY_LOGGER (9)
 * ---------------------------------------------------------- */
void vLoggerTask(void *pvParameters)
{
    (void)pvParameters;
    static char buf[LOG_MSG_LEN];

    for (;;) {
        if (xQueueReceive(xLogQueue, buf, portMAX_DELAY) == pdTRUE) {
            write_msg(buf);
        }
    }
}

/* ----------------------------------------------------------
 * log_event — safe to call from any task
 * ---------------------------------------------------------- */
void log_event(const char *fmt, ...)
{
    static char buf[LOG_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Non-blocking send — drop if queue full rather than block */
    if (xLogQueue) {
        xQueueSend(xLogQueue, buf, 0);
    }
}

/* ----------------------------------------------------------
 * log_status_line
 * Format: [T=045s] [CPU:23.4% OK] [HEAP:67% OK] ...
 * ---------------------------------------------------------- */
void log_status_line(uint32_t elapsed_s,
                     float cpu_pct, float temp_c,
                     float bat_v,   float comm_s,
                     uint32_t heap_pct)
{
    /* Status string helpers */
    const char *cpu_status  = (cpu_pct  > CFG_CPU_CRITICAL)   ? "CRIT" :
                              (cpu_pct  > CFG_CPU_NORMAL_MAX)  ? "WARN" : "OK";
    const char *temp_status = (temp_c   > CFG_TEMP_CRITICAL)   ? "CRIT" :
                              (temp_c   > CFG_TEMP_NORMAL_MAX)  ? "WARN" : "OK";
    const char *bat_status  = (bat_v * 1000 < CFG_BAT_CRITICAL_MV) ? "CRIT" :
                              (bat_v * 1000 < CFG_BAT_NORMAL_MV)   ? "WARN" : "OK";
    const char *comm_status = (comm_s * 1000 > CFG_COMM_CRITICAL)   ? "CRIT" :
                              (comm_s * 1000 > CFG_COMM_NORMAL_MAX)  ? "WARN" : "OK";
    const char *heap_status = (heap_pct < CFG_HEAP_CRITICAL)   ? "CRIT" :
                              (heap_pct < CFG_HEAP_NORMAL_MIN)  ? "WARN" : "OK";

    static char line[LOG_MSG_LEN];
    snprintf(line, sizeof(line),
             "[T=%03us] [CPU:%.1f%% %s] [HEAP:%u%% %s] "
             "[TEMP:%.1f\xc2\xb0""C %s] [BAT:%.2fV %s] [COMM:%.1fs %s]",
             (unsigned)elapsed_s,
             cpu_pct,  cpu_status,
             heap_pct, heap_status,
             temp_c,   temp_status,
             bat_v,    bat_status,
             comm_s,   comm_status);

    write_msg(line);
}
