/* ==========================================================
 * config.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Khurshid (System Architect)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * All monitoring thresholds and scenario timing constants.
 * Priority defines mirror task_scheduler.h for convenience.
 * ========================================================== */

#ifndef CONFIG_H
#define CONFIG_H

/* ----------------------------------------------------------
 * TASK PRIORITIES  (re-exported from task_scheduler.h)
 * ---------------------------------------------------------- */
#define PRIORITY_LOGGER       9
#define PRIORITY_WATCHDOG     10
#define PRIORITY_RECOVERY     8
#define PRIORITY_RESPONDER    7
#define PRIORITY_MONITOR      3
#define PRIORITY_SIMULATOR    1

/* ----------------------------------------------------------
 * CPU LOAD THRESHOLDS  (%)
 * ---------------------------------------------------------- */
#define CFG_CPU_NORMAL_MAX    60
#define CFG_CPU_WARN_MAX      80
#define CFG_CPU_CRITICAL      80

/* ----------------------------------------------------------
 * HEAP FREE THRESHOLDS  (%)
 * ---------------------------------------------------------- */
#define CFG_HEAP_NORMAL_MIN   40
#define CFG_HEAP_WARN_MIN     20
#define CFG_HEAP_CRITICAL     20

/* ----------------------------------------------------------
 * STACK FREE THRESHOLDS  (% of STACK_MONITOR words)
 * ---------------------------------------------------------- */
#define CFG_STACK_NORMAL_MIN  30
#define CFG_STACK_WARN_MIN    15

/* ----------------------------------------------------------
 * BATTERY THRESHOLDS  (mV)
 * ---------------------------------------------------------- */
#define CFG_BAT_NORMAL_MV     3600   /* 3.6 V */
#define CFG_BAT_WARN_MV       3300   /* 3.3 V */
#define CFG_BAT_CRITICAL_MV   3300   /* 3.3 V */

/* ----------------------------------------------------------
 * TEMPERATURE THRESHOLDS  (°C)
 * ---------------------------------------------------------- */
#define CFG_TEMP_NORMAL_MAX   60
#define CFG_TEMP_WARN_MAX     80
#define CFG_TEMP_CRITICAL     80

/* ----------------------------------------------------------
 * COMM HEARTBEAT THRESHOLDS  (ms)
 * ---------------------------------------------------------- */
#define CFG_COMM_NORMAL_MAX   1000
#define CFG_COMM_WARN_MAX     3000
#define CFG_COMM_CRITICAL     3000

/* ----------------------------------------------------------
 * RADIATION THRESHOLDS  (counts / second)
 * ---------------------------------------------------------- */
#define CFG_RAD_NORMAL_MAX    5
#define CFG_RAD_STORM_MIN     40
#define CFG_RAD_STORM_MAX     80
#define CFG_RAD_STORM_DUR_S   12

/* ----------------------------------------------------------
 * SENSOR SIMULATION PARAMETERS
 * ---------------------------------------------------------- */
#define SIM_TEMP_BASELINE_C       22.0f
#define SIM_TEMP_NOISE_STD        1.5f
#define SIM_TEMP_DRIFT_PER_10     0.3f    /* °C per 10 sim ticks  */
#define SIM_TEMP_SPIKE_TARGET     92.0f
#define SIM_TEMP_SPIKE_RAMP_S     8       /* s to ramp to spike   */

#define SIM_BAT_START_V           4.1f
#define SIM_BAT_DRAIN_NORMAL_V    0.002f  /* V per sim tick       */
#define SIM_BAT_DRAIN_FAULT_V     0.015f  /* V per tick (fault)   */

#define SIM_HEARTBEAT_PERIOD_MS   800
#define SIM_COMM_BLACKOUT_S       5       /* s with no heartbeat  */

#define SIM_LEAK_BYTES            512     /* bytes per leak event */
#define SIM_LEAK_PERIOD_S         2       /* s between allocs     */

#define SIM_CPU_STORM_OFFSET      55.0f   /* % synthetic add      */
#define SIM_CPU_TASK_WEIGHT       5.0f    /* % per active task    */

/* ----------------------------------------------------------
 * SCENARIO TIMELINE  (seconds from boot)
 * ---------------------------------------------------------- */
#define SCENARIO_THERMAL_START    15
#define SCENARIO_THERMAL_END      35
#define SCENARIO_BATTERY_START    35
#define SCENARIO_BATTERY_END      55
#define SCENARIO_COMM_START       55
#define SCENARIO_COMM_END         70
#define SCENARIO_LEAK_START       70
#define SCENARIO_COMBINED_START   90
#define SCENARIO_CLEAR            110

/* ----------------------------------------------------------
 * LOGGER
 * ---------------------------------------------------------- */
#define LOG_QUEUE_DEPTH   32
#define LOG_MSG_LEN       200

/* ----------------------------------------------------------
 * WATCHDOG
 * ---------------------------------------------------------- */
#define WATCHDOG_TIMEOUT_MS       2000   /* frozen if no kick for 2 s  */
#define WATCHDOG_RESPONDER_TTL_MS 10000  /* kill stuck responder >10 s */

#endif /* CONFIG_H */
