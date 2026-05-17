/* ==========================================================
 * simulation.h
 * ZeroGravity — Simulated Sensor Interface
 * ========================================================== */

#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdint.h>
#include <stddef.h>

uint32_t sim_get_cpu_load      (void);
int32_t  sim_get_temperature   (void);
uint32_t sim_get_battery_mv    (void);
uint32_t sim_get_comm_gap_ms   (void);
size_t   sim_get_free_heap     (void);

#endif /* SIMULATION_H */
