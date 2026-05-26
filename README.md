# Spacecraft Onboard System Monitoring
**Team ZeroGravity** | OS PBL Week 9 Task 4 | Woosong University 2026

FreeRTOS-based dynamic task management system that simulates spacecraft health monitoring. The system monitors CPU, memory, battery, temperature, and communication — dynamically creating and terminating response tasks based on real-time conditions.

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
Layer 3 — System Safety (Priority 8-10)
    Watchdog Task | Logger Task | Recovery Manager

Layer 2 — Condition-Triggered Responders (Priority 7)
    LowMemory | Thermal | Battery | CommTimeout
    (created dynamically, deleted when signal recovers)

Layer 1 — Always-On Core Monitors (Priority 3)
    CPU Monitor | Memory Monitor | Sensor Monitor
```

### Monitored Signals & Thresholds

| Signal | Normal | Warning | Critical (spawn responder) |
|---|---|---|---|
| CPU Load | < 60% | 60–80% | > 80% |
| Heap Free | > 40% | 20–40% | < 20% |
| Stack Free | > 30% | 15–30% | < 15% |
| Battery | > 3.6V | 3.3–3.6V | < 3.3V |
| Temperature | 0–60°C | 60–80°C | > 80°C |
| Comm Heartbeat | < 1s gap | 1–3s gap | > 3s no reply |

---

## Project Structure

```
ZeroGravity/
├── include/
│   └── tasks/
│       ├── task_scheduler.h   # priorities, handles, queues, event groups
│       ├── monitors.h         # core monitor task declarations
│       ├── responders.h       # dynamic responder task declarations
│       └── simulation.h       # simulated sensor interface
├── src/
│   ├── main.c                 # entry point, FreeRTOS scheduler start
│   ├── task_scheduler.c       # scheduler init, spawn/kill helpers
│   ├── monitors.c             # CPU, Memory, Sensor monitor tasks
│   ├── responders.c           # 4 dynamic responder tasks
│   ├── simulation.c           # simulated sensor values (Shokhrukh)
│   ├── watchdog.c             # watchdog & recovery (Nur)
│   └── logger.c               # event logger (Nur)
├── Makefile
└── README.md
```

---

## Task Priority Table

| Task | Priority | Period | Type |
|---|---|---|---|
| WatchdogTask | 10 | 500ms | Always-on |
| LoggerTask | 9 | event-driven | Always-on |
| RecoveryManager | 8 | event-driven | Always-on |
| All Responders | 7 | dynamic | Spawned on trigger |
| CPUMonitor | 3 | 1000ms | Always-on |
| MemoryMonitor | 3 | 1000ms | Always-on |
| SensorMonitor | 3 | 2000ms | Always-on |

---

## Success Criteria

| Criterion | Target |
|---|---|
| Core monitors run continuously | 100% uptime over 1-hour test |
| All 4 trigger scenarios fire correctly | Each spawns the right responder |
| Watchdog recovers from frozen task | Recovery within 2 seconds |
| No memory leaks after 1-hour run | Heap usage stable |
| All scenarios logged | Complete event log produced |

---

## How to Build & Run

> Requires FreeRTOS Win32/POSIX simulator

```bash
# Clone the repo
git clone https://github.com/jeeluuve05/os-pbl-task4-team1-spacecraft-monitoring
cd os-pbl-task4-team1-spacecraft-monitoring

# Build
make

# Run
./build/zerogravity
```

---

## Inter-Task Communication

- **Queues** — monitors send sensor data to responders
- **Event Groups** — monitors signal the safety layer on critical events
- **Semaphores** — mutual exclusion where needed

---

## Course Info

- **Course:** Operating Systems (PBL)
- **Professor:** Edward Youngil Kim
- **University:** Woosong University
- **Semester:** Spring 2026
