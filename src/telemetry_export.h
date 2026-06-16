/* ==========================================================
 * telemetry_export.h
 * ZeroGravity — Live Telemetry JSON Export
 *
 * DROP THIS FILE INTO: include/telemetry_export.h
 *
 * Writes current sensor state to /tmp/zg_telemetry.json
 * every second so the Python server can serve it to the
 * browser dashboard.
 * ========================================================== */
#ifndef TELEMETRY_EXPORT_H
#define TELEMETRY_EXPORT_H

/* Call once per second from vSimulatorTask (every 2 ticks at 500ms) */
void telemetry_export_write(void);

#endif /* TELEMETRY_EXPORT_H */
