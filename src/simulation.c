/* ==========================================================
 * simulation.c
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Author    : Shokhrukh (Simulation & Testing Engineer)
 * Platform  : FreeRTOS POSIX Simulator (macOS / Linux GCC)
 *
 * Compatibility adapter: exposes the sim_get_* interface
 * declared in simulation.h (used by Saida's monitors.c) by
 * reading directly from the live g_sim state maintained in
 * sensor_simulator.c. No logic lives here.
 * ========================================================== */

#include "simulation.h"
#include "sensor_simulator.h"
#include "FreeRTOS.h"
#include "task.h"

uint32_t sim_get_cpu_load(void)
{
    return (uint32_t)g_sim.cpu_load_pct;
}

int32_t sim_get_temperature(void)
{
    return (int32_t)g_sim.temperature_c;
}

uint32_t sim_get_battery_mv(void)
{
    return (uint32_t)(g_sim.battery_v * 1000.0f);
}

uint32_t sim_get_comm_gap_ms(void)
{
    return g_sim.comm_gap_ms;
}

size_t sim_get_free_heap(void)
{
    return (size_t)xPortGetFreeHeapSize();
}
