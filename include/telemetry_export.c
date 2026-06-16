/* ==========================================================
 * telemetry_export.c
 * ZeroGravity — Live Telemetry JSON Export
 *
 * DROP THIS FILE INTO: src/telemetry_export.c
 * ADD TO MAKEFILE:     src/telemetry_export.c  \
 *
 * HOW IT WORKS:
 *   Every second, writes /tmp/zg_telemetry.json with the
 *   current sensor state. The Python server (zerogravity_server.py)
 *   reads this file and serves it to the browser dashboard.
 * ========================================================== */

#include "../include/telemetry_export.h"
#include "../include/sensor_simulator.h"
#include "../include/config.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Output file — accessible by Python server in same WSL session */
#define TELEMETRY_FILE "/tmp/zg_telemetry.json"

/* Responder task handles — declared extern so we can check NULL */
extern TaskHandle_t xMemoryResponderHandle;
extern TaskHandle_t xThermalResponderHandle;
extern TaskHandle_t xBatteryResponderHandle;
extern TaskHandle_t xCommsResponderHandle;

/* ── Phase name helper ── */
static const char *phase_str(SimPhase_t p) {
    switch(p) {
        case SIM_PHASE_NOMINAL:      return "NOMINAL";
        case SIM_PHASE_THERMAL_SPIKE:return "THERMAL_SPIKE";
        case SIM_PHASE_BATTERY_FAULT:return "BATTERY_FAULT";
        case SIM_PHASE_COMM_BLACKOUT:return "COMM_BLACKOUT";
        case SIM_PHASE_MEMORY_LEAK:  return "MEMORY_LEAK";
        case SIM_PHASE_COMBINED:     return "COMBINED";
        case SIM_PHASE_RECOVERY:     return "RECOVERY";
        default:                     return "UNKNOWN";
    }
}

/* ── Classify sensor value ── */
static const char *classify_cpu(float v) {
    if (v >= CFG_CPU_CRITICAL)    return "CRITICAL";
    if (v >= CFG_CPU_WARN_MAX)    return "WARNING";
    return "NOMINAL";
}
static const char *classify_temp(float v) {
    if (v >= CFG_TEMP_CRITICAL)   return "CRITICAL";
    if (v >= CFG_TEMP_WARN_MAX)   return "WARNING";
    return "NOMINAL";
}
static const char *classify_batt(float v) {
    if (v * 1000.0f <= CFG_BAT_CRITICAL_MV) return "CRITICAL";
    if (v * 1000.0f <= CFG_BAT_WARN_MV)     return "WARNING";
    return "NOMINAL";
}
static const char *classify_mem(uint32_t total, uint32_t free_b) {
    if (total == 0) return "NOMINAL";
    uint32_t pct = free_b * 100 / total;
    if (pct <= (uint32_t)CFG_HEAP_CRITICAL)  return "CRITICAL";
    if (pct <= (uint32_t)CFG_HEAP_WARN_MIN)  return "WARNING";
    return "NOMINAL";
}
static const char *classify_comms(uint32_t gap_ms) {
    if (gap_ms >= (uint32_t)CFG_COMM_CRITICAL) return "CRITICAL";
    if (gap_ms >= (uint32_t)CFG_COMM_WARN_MAX) return "WARNING";
    return "NOMINAL";
}

/* ── Main export function — call every second ── */
void telemetry_export_write(void) {
    FILE *f = fopen(TELEMETRY_FILE, "w");
    if (!f) return;   /* silently skip if /tmp not writable */

    SimState_t s = g_sim;  /* snapshot (volatile, read once) */

    uint32_t heap_pct = (s.heap_total_bytes > 0)
        ? (s.heap_free_bytes * 100 / s.heap_total_bytes)
        : 0;

    float comms_s = s.comm_gap_ms / 1000.0f;

    /* MET string: T+HH:MM:SS */
    uint32_t el = s.elapsed_s;
    char met[16];
    snprintf(met, sizeof(met), "T+%02u:%02u:%02u",
             el / 3600, (el % 3600) / 60, el % 60);

    /* Responder states */
    int resp_mem   = (xMemoryResponderHandle   != NULL) ? 1 : 0;
    int resp_therm = (xThermalResponderHandle  != NULL) ? 1 : 0;
    int resp_batt  = (xBatteryResponderHandle  != NULL) ? 1 : 0;
    int resp_comms = (xCommsResponderHandle    != NULL) ? 1 : 0;

    /* Active FreeRTOS task count */
    UBaseType_t task_count = uxTaskGetNumberOfTasks();

    fprintf(f,
        "{\n"
        "  \"source\": \"FreeRTOS\",\n"
        "  \"met\": \"%s\",\n"
        "  \"elapsed_s\": %u,\n"
        "  \"phase\": \"%s\",\n"
        "  \"task_count\": %u,\n"
        "  \"sensors\": {\n"
        "    \"cpu\":   { \"value\": %.1f, \"unit\": \"%%\",  \"status\": \"%s\" },\n"
        "    \"temp\":  { \"value\": %.1f, \"unit\": \"C\",   \"status\": \"%s\" },\n"
        "    \"batt\":  { \"value\": %.3f, \"unit\": \"V\",   \"status\": \"%s\" },\n"
        "    \"mem\":   { \"value\": %u,   \"unit\": \"%%\",  \"status\": \"%s\" },\n"
        "    \"comms\": { \"value\": %.2f, \"unit\": \"s\",   \"status\": \"%s\" },\n"
        "    \"rad\":   { \"value\": %u,   \"unit\": \"cps\", \"status\": \"%s\" }\n"
        "  },\n"
        "  \"responders\": {\n"
        "    \"mem\":   %s,\n"
        "    \"therm\": %s,\n"
        "    \"batt\":  %s,\n"
        "    \"comms\": %s\n"
        "  }\n"
        "}\n",
        met,
        (unsigned)el,
        phase_str(s.phase),
        (unsigned)task_count,
        s.cpu_load_pct,    classify_cpu(s.cpu_load_pct),
        s.temperature_c,   classify_temp(s.temperature_c),
        s.battery_v,       classify_batt(s.battery_v),
        (unsigned)heap_pct,classify_mem(s.heap_total_bytes, s.heap_free_bytes),
        comms_s,           classify_comms(s.comm_gap_ms),
        (unsigned)s.radiation_cps,
                           s.radiation_cps >= 40 ? "STORM" :
                           s.radiation_cps >= 15 ? "ELEVATED" : "NOMINAL",
        resp_mem   ? "true" : "false",
        resp_therm ? "true" : "false",
        resp_batt  ? "true" : "false",
        resp_comms ? "true" : "false"
    );

    fclose(f);
}
