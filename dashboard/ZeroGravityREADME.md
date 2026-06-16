# ZeroGravity Mission Control Dashboard

NASA Open MCT–styled live dashboard for the FreeRTOS 3-layer dynamic task
management spacecraft monitoring system.

## What this folder contains

| File          | Purpose                                                          |
|---------------|------------------------------------------------------------------|
| `index.html`  | The dashboard UI (self-contained — gauges, chart, Mars, log)     |
| `serve.py`    | Tiny Python bridge: serves the dashboard + tails `system.log`    |
| `README.md`   | This file                                                        |

## Running it

The dashboard needs to be served over HTTP (not opened as `file://`) so it
can fetch live telemetry from the bridge server.

```bash
# 1. Make sure FreeRTOS is running so it's writing to ../logs/system.log
#    (in another terminal, from the project root):
./build/zerogravity_sim         # or whatever your build produces

# 2. Start the dashboard bridge:
cd dashboard/
python serve.py

# 3. Open in browser:
#    http://localhost:8765/
```

## How the integration works

```
┌─────────────────┐      writes to       ┌──────────────────┐
│  FreeRTOS sim   │ ───────────────────► │  logs/system.log │
│  (your C code)  │                       └──────────┬───────┘
└─────────────────┘                                  │ tails
                                                     ▼
                                          ┌──────────────────┐
                                          │   serve.py       │
                                          │   - parses log   │
                                          │   - HTTP server  │
                                          └─────┬─────┬──────┘
                                                │     │
                              /api/state (JSON) │     │ /  (static)
                                                ▼     ▼
                                          ┌──────────────────┐
                                          │  index.html      │
                                          │  (browser)       │
                                          │  polls every 1s  │
                                          └──────────────────┘
```

## Log format

The parser in `serve.py` tries to match a few common formats. If your
`logger.c` writes lines in a different format, edit the `PATTERNS` regexes
near the top of `serve.py`.

Expected sensor line format (flexible — keys can be in any order):
```
[14:23:45.123] MON cpu=32 temp=28 batt=3.92 mem=78 comms=0.3
```

Expected event line format:
```
[14:23:50] MGR Thermal fault detected (45C > 40C threshold)
[14:23:51] WDG task=ThermalMonitor state=ALIVE beat=0.5
```

## Demo mode (no FreeRTOS running)

If `../logs/system.log` doesn't exist or is empty, the dashboard falls back
to its built-in simulated phase cycle (NOMINAL → THERMAL → BATTERY → COMMS
→ MEMORY → COMBINED → RECOVERY). Useful for testing the UI standalone.
