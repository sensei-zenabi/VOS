# Simple Makefile wrapper around the CMake build for convenience

BUILD_DIR ?= build
CONFIGURE_FLAGS ?=

.PHONY: all configure build clean run

all: build

configure:
	cmake -S . -B $(BUILD_DIR) $(CONFIGURE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: build
	$(BUILD_DIR)/cool_retro_terminal
