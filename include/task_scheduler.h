/* ==========================================================
 * task_scheduler.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Designer : Saida (Task & Scheduler Designer)
 * ========================================================== */

#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

/* ----------------------------------------------------------
 * TASK PRIORITIES
 * Higher number = higher priority in FreeRTOS
 * ---------------------------------------------------------- */
#define PRIORITY_SAFETY_WATCHDOG      10
#define PRIORITY_SAFETY_LOGGER         9
#define PRIORITY_SAFETY_RECOVERY       8
#define PRIORITY_RESPONDER             7   /* all dynamic responders share this */
#define PRIORITY_CORE_MONITOR          3   /* all always-on monitors share this */

/* ----------------------------------------------------------
 * STACK SIZES (in words)
 * ---------------------------------------------------------- */
#define STACK_SAFETY                  2048
#define STACK_RESPONDER               1024
#define STACK_MONITOR                  512

/* ----------------------------------------------------------
 * TASK PERIODS (in ms, used with vTaskDelayUntil)
 * ---------------------------------------------------------- */
#define PERIOD_CPU_MONITOR_MS         1000
#define PERIOD_MEMORY_MONITOR_MS      1000
#define PERIOD_SENSOR_MONITOR_MS      2000
#define PERIOD_WATCHDOG_MS             500

/* ----------------------------------------------------------
 * THRESHOLD VALUES
 * ---------------------------------------------------------- */
#define THRESH_CPU_CRITICAL_PCT        80   /* % — spawn responder above this  */
#define THRESH_HEAP_CRITICAL_PCT       20   /* % — spawn responder below this  */
#define THRESH_STACK_WARN_PCT          15   /* % — log warning below this      */
#define THRESH_BATTERY_CRITICAL_V     330   /* mV (3.30V)                      */
#define THRESH_TEMP_CRITICAL_C         80   /* °C                              */
#define THRESH_COMM_TIMEOUT_MS        3000  /* ms — spawn responder above this */

/* ----------------------------------------------------------
 * EVENT GROUP BITS  (used by safety layer / watchdog)
 * ---------------------------------------------------------- */
#define EVT_CPU_CRITICAL        ( 1 << 0 )
#define EVT_MEMORY_CRITICAL     ( 1 << 1 )
#define EVT_THERMAL_CRITICAL    ( 1 << 2 )
#define EVT_BATTERY_CRITICAL    ( 1 << 3 )
#define EVT_COMM_TIMEOUT        ( 1 << 4 )
#define EVT_TASK_FROZEN         ( 1 << 5 )

/* ----------------------------------------------------------
 * SHARED HANDLES  (defined in task_scheduler.c)
 * ---------------------------------------------------------- */

/* Always-on task handles */
extern TaskHandle_t xCPUMonitorHandle;
extern TaskHandle_t xMemoryMonitorHandle;
extern TaskHandle_t xSensorMonitorHandle;

/* Dynamic responder task handles (NULL when not running) */
extern TaskHandle_t xCPUResponderHandle;
extern TaskHandle_t xLowMemoryResponderHandle;
extern TaskHandle_t xThermalResponderHandle;
extern TaskHandle_t xBatteryResponderHandle;
extern TaskHandle_t xCommTimeoutResponderHandle;

/* Queue handles — monitors send data, responders receive */
extern QueueHandle_t xCPUDataQueue;
extern QueueHandle_t xMemoryDataQueue;
extern QueueHandle_t xSensorDataQueue;

/* Event group — monitors signal safety layer */
extern EventGroupHandle_t xSystemEventGroup;

/* ----------------------------------------------------------
 * SENSOR DATA STRUCTURES
 * ---------------------------------------------------------- */
typedef struct {
    uint32_t cpu_load_pct;      /* 0–100 */
    uint32_t timestamp_ms;
} CPUData_t;

typedef struct {
    uint32_t heap_free_bytes;
    uint32_t heap_total_bytes;
    uint32_t heap_free_pct;     /* 0–100 */
    UBaseType_t stack_high_water; /* lowest free stack words seen */
    uint32_t timestamp_ms;
} MemoryData_t;

typedef struct {
    int32_t  temperature_c;     /* °C  */
    uint32_t battery_mv;        /* mV  */
    uint32_t comm_last_reply_ms;/* ms since last comm reply */
    uint32_t timestamp_ms;
} SensorData_t;

/* ----------------------------------------------------------
 * PUBLIC FUNCTIONS
 * ---------------------------------------------------------- */

/**
 * @brief  Create all always-on tasks and shared RTOS objects.
 *         Call once before vTaskStartScheduler().
 */
void vSchedulerInit(void);

/**
 * @brief  Spawn a dynamic responder if not already running.
 *         Safe to call from any task context.
 */
void vSpawnResponder(TaskFunction_t pvTask,
                     const char * const pcName,
                     TaskHandle_t *pxHandle);

/**
 * @brief  Terminate a dynamic responder by handle.
 *         Sets the handle to NULL after deletion.
 */
void vKillResponder(TaskHandle_t *pxHandle);

#endif /* TASK_SCHEDULER_H */
