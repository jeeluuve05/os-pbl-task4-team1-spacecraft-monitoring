/* ==========================================================
 * responders.h
 * ZeroGravity — Spacecraft Onboard System Monitoring
 * Designer : Saida
 * ========================================================== */

#ifndef RESPONDERS_H
#define RESPONDERS_H

#include "FreeRTOS.h"
#include "task.h"

void vCPUOverloadResponderTask    (void *pvParameters);
void vLowMemoryResponderTask      (void *pvParameters);
void vThermalResponderTask        (void *pvParameters);
void vBatteryResponderTask        (void *pvParameters);
void vCommTimeoutResponderTask    (void *pvParameters);

#endif /* RESPONDERS_H */
