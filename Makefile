.PHONY: all configure build run clean

all: build

configure:
	cmake -B build

build:
	cmake -B build
	cmake --build build

run: build
	./build/minceraft

clean:
	rm -rf build
