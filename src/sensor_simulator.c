/* ==========================================================
 * sensor_simulator.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Shokhrukh (Simulation & Testing Engineer)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * Sensor state machine:
 *   - Gaussian noise via Box-Muller transform
 *   - Deterministic 110-second scenario timeline
 *   - Fault injection: thermal spike, battery drain, comm
 *     blackout, memory leak, CPU storm, radiation storm
 * ========================================================== */

#include "sensor_simulator.h"
#include "config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* ----------------------------------------------------------
 * Global sensor state (volatile — read from multiple tasks)
 * ---------------------------------------------------------- */
volatile SimState_t g_sim;

/* ----------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------- */
static uint32_t   s_tick          = 0;
static float      s_temp_drift    = 0.0f;
static float      s_bat_v         = SIM_BAT_START_V;
static TickType_t s_last_hb_tick  = 0;
static int        s_blackout      = 0;

/* Memory-leak bookkeeping */
#define MAX_LEAK_PTRS   64
static void    *s_leak_ptrs[MAX_LEAK_PTRS];
static int      s_leak_count  = 0;
static uint32_t s_last_leak_s = 0;

/* ----------------------------------------------------------
 * Box-Muller Gaussian: N(mean, std)
 * ---------------------------------------------------------- */
static float gaussian(float mean, float std)
{
    float u1 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 2.0f);
    float u2 = ((float)rand() + 1.0f) / ((float)RAND_MAX + 2.0f);
    float z  = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return mean + std * z;
}

/* ----------------------------------------------------------
 * Simulated baseline CPU % via a bounded random walk.
 * The POSIX tick-interrupt saturates clock(), so we simulate
 * a realistic nominal load (15–35%) and let the scenario
 * pipeline add synthetic offsets on top.
 * ---------------------------------------------------------- */
static float host_cpu_pct(void)
{
    static float base = 22.0f;
    base += gaussian(0.0f, 1.5f);
    if (base < 10.0f) base = 10.0f;
    if (base > 35.0f) base = 35.0f;
    return base;
}

/* ----------------------------------------------------------
 * Phase transition (prints on change only)
 * ---------------------------------------------------------- */
static void set_phase(SimPhase_t next)
{
    if (g_sim.phase != next) {
        printf("[SIM] Phase: %s -> %s (t=%us)\n",
               sim_phase_name(g_sim.phase),
               sim_phase_name(next),
               (unsigned)g_sim.elapsed_s);
        g_sim.phase = next;
    }
}

/* ----------------------------------------------------------
 * Memory leak helpers
 * ---------------------------------------------------------- */
static void do_leak(void)
{
    if (s_leak_count < MAX_LEAK_PTRS) {
        void *p = pvPortMalloc(SIM_LEAK_BYTES);
        if (p) s_leak_ptrs[s_leak_count++] = p;
    }
}

static void free_leaks(void)
{
    for (int i = 0; i < s_leak_count; i++) {
        vPortFree(s_leak_ptrs[i]);
        s_leak_ptrs[i] = NULL;
    }
    s_leak_count = 0;
}

/* ----------------------------------------------------------
 * sim_sensor_init
 * ---------------------------------------------------------- */
void sim_sensor_init(void)
{
    srand((unsigned)time(NULL));

    g_sim.cpu_load_pct    = 20.0f;
    g_sim.temperature_c   = SIM_TEMP_BASELINE_C;
    g_sim.battery_v       = SIM_BAT_START_V;
    g_sim.comm_gap_ms     = 0;
    g_sim.heap_free_bytes  = (uint32_t)configTOTAL_HEAP_SIZE;
    g_sim.heap_total_bytes = (uint32_t)configTOTAL_HEAP_SIZE;
    g_sim.radiation_cps   = 3;
    g_sim.phase           = SIM_PHASE_NOMINAL;
    g_sim.elapsed_s       = 0;

    s_bat_v       = SIM_BAT_START_V;
    s_last_hb_tick = xTaskGetTickCount();
    s_tick        = 0;
    s_temp_drift  = 0.0f;
    s_blackout    = 0;
    s_leak_count  = 0;
    s_last_leak_s = 0;

    printf("[SIM] Sensor simulator initialised. 110-second scenario ready.\n");
}

/* ----------------------------------------------------------
 * sim_heartbeat — called by vGroundStationTask every 800 ms
 * ---------------------------------------------------------- */
void sim_heartbeat(void)
{
    s_last_hb_tick = xTaskGetTickCount();
}

int sim_is_blackout(void) { return s_blackout; }

/* ----------------------------------------------------------
 * sim_tick — called every 500 ms by vSimulatorTask
 * ---------------------------------------------------------- */
void sim_tick(void)
{
    s_tick++;
    uint32_t elapsed_s = s_tick / 2;   /* 2 ticks = 1 second */
    g_sim.elapsed_s = elapsed_s;

    /* ---- Scenario timeline ---- */
    if      (elapsed_s < SCENARIO_THERMAL_START)   set_phase(SIM_PHASE_NOMINAL);
    else if (elapsed_s < SCENARIO_THERMAL_END)     set_phase(SIM_PHASE_THERMAL_SPIKE);
    else if (elapsed_s < SCENARIO_BATTERY_END)     set_phase(SIM_PHASE_BATTERY_FAULT);
    else if (elapsed_s < SCENARIO_COMM_END)        set_phase(SIM_PHASE_COMM_BLACKOUT);
    else if (elapsed_s < SCENARIO_COMBINED_START)  set_phase(SIM_PHASE_MEMORY_LEAK);
    else if (elapsed_s < SCENARIO_CLEAR)           set_phase(SIM_PHASE_COMBINED);
    else                                           set_phase(SIM_PHASE_RECOVERY);

    SimPhase_t phase = g_sim.phase;

    /* ---- Temperature ---- */
    {
        /* Accumulate drift: +0.3°C per 10 sim ticks (5 s) */
        if (s_tick % 10 == 0) s_temp_drift += SIM_TEMP_DRIFT_PER_10;

        float base = SIM_TEMP_BASELINE_C + s_temp_drift;

        if (phase == SIM_PHASE_THERMAL_SPIKE) {
            uint32_t ramp_ticks = (uint32_t)(SIM_TEMP_SPIKE_RAMP_S * 2);
            uint32_t into = s_tick - (uint32_t)(SCENARIO_THERMAL_START * 2);
            float frac = (into < ramp_ticks) ? (float)into / (float)ramp_ticks : 1.0f;
            base = SIM_TEMP_BASELINE_C +
                   frac * (SIM_TEMP_SPIKE_TARGET - SIM_TEMP_BASELINE_C);
        }
        /* Radiation in COMBINED phase adds heat */
        if (phase == SIM_PHASE_COMBINED) base += 5.0f;

        /* Cool down in RECOVERY */
        if (phase == SIM_PHASE_RECOVERY && s_temp_drift > 0.0f)
            s_temp_drift -= 0.1f;

        g_sim.temperature_c = gaussian(base, SIM_TEMP_NOISE_STD);
    }

    /* ---- Battery ---- */
    {
        float drain = SIM_BAT_DRAIN_NORMAL_V;
        if (phase == SIM_PHASE_BATTERY_FAULT) drain = SIM_BAT_DRAIN_FAULT_V;
        /* Slow recovery charge */
        if (phase == SIM_PHASE_RECOVERY && s_bat_v < SIM_BAT_START_V)
            drain = -0.005f;

        s_bat_v -= drain;
        if (s_bat_v < 2.5f)           s_bat_v = 2.5f;
        if (s_bat_v > SIM_BAT_START_V) s_bat_v = SIM_BAT_START_V;
        g_sim.battery_v = s_bat_v;
    }

    /* ---- Comm gap ---- */
    {
        s_blackout = (phase == SIM_PHASE_COMM_BLACKOUT) ? 1 : 0;
        TickType_t now = xTaskGetTickCount();
        uint32_t gap_ms = (uint32_t)((now - s_last_hb_tick) *
                                     (uint32_t)portTICK_PERIOD_MS);
        g_sim.comm_gap_ms = gap_ms;
    }

    /* ---- Memory leak ---- */
    {
        int leak_on = (phase == SIM_PHASE_MEMORY_LEAK ||
                       phase == SIM_PHASE_COMBINED);
        if (leak_on) {
            if (elapsed_s >= s_last_leak_s + SIM_LEAK_PERIOD_S) {
                do_leak();
                s_last_leak_s = elapsed_s;
            }
        } else if (s_leak_count > 0) {
            free_leaks();
            s_last_leak_s = 0;
        }
        g_sim.heap_free_bytes  = (uint32_t)xPortGetFreeHeapSize();
        g_sim.heap_total_bytes = (uint32_t)configTOTAL_HEAP_SIZE;
    }

    /* ---- CPU load ---- */
    {
        float real_pct = host_cpu_pct();
        float synth    = 0.0f;
        if (phase == SIM_PHASE_THERMAL_SPIKE) synth += 10.0f;
        if (phase == SIM_PHASE_BATTERY_FAULT) synth +=  8.0f;
        if (phase == SIM_PHASE_COMBINED)      synth += SIM_CPU_STORM_OFFSET;
        float cpu = real_pct + synth + gaussian(0.0f, 2.0f);
        if (cpu < 0.0f)   cpu = 0.0f;
        if (cpu > 100.0f) cpu = 100.0f;
        g_sim.cpu_load_pct = cpu;
    }

    /* ---- Radiation ---- */
    {
        if (phase == SIM_PHASE_COMBINED) {
            g_sim.radiation_cps = (uint32_t)(CFG_RAD_STORM_MIN +
                rand() % (CFG_RAD_STORM_MAX - CFG_RAD_STORM_MIN + 1));
        } else {
            g_sim.radiation_cps = (uint32_t)(2 + rand() % CFG_RAD_NORMAL_MAX);
        }
    }
}

/* ----------------------------------------------------------
 * sim_phase_name
 * ---------------------------------------------------------- */
const char *sim_phase_name(SimPhase_t phase)
{
    switch (phase) {
        case SIM_PHASE_NOMINAL:       return "NOMINAL";
        case SIM_PHASE_THERMAL_SPIKE: return "THERMAL_SPIKE";
        case SIM_PHASE_BATTERY_FAULT: return "BATTERY_FAULT";
        case SIM_PHASE_COMM_BLACKOUT: return "COMM_BLACKOUT";
        case SIM_PHASE_MEMORY_LEAK:   return "MEMORY_LEAK";
        case SIM_PHASE_COMBINED:      return "COMBINED_FAULT";
        case SIM_PHASE_RECOVERY:      return "RECOVERY";
        default:                      return "UNKNOWN";
    }
}
