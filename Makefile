# ==============================================================
# Makefile — ZeroGravity Spacecraft Monitor
#
# ── macOS / Linux (this file) ──────────────────────────────
# Platform : FreeRTOS POSIX Simulator, GCC
# Build    : gcc -lpthread -lm
#
# Usage:
#   make          — build ./build/zerogravity
#   make run      — build + run
#   make clean    — remove build artefacts
#
# Requires FreeRTOS-Kernel cloned into ./FreeRTOS/
#   git clone --depth 1 \
#     https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS
#
# ── Windows / VS2022 (demo machine) ────────────────────────
# Platform : FreeRTOS Win32 Simulator port, MSVC
# Do NOT use this Makefile on Windows — use Visual Studio.
#
# Setup steps for VS2022:
#   1. Open / create a Win32 FreeRTOS demo project in VS2022.
#      (Start from FreeRTOS\Demo\WIN32-MSVC\ as a template.)
#   2. Add the following source files to the VS project:
#        src\main.c
#        src\task_scheduler.c
#        src\monitors.c
#        src\responders.c
#        src\simulation.c
#        src\sensor_simulator.c
#        src\logger.c
#        src\tasks\watchdog.c
#   3. Add include\ to Additional Include Directories
#      (Project → Properties → C/C++ → General).
#   4. The Win32 port's own FreeRTOSConfig.h is compatible with
#      ours; use whichever the VS template already provides.
#      (Our include\FreeRTOSConfig.h has #ifdef _WIN32 guards.)
#   5. Build → run.  logs\system.log is created automatically.
# ==============================================================

CC      := gcc
TARGET  := build/zerogravity
CFLAGS  := -Wall -Wextra -g -O0 \
            -D_REENTRANT \
            -DprojCOVERAGE_TEST=0

# ---- Directories ------------------------------------------------
FREERTOS_DIR   := FreeRTOS
FREERTOS_PORT  := $(FREERTOS_DIR)/portable/ThirdParty/GCC/Posix
FREERTOS_MEMMG := $(FREERTOS_DIR)/portable/MemMang

INC := -I include \
       -I $(FREERTOS_DIR)/include \
       -I $(FREERTOS_PORT) \
       -I $(FREERTOS_PORT)/utils

# ---- FreeRTOS kernel sources ------------------------------------
FREERTOS_SRC := \
    $(FREERTOS_DIR)/tasks.c         \
    $(FREERTOS_DIR)/queue.c         \
    $(FREERTOS_DIR)/list.c          \
    $(FREERTOS_DIR)/event_groups.c  \
    $(FREERTOS_DIR)/timers.c        \
    $(FREERTOS_PORT)/port.c         \
    $(FREERTOS_PORT)/utils/wait_for_event.c \
    $(FREERTOS_MEMMG)/heap_4.c

# ---- Project sources --------------------------------------------
APP_SRC := \
    src/main.c               \
    src/task_scheduler.c     \
    src/monitors.c           \
    src/responders.c         \
    src/simulation.c         \
    src/sensor_simulator.c   \
    src/logger.c             \
    src/tasks/watchdog.c

ALL_SRC := $(FREERTOS_SRC) $(APP_SRC)

# ---- Object files -----------------------------------------------
OBJ_DIR := build/obj
OBJS    := $(patsubst %.c, $(OBJ_DIR)/%.o, $(ALL_SRC))

# ==============================================================
# Targets
# ==============================================================

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INC) $^ -o $@ -lpthread -lm
	@echo ""
	@echo "  Build OK  ->  $(TARGET)"
	@echo ""

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

run: all
	@mkdir -p logs
	./$(TARGET)

clean:
	rm -rf build logs/system.log
