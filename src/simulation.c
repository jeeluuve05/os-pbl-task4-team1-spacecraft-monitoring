/* ==========================================================
 * simulation.c
 * ZeroGravity — Fake Sensor Values for Testing
 * Owner : Shokhrukh (Simulation & Testing Engineer)
 * This is a STUB so Saida can build and test her scheduler.
 * Shokhrukh will replace this with proper scenarios.
 * ========================================================== */

#include "simulation.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* ----------------------------------------------------------
 * These values slowly change over time to simulate
 * a spacecraft going from normal → critical → recovered
 * ---------------------------------------------------------- */

/* Returns simulated CPU load (0–100%) */
uint32_t sim_get_cpu_load(void)
{
    /* Slowly ramp up CPU load to test the responder trigger */
    static uint32_t load = 40;
    static int direction = 1;

    load += direction * 5;
    if (load >= 95) direction = -1;
    if (load <= 40) direction = 1;

    printf("[SIM] CPU load: %u%%\n", (unsigned)load);
    return load;
}

/* Returns simulated free heap in bytes */
size_t sim_get_free_heap(void)
{
    /* Returns xPortGetFreeHeapSize() — real FreeRTOS value */
    return xPortGetFreeHeapSize();
}

/* Returns simulated temperature in °C */
int32_t sim_get_temperature(void)
{
    static int32_t temp = 50;
    static int dir = 1;

    temp += dir * 3;
    if (temp >= 90) dir = -1;
    if (temp <= 50) dir = 1;

    printf("[SIM] Temperature: %d C\n", (int)temp);
    return temp;
}

/* Returns simulated battery voltage in mV */
uint32_t sim_get_battery_mv(void)
{
    static uint32_t mv = 3800;
    static int dir = -1;

    mv += dir * 50;
    if (mv <= 3100) dir = 1;
    if (mv >= 3800) dir = -1;

    printf("[SIM] Battery: %umV\n", (unsigned)mv);
    return mv;
}

/* Returns simulated comm gap in ms */
uint32_t sim_get_comm_gap_ms(void)
{
    static uint32_t gap = 500;
    static int dir = 1;

    gap += dir * 300;
    if (gap >= 4000) dir = -1;
    if (gap <= 500)  dir = 1;

    printf("[SIM] Comm gap: %ums\n", (unsigned)gap);
    return gap;
}
