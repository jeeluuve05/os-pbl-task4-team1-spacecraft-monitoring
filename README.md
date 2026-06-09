# Spacecraft Onboard System Monitoring
**Team ZeroGravity** | OS PBL Week 9 Task 4 | Woosong University 2026

FreeRTOS-based dynamic task management system that simulates spacecraft health monitoring. The system monitors CPU, memory, battery, temperature, and communication — dynamically creating and terminating responder tasks based on real-time sensor conditions across a scripted 110-second fault-injection timeline.

---

## Team Members

| Name | Technical Role | Sub-Role |
|---|---|---|
| Khurshid | Leader / FreeRTOS System Architect | — |
| Saida | Task & Scheduler Designer | Co-Leader |
| Shokhrukh | Simulation & Testing Engineer | Organization of Project Progress |
| Nur | Logging & Watchdog Developer | Presentation Maker |
| Pakhe Jong | Documentation & Analysis | Meeting Notes / Report Writer |
| Jinhyonso | Documentation & Analysis | Meeting Notes / Report Writer |

---

## System Architecture

### 3-Layer Design

```
Layer 3 — System Safety
    WatchdogTask (priority 10, 500ms) | LoggerTask (priority 9, event-driven)
    RecoveryManager (priority 8, event-driven)

Layer 2 — Condition-Triggered Responders (priority 7, all dynamic)
    CPUOverload | LowMemory | Thermal | Battery | CommTimeout
    (spawned by Layer-1 monitors when threshold breached,
     self-delete when signal recovers)

Layer 1 — Always-On Core Monitors (priority 3)
    CPUMonitor (1000ms) | MemoryMonitor (1000ms) | SensorMonitor (2000ms)

Supporting Tasks
    GroundStation (priority 3, 800ms heartbeat) | Simulator (priority 1, 500ms tick)
```

### Monitored Signals & Thresholds

| Signal | Normal | Warning | Critical — spawns responder |
|---|---|---|---|
| CPU Load | < 60% | 60–80% | > 80% |
| Heap Free | > 40% | 20–40% | < 20% |
| Stack Free | > 30% | 15–30% | < 15% (log only) |
| Battery | > 3.6 V | 3.3–3.6 V | < 3.3 V |
| Temperature | 0–60 °C | 60–80 °C | > 80 °C |
| Comm Heartbeat | < 1 s gap | 1–3 s gap | > 3 s no reply |

---

## Task Priority Table

| Task | Priority | Period | Type |
|---|---|---|---|
| WatchdogTask | 10 | 500 ms | Always-on (Layer 3) |
| LoggerTask | 9 | event-driven | Always-on (Layer 3) |
| RecoveryManager | 8 | event-driven | Always-on (Layer 3) |
| CPUOverloadResponder | 7 | 500 ms poll | Dynamic (Layer 2) |
| LowMemoryResponder | 7 | 500 ms poll | Dynamic (Layer 2) |
| ThermalResponder | 7 | 1000 ms poll | Dynamic (Layer 2) |
| BatteryResponder | 7 | 1000 ms poll | Dynamic (Layer 2) |
| CommTimeoutResponder | 7 | 500 ms poll | Dynamic (Layer 2) |
| CPUMonitor | 3 | 1000 ms | Always-on (Layer 1) |
| MemoryMonitor | 3 | 1000 ms | Always-on (Layer 1) |
| SensorMonitor | 3 | 2000 ms | Always-on (Layer 1) |
| GroundStation | 3 | 800 ms | Always-on (supporting) |
| SimulatorTask | 1 | 500 ms | Always-on (supporting) |

---

## Fault Scenario Timeline (110 seconds)

The sensor state machine in `sensor_simulator.c` drives a deterministic scenario sequence. Each phase is logged with a `[SIM] Phase:` transition line.

| Time | Phase | Faults Active |
|---|---|---|
| 0–15 s | NOMINAL | All signals normal |
| 15–35 s | THERMAL_SPIKE | Temperature ramps to ~92 °C over 8 s; CPU +10% synthetic load |
| 35–55 s | BATTERY_FAULT | Fast battery drain (0.015 V/tick vs. 0.002 V/tick normal) |
| 55–70 s | COMM_BLACKOUT | Ground-station heartbeats blocked; comm gap grows |
| 70–90 s | MEMORY_LEAK | 512-byte allocations every 2 s (up to 64 blocks, never freed) |
| 90–110 s | COMBINED_FAULT | All faults simultaneously; CPU +55% synthetic offset |
| 110 s+ | RECOVERY | Battery recharges, temperature cools, leaked memory freed |

---

## Project Structure

```
include/
├── FreeRTOSConfig.h        — POSIX/Win32 tick rate, stack, assert guards
├── config.h                — all thresholds + scenario timing constants
├── logger.h                — logger queue API (log_event, log_status_line)
├── monitors.h              — Layer-1 monitor task declarations
├── responders.h            — Layer-2 responder task declarations
├── sensor_simulator.h      — SimState_t struct, SimPhase_t enum, sim_tick/init
├── simulation.h            — sim_get_* interface declarations
├── task_scheduler.h        — handles, queues, event bits, priorities, data structs
└── tasks/
    ├── watchdog.h          — heartbeat API (g_wd_heartbeat_ms, WD_TASK_COUNT)
    ├── dynamic_tasks.h     — (structural placeholder)
    └── monitor_tasks.h     — (structural placeholder)

src/
├── main.c                  — entry point; inits sim, logger, scheduler; starts all tasks
├── task_scheduler.c        — vSchedulerInit(), vSpawnResponder(), vKillResponder()
├── monitors.c              — vCPUMonitorTask, vMemoryMonitorTask, vSensorMonitorTask
├── responders.c            — 5 dynamic responder tasks (CPU, Memory, Thermal, Battery, Comm)
├── sensor_simulator.c      — 110 s fault-injection state machine with Gaussian noise (Shokhrukh)
├── simulation.c            — thin adapter; reads g_sim and exposes sim_get_* interface
├── logger.c                — queue-based logger → stdout + logs/system.log (Nur)
└── tasks/
    ├── watchdog.c          — vWatchdogTask + vRecoveryManagerTask (Nur)
    ├── dynamic_tasks.c     — (structural placeholder)
    └── monitor_tasks.c     — (structural placeholder)
```

---

## Inter-Task Communication

| Mechanism | Used for |
|---|---|
| **Queues** (`xCPUDataQueue`, `xMemoryDataQueue`, `xSensorDataQueue`) | Monitors send typed sensor structs each period |
| **Event group** (`xSystemEventGroup`) | Monitors set critical-event bits; watchdog and recovery manager wait on them |
| **Log queue** (`xLogQueue`, depth 32) | Any task calls `log_event()`; `vLoggerTask` drains to stdout + `logs/system.log` |
| **Mutex** (`xPrintMutex`) | Serialises all `printf`/`fprintf` calls across tasks |
| **Heartbeat array** (`g_wd_heartbeat_ms[]`) | Logger, simulator, ground-station write timestamp; watchdog reads and checks gap |

---

## How to Build & Run

### macOS / Linux (GCC + FreeRTOS POSIX port)

```bash
# Clone and fetch FreeRTOS kernel
git clone https://github.com/jeeluuve05/os-pbl-task4-team1-spacecraft-monitoring
cd os-pbl-task4-team1-spacecraft-monitoring
git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS

# Build (links -lpthread -lm)
make

# Run (creates logs/system.log automatically)
make run
# or: ./build/zerogravity
```

### Windows / Visual Studio 2022 (FreeRTOS Win32 port, MSVC)

> **Note:** The project demo runs on Windows using the FreeRTOS Win32 simulator.
> See the full setup steps in the **Makefile header** (the `# Windows / VS2022` section).

Summary:
1. Use `FreeRTOS\Demo\WIN32-MSVC\` as the VS project template.
2. Add all files listed under `APP_SRC` in the Makefile to the VS project.
3. Add `include\` to **Additional Include Directories**.
4. Build and run — `logs\system.log` is created automatically.

`FreeRTOSConfig.h` contains `#ifdef _WIN32` guards that switch the tick rate to 100 Hz and the assert style to halt-without-dialog for MSVC compatibility.

---

## Success Criteria

| Criterion | Target |
|---|---|
| Core monitors run continuously | Uninterrupted over full 110 s scenario |
| All 5 responders spawn on correct trigger | Each critical threshold spawns the right responder |
| Responders self-delete on recovery | Handle set to NULL before `vTaskDelete(NULL)` |
| Watchdog recovers frozen monitor | Task recreated within one 500 ms watchdog period |
| Watchdog kills stuck responder | Responder alive > 10 s without recovery → killed and re-evaluated |
| All events logged to file | Complete `logs/system.log` produced after run |

---

## Course Info

- **Course:** Operating Systems (PBL)
- **Professor:** Edward Youngil Kim
- **University:** Woosong University
- **Semester:** Spring 2026
