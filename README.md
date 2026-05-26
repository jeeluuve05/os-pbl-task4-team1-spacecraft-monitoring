# os-pbl-task4-team1-spacecraft-monitoring
Team ZeroGravity | OS PBL Week 9 Task 4 | Woosong University 2026
FreeRTOS-based dynamic task management system that simulates spacecraft health monitoring. The system monitors CPU, memory, battery, temperature, and communication — dynamically creating and terminating response tasks based on real-time conditions.
Team Members
NameTechnical RoleSub-RoleKhurshidLeader / FreeRTOS System Architect—SaidaTask & Scheduler DesignerCo-LeaderShokhrukhSimulation & Testing EngineerOrganization of Project ProgressNurLogging & Watchdog DeveloperPresentation MakerPakhe JongDocumentation & AnalysisMeeting Notes / Report WriterJinhyonsoDocumentation & AnalysisMeeting Notes / Report Writer

System Architecture
3-Layer Design
Layer 3 — System Safety (Priority 8-10)
    Watchdog Task | Logger Task | Recovery Manager

Layer 2 — Condition-Triggered Responders (Priority 7)
    LowMemory | Thermal | Battery | CommTimeout
    (created dynamically, deleted when signal recovers)

Layer 1 — Always-On Core Monitors (Priority 3)
    CPU Monitor | Memory Monitor | Sensor Monitor
Monitored Signals & Thresholds
SignalNormalWarningCritical (spawn responder)CPU Load< 60%60–80%> 80%Heap Free> 40%20–40%< 20%Stack Free> 30%15–30%< 15%Battery> 3.6V3.3–3.6V< 3.3VTemperature0–60°C60–80°C> 80°CComm Heartbeat< 1s gap1–3s gap> 3s no reply

Project Structure
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

Task Priority Table
TaskPriorityPeriodTypeWatchdogTask10500msAlways-onLoggerTask9event-drivenAlways-onRecoveryManager8event-drivenAlways-onAll Responders7dynamicSpawned on triggerCPUMonitor31000msAlways-onMemoryMonitor31000msAlways-onSensorMonitor32000msAlways-on

Success Criteria
CriterionTargetCore monitors run continuously100% uptime over 1-hour testAll 4 trigger scenarios fire correctlyEach spawns the right responderWatchdog recovers from frozen taskRecovery within 2 secondsNo memory leaks after 1-hour runHeap usage stableAll scenarios loggedComplete event log produced

How to Build & Run

Requires FreeRTOS Win32/POSIX simulator

bash# Clone the repo
git clone https://github.com/jeeluuve05/os-pbl-task4-team1-spacecraft-monitoring
cd os-pbl-task4-team1-spacecraft-monitoring

# Build
make

# Run
./build/zerogravity

Inter-Task Communication

Queues — monitors send sensor data to responders
Event Groups — monitors signal the safety layer on critical events
Semaphores — mutual exclusion where needed


Course Info

Course: Operating Systems (PBL)
Professor: Edward Youngil Kim
University: Woosong University
Semester: Spring 2026
