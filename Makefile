# Build the Cool Retro Terminal clone without requiring CMake

CXX ?= g++
PKG_CONFIG ?= pkg-config
BUILD_DIR ?= build
OBJ_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/cool_retro_terminal

SRCS := \
    src/main.cpp \
    src/terminal.cpp \
    src/ansi_parser.cpp \
    src/crt_effect.cpp \
    src/font_atlas.cpp

OBJS := $(SRCS:src/%.cpp=$(OBJ_DIR)/%.o)

# Validate toolchain availability up-front to fail fast with clear guidance
ifeq ($(shell command -v $(PKG_CONFIG) >/dev/null 2>&1 && echo yes),yes)
else
$(error pkg-config not found. Please install pkg-config to build this project.)
endif

SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL2_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
SDL2TTF_CFLAGS := $(shell $(PKG_CONFIG) --cflags SDL2_ttf 2>/dev/null)
SDL2TTF_LIBS := $(shell $(PKG_CONFIG) --libs SDL2_ttf 2>/dev/null)

ifeq ($(SDL2_CFLAGS),)
$(error SDL2 development files not found. Install libsdl2-dev or the equivalent package.)
endif
ifeq ($(SDL2TTF_CFLAGS),)
$(error SDL2_ttf development files not found. Install libsdl2-ttf-dev or the equivalent package.)
endif

CPPFLAGS ?=
CPPFLAGS += -Iinclude $(SDL2_CFLAGS) $(SDL2TTF_CFLAGS)
CXXFLAGS ?=
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS ?=
LDLIBS ?=
LDLIBS += $(SDL2_LIBS) $(SDL2TTF_LIBS) -lutil

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(OBJ_DIR): | $(BUILD_DIR)
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	$(TARGET)

-include $(OBJS:.o=.d)
