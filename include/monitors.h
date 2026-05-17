/* ==========================================================
 * monitors.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Designer : Saida
 * ========================================================== */

#ifndef MONITORS_H
#define MONITORS_H

#include "FreeRTOS.h"
#include "task.h"

void vCPUMonitorTask    (void *pvParameters);
void vMemoryMonitorTask (void *pvParameters);
void vSensorMonitorTask (void *pvParameters);

#endif /* MONITORS_H */
