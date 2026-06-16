#!/usr/bin/env python3
"""
ZeroGravity Dashboard — Live Bridge Server
==========================================
Serves the dashboard HTML and tails ../logs/system.log so the browser shows
real FreeRTOS telemetry instead of simulated data.

Run from the dashboard/ folder:
    python serve.py
Then open http://localhost:8765/ in your browser.

Endpoints:
  /                  → serves index.html (the dashboard)
  /api/state         → JSON: current parsed sensor values + recent log lines
  /api/log/raw       → text: last 200 raw log lines (for debugging)

Stop with Ctrl+C.
"""

import http.server
import json
import re
import socketserver
import sys
import threading
import time
from collections import deque
from pathlib import Path

PORT = 8765
HERE = Path(__file__).resolve().parent
LOG_FILE = HERE.parent / "logs" / "system.log"

# Ring buffers for streaming state to the browser
log_lines = deque(maxlen=2000)      # raw recent log lines
parsed_events = deque(maxlen=500)   # structured event entries
state = {
    "cpu": 32.0,
    "temp": 28.0,
    "battery": 3.92,
    "memory": 78.0,
    "comms": 0.3,
    "phase": "NOMINAL",
    "tasks": {            # keyed by name used in index.html data-task / data-resp
        "CPUMon":    {"state": "IDLE"}, "MemMon":     {"state": "IDLE"},
        "SensorMon": {"state": "IDLE"}, "SimTask":    {"state": "IDLE"},
        "GroundSta": {"state": "IDLE"}, "Logger":     {"state": "IDLE"},
        "Thermal":   {"state": "IDLE"}, "Battery":    {"state": "IDLE"},
        "Comms":     {"state": "IDLE"}, "LowMemory":  {"state": "IDLE"},
    },
    "uptime_seconds": 0,
    "updated_at": 0,
}
state_lock = threading.Lock()

# ====================================================================
# LOG PARSER — tuned to ZeroGravity logger.c output format
# ====================================================================

# Maps FreeRTOS phase names to dashboard phase keys
# Maps keywords in "RESPONDER CREATED" log lines → dashboard responder key
RESPONDER_SPAWN_MAP = {
    "THERMAL": "Thermal",
    "BATTERY": "Battery",
    "COMM":    "Comms",
    "MEMORY":  "LowMemory",
}

# Maps FreeRTOS task name strings in WATCHDOG lines → dashboard responder key
RESPONDER_KILL_MAP = {
    "thermal_responder":      "Thermal",
    "battery_responder":      "Battery",
    "comm_timeout_responder": "Comms",
    "low_memory_responder":   "LowMemory",
}

# Authoritative responder states per phase — applied after every state update
PHASE_RESPONDER_MAP = {
    "THERMAL":  {"Thermal": "ALIVE", "Battery": "IDLE",  "Comms": "IDLE",  "LowMemory": "IDLE"},
    "BATTERY":  {"Thermal": "IDLE",  "Battery": "ALIVE", "Comms": "IDLE",  "LowMemory": "IDLE"},
    "COMMS":    {"Thermal": "IDLE",  "Battery": "IDLE",  "Comms": "ALIVE", "LowMemory": "IDLE"},
    "MEMORY":   {"Thermal": "IDLE",  "Battery": "IDLE",  "Comms": "IDLE",  "LowMemory": "ALIVE"},
    "COMBINED": {"Thermal": "IDLE",  "Battery": "ALIVE", "Comms": "IDLE",  "LowMemory": "ALIVE"},
    "NOMINAL":  {"Thermal": "IDLE",  "Battery": "IDLE",  "Comms": "IDLE",  "LowMemory": "IDLE"},
    "RECOVERY": {"Thermal": "IDLE",  "Battery": "IDLE",  "Comms": "IDLE",  "LowMemory": "IDLE"},
}

# L1 monitors + L3 safety tasks — always ALIVE while FreeRTOS is running
L1_L3_TASKS = ["CPUMon", "MemMon", "SensorMon", "SimTask", "GroundSta", "Logger"]

PHASE_MAP = {
    "THERMAL_SPIKE":  "THERMAL",
    "BATTERY_FAULT":  "BATTERY",
    "COMM_BLACKOUT":  "COMMS",
    "MEMORY_LEAK":    "MEMORY",
    "COMBINED_FAULT": "COMBINED",
    "NOMINAL":        "NOMINAL",
    "RECOVERY":       "RECOVERY",
}

PATTERNS = {
    # [T=045s] [CPU:23.4% OK] [HEAP:67% OK] [TEMP:28.1°C OK] [BAT:3.99V OK] [COMM:0.5s OK]
    "sensor": re.compile(
        r"\[T=(?P<uptime>\d+)s\]"
        r".*?\[CPU:(?P<cpu>[\d.]+)%"
        r".*?\[HEAP:(?P<memory>\d+)%"
        r".*?\[TEMP:(?P<temp>[-\d.]+)"
        r".*?\[BAT:(?P<battery>[\d.]+)V"
        r".*?\[COMM:(?P<comms>[\d.]+)s"
    ),
    # [SIM] Phase: NOMINAL -> THERMAL_SPIKE (t=15s)
    "phase": re.compile(
        r"\[SIM\]\s+Phase:\s+\S+\s+->\s+(?P<phase>\w+)",
        re.IGNORECASE,
    ),
    # [T=NNNs] prefix strip for event lines
    "event_prefix": re.compile(r"^\[T=\d+s\]\s+(?P<msg>.+)$"),
}


def parse_line(line: str) -> dict | None:
    """Parse a ZeroGravity log line. Returns a typed dict or None."""
    line = line.strip()
    if not line:
        return None

    out = {"raw": line, "ts": time.time()}

    # 1. Sensor status line — highest priority, most common
    m = PATTERNS["sensor"].search(line)
    if m:
        out["type"]    = "sensor"
        out["uptime"]  = int(m.group("uptime"))
        out["cpu"]     = float(m.group("cpu"))
        out["memory"]  = float(m.group("memory"))
        out["temp"]    = float(m.group("temp"))
        out["battery"] = float(m.group("battery"))
        out["comms"]   = float(m.group("comms"))
        return out

    # 2. Phase transition line
    m = PATTERNS["phase"].search(line)
    if m:
        raw_phase    = m.group("phase")
        out["type"]  = "phase"
        out["phase"] = PHASE_MAP.get(raw_phase, raw_phase)
        return out

    # 3. Event line — strip [T=NNNs] prefix then classify by content
    m = PATTERNS["event_prefix"].match(line)
    raw_msg = m.group("msg") if m else line

    # Clean leading/trailing decorations (!!! and ***)
    msg = re.sub(r'^[!\*\s]+', '', raw_msg).strip()
    msg = re.sub(r'[!\*\s]+$', '', msg).strip()

    line_lower = line.lower()
    if msg.startswith("WATCHDOG:") or "!!! WATCHDOG" in line:
        tag = "WDG"
    elif "[recoverymgr]" in line_lower or "***" in raw_msg:
        tag = "MGR"
    else:
        tag = "LOG"

    out["type"] = "event"
    out["tag"]  = tag
    out["msg"]  = msg
    return out


def _apply_phase_responder_mapping():
    """Set responder task states from current phase. Must be called inside state_lock."""
    mapping = PHASE_RESPONDER_MAP.get(state["phase"], PHASE_RESPONDER_MAP["NOMINAL"])
    for key, task_state in mapping.items():
        state["tasks"][key]["state"] = task_state


def apply_to_state(parsed: dict):
    """Update the global state dict from a parsed log entry."""
    if not parsed:
        return
    with state_lock:
        t = parsed.get("type")
        if t == "sensor":
            state["cpu"]            = parsed["cpu"]
            state["temp"]           = parsed["temp"]
            state["battery"]        = parsed["battery"]
            state["memory"]         = parsed["memory"]
            state["comms"]          = parsed["comms"]
            state["uptime_seconds"] = parsed["uptime"]
            # Infer phase from uptime using the FreeRTOS fault timeline
            u = parsed["uptime"]
            if   u < 15:  state["phase"] = "NOMINAL"
            elif u < 35:  state["phase"] = "THERMAL"
            elif u < 55:  state["phase"] = "BATTERY"
            elif u < 70:  state["phase"] = "COMMS"
            elif u < 90:  state["phase"] = "MEMORY"
            elif u < 110: state["phase"] = "COMBINED"
            else:         state["phase"] = "RECOVERY"
            # L1 monitors + L3 safety tasks are always ALIVE once running
            for name in L1_L3_TASKS:
                state["tasks"][name]["state"] = "ALIVE"
        elif t == "phase":
            state["phase"] = parsed["phase"]
        elif t == "event":
            raw = parsed.get("raw", "")
            msg = parsed.get("msg", "")
            # Responder spawn: "*** CRITICAL: ... THERMAL RESPONDER CREATED ..."
            if "RESPONDER CREATED" in raw:
                for keyword, key in RESPONDER_SPAWN_MAP.items():
                    if keyword in raw.upper():
                        state["tasks"][key]["state"] = "ALIVE"
                        break
            # Responder kill: "!!! WATCHDOG: task 'thermal_responder' unresponsive"
            elif "unresponsive" in raw:
                for task_name, key in RESPONDER_KILL_MAP.items():
                    if task_name in raw:
                        state["tasks"][key]["state"] = "IDLE"
                        break
        # Phase mapping is authoritative — overrides any stale spawn/kill state
        _apply_phase_responder_mapping()
        state["updated_at"] = time.time()


# ====================================================================
# LOG TAILER — runs in a background thread
# ====================================================================
def tail_log():
    """Follow logs/system.log like `tail -f`. Re-opens if file is rotated/recreated."""
    print(f"[bridge] Tailing {LOG_FILE}", flush=True)
    while True:
        try:
            if not LOG_FILE.exists():
                time.sleep(0.5)
                continue
            with LOG_FILE.open("r", encoding="utf-8", errors="replace") as f:
                # Read from start to seed initial state from existing log,
                # then follow new lines as they arrive
                f.seek(0)
                while True:
                    line = f.readline()
                    if not line:
                        time.sleep(0.1)
                        continue
                    log_lines.append(line.rstrip())
                    parsed = parse_line(line)
                    if parsed:
                        parsed_events.append(parsed)
                        apply_to_state(parsed)
        except Exception as e:
            print(f"[bridge] tail error: {e}", flush=True)
            time.sleep(1)


# ====================================================================
# HTTP HANDLER
# ====================================================================
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return  # silence noisy access logs

    def _send_json(self, payload, status=200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.startswith("/api/state"):
            with state_lock:
                snap = dict(state)
                snap["tasks"] = {k: dict(v) for k, v in state["tasks"].items()}
            snap["recent_events"] = list(parsed_events)[-60:]
            return self._send_json(snap)

        if self.path.startswith("/api/log/raw"):
            tail = "\n".join(list(log_lines)[-200:])
            body = tail.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
            return

        # Fall through to static file serving (index.html, etc.)
        if self.path == "/":
            self.path = "/index.html"
        return super().do_GET()


def main():
    if not LOG_FILE.parent.exists():
        print(f"[bridge] WARNING: {LOG_FILE.parent} doesn't exist. "
              f"Run FreeRTOS first to create logs/system.log.", flush=True)

    threading.Thread(target=tail_log, daemon=True).start()

    import os
    os.chdir(HERE)

    with socketserver.ThreadingTCPServer(("0.0.0.0", PORT), Handler) as httpd:
        url = f"http://localhost:{PORT}/"
        print(f"[bridge] ZeroGravity dashboard live at {url}", flush=True)
        print(f"[bridge] Tailing: {LOG_FILE}", flush=True)
        print(f"[bridge] Press Ctrl+C to stop.", flush=True)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[bridge] shutting down.", flush=True)


if __name__ == "__main__":
    main()
