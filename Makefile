.PHONY: all debug release run run_debug run_release clean

# Default to debug
all: debug

# Debug Build
debug:
	cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -Wno-dev
	cmake --build build/debug

# Release Build
release:
	cmake -B build/release -DCMAKE_BUILD_TYPE=Release -Wno-dev
	cmake --build build/release

# Run Debug
run: run_debug

run_debug: debug
	./build/debug/lithos

# Run Release
run_release: release
	./build/release/minceraft

clean:
	rm -rf build build_release
