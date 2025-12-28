.PHONY: all debug release run run_debug run_release clean

# Default to debug
all: debug

# Debug Build
debug:
	cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -Wno-dev
	cmake --build build/debug --config Debug

# Release Build
release:
	cmake -B build/release -DCMAKE_BUILD_TYPE=Release -Wno-dev
	cmake --build build/release --config Release

# Detect OS and set Python command
ifeq ($(OS),Windows_NT)
PYTHON_CMD := python
else
PYTHON_CMD := python3
endif

# Run Debug
run: run_debug

run_debug: debug
	$(PYTHON_CMD) run_game.py debug

# Run Release
run_release: release
	$(PYTHON_CMD) run_game.py release

clean:
	rm -rf build
