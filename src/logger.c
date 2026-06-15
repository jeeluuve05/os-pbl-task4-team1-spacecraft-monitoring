/* ==========================================================
 * logger.c — Cleaned & Thread-Safe Version
 * ========================================================== */

#include "logger.h"
#include "config.h"
#include "task_scheduler.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#  include <direct.h>
#  define MAKE_LOGS_DIR()  _mkdir("logs")
#else
#  include <sys/stat.h>
#  define MAKE_LOGS_DIR()  mkdir("logs", 0755)
#endif

QueueHandle_t     xLogQueue   = NULL;
SemaphoreHandle_t xPrintMutex = NULL;

static FILE *s_log_file = NULL;

void logger_init(void)
{
    xLogQueue   = xQueueCreate(LOG_QUEUE_DEPTH, LOG_MSG_LEN);
    xPrintMutex = xSemaphoreCreateMutex();

    configASSERT(xLogQueue   != NULL);
    configASSERT(xPrintMutex != NULL);

    MAKE_LOGS_DIR();   
    s_log_file = fopen("logs/system.log", "w");
    if (!s_log_file) {
        printf("[Logger] WARNING: could not open logs/system.log\n");
    }

    printf("[Logger] Initialised. Queue depth=%d, msg_len=%d\n",
           LOG_QUEUE_DEPTH, LOG_MSG_LEN);
}

/* Internal: Only called by vLoggerTask */
static void write_msg(const char *msg)
{
    // Block indefinitely until we get the console/file lock
    if (xSemaphoreTake(xPrintMutex, portMAX_DELAY) == pdTRUE) {
        printf("%s\n", msg);
        if (s_log_file) {
            fprintf(s_log_file, "%s\n", msg);
            fflush(s_log_file);
        }
        xSemaphoreGive(xPrintMutex);
    }
}

void vLoggerTask(void *pvParameters)
{
    (void)pvParameters;
    // static is perfectly safe here because only ONE task runs this loop
    static char buf[LOG_MSG_LEN]; 

    for (;;) {
        if (xQueueReceive(xLogQueue, buf, portMAX_DELAY) == pdTRUE) {
            write_msg(buf);
        }
    }
}

void log_event(const char *fmt, ...)
{
    // REMOVED 'static'. Now safe for multiple concurrent task calls.
    char buf[LOG_MSG_LEN]; 
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (xLogQueue) {
        // Non-blocking send — drop if queue full to protect flight-critical deadlines
        xQueueSend(xLogQueue, buf, 0);
    }
}

void log_status_line(uint32_t elapsed_s,
                     float cpu_pct, float temp_c,
                     float bat_v,   float comm_s,
                     uint32_t heap_pct)
{
    const char *cpu_status  = (cpu_pct    > CFG_CPU_CRITICAL)     ? "CRIT" :
                              (cpu_pct    > CFG_CPU_NORMAL_MAX)   ? "WARN" : "OK";
    const char *temp_status = (temp_c     > CFG_TEMP_CRITICAL)    ? "CRIT" :
                              (temp_c     > CFG_TEMP_NORMAL_MAX)  ? "WARN" : "OK";
    const char *bat_status  = (bat_v * 1000 < CFG_BAT_CRITICAL_MV)  ? "CRIT" :
                              (bat_v * 1000 < CFG_BAT_NORMAL_MV)    ? "WARN" : "OK";
    const char *comm_status = (comm_s * 1000 > CFG_COMM_CRITICAL)   ? "CRIT" :
                              (comm_s * 1000 > CFG_COMM_NORMAL_MAX) ? "WARN" : "OK";
    const char *heap_status = (heap_pct   < CFG_HEAP_CRITICAL)    ? "CRIT" :
                              (heap_pct   < CFG_HEAP_NORMAL_MIN)  ? "WARN" : "OK";

    char line[LOG_MSG_LEN]; // Removed 'static' here as well for safety
    snprintf(line, sizeof(line),
             "[T=%03us] [CPU:%.1f%% %s] [HEAP:%u%% %s] "
             "[TEMP:%.1f\xc2\xb0""C %s] [BAT:%.2fV %s] [COMM:%.1fs %s]",
             (unsigned)elapsed_s,
             cpu_pct,  cpu_status,
             heap_pct, heap_status,
             temp_c,   temp_status,
             bat_v,    bat_status,
             comm_s,   comm_status);

    // Routed through the queue instead of calling write_msg directly
    if (xLogQueue) {
        xQueueSend(xLogQueue, line, 0);
    }
}
