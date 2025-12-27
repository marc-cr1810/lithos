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

# Run Debug
run: run_debug

run_debug: debug
	python3 run_game.py debug

# Run Release
run_release: release
	python3 run_game.py release

clean:
	rm -rf build build_release
