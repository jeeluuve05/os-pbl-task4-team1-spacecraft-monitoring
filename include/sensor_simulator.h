/* ==========================================================
 * sensor_simulator.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Shokhrukh (Simulation & Testing Engineer)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * Sensor state machine with Gaussian noise, deterministic
 * scenario timeline, and fault injection events.
 * ========================================================== */

#ifndef SENSOR_SIMULATOR_H
#define SENSOR_SIMULATOR_H

#include <stdint.h>
#include <stddef.h>

/* ----------------------------------------------------------
 * Scenario phase — drives which faults are active
 * ---------------------------------------------------------- */
typedef enum {
    SIM_PHASE_NOMINAL = 0,
    SIM_PHASE_THERMAL_SPIKE,
    SIM_PHASE_BATTERY_FAULT,
    SIM_PHASE_COMM_BLACKOUT,
    SIM_PHASE_MEMORY_LEAK,
    SIM_PHASE_COMBINED,      /* memory leak + CPU storm + radiation */
    SIM_PHASE_RECOVERY
} SimPhase_t;

/* ----------------------------------------------------------
 * Live sensor state — readable by any task
 * ---------------------------------------------------------- */
typedef struct {
    float       cpu_load_pct;
    float       temperature_c;
    float       battery_v;
    uint32_t    comm_gap_ms;
    uint32_t    heap_free_bytes;
    uint32_t    heap_total_bytes;
    uint32_t    radiation_cps;
    SimPhase_t  phase;
    uint32_t    elapsed_s;
} SimState_t;

extern volatile SimState_t g_sim;

/* ----------------------------------------------------------
 * Public API
 * sim_get_* compatibility shims live in simulation.c so that
 * simulation.h / Saida's monitors.c compile unchanged.
 * ---------------------------------------------------------- */

/** Call once before vTaskStartScheduler(). Initialises state. */
void sim_sensor_init(void);

/** Called by vSimulatorTask every 500 ms. Advances scenario. */
void sim_tick(void);

/** Called by vGroundStationTask every 800 ms (unless blackout). */
void sim_heartbeat(void);

/** Returns 1 if the comm blackout fault is currently active. */
int sim_is_blackout(void);

/** Human-readable phase name for logging. */
const char *sim_phase_name(SimPhase_t phase);

#endif /* SENSOR_SIMULATOR_H */
