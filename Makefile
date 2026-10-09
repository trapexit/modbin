FILENAME := modbin

ifdef TARGET
  EXE := $(FILENAME)_$(TARGET)
else
  EXE := $(FILENAME)
endif

JOBS := $(shell nproc)

OUTPUT = build/$(EXE)

.DEFAULT_GOAL := all

CC    ?= gcc
CXX   ?= g++
STRIP ?= strip
PYTHON ?= python3
ZIG_VENV ?= .venv
SYSTEM_ZIG := $(shell command -v zig 2>/dev/null)
ZIG ?= $(if $(SYSTEM_ZIG),$(SYSTEM_ZIG),$(abspath $(ZIG_VENV))/bin/python-zig)

ifeq ($(DEBUG),1)
OPT := -O0 -ggdb
else
OPT := -Os -flto -static
ifneq ($(TARGET),)
  ifneq ($(filter %macos,$(TARGET)),)
    LDFLAGS += -Wl,-dead_strip -Wl,-S -Wl,-x
  else
    LDFLAGS += -Wl,--gc-sections -Wl,--strip-all
  endif
endif
endif

ifeq ($(SANITIZE),1)
OPT += -fsanitize=undefined
endif

CFLAGS = $(OPT) -Wall
CXXFLAGS = $(OPT) -Wall -std=c++17
CPPFLAGS ?= -MMD -MP

SRCS_C   := $(wildcard src/*.c)
SRCS_CXX := $(wildcard src/*.cpp)

ifdef TARGET
  BUILDDIR = build/$(TARGET)
else
  BUILDDIR = build
endif
OBJS := $(SRCS_C:src/%.c=$(BUILDDIR)/%.c.o)
OBJS += $(SRCS_CXX:src/%.cpp=$(BUILDDIR)/%.cpp.o)
DEPS  = $(OBJS:.o=.d)


all: $(OUTPUT)

$(OUTPUT): builddir $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJS) $(LDFLAGS)

strip: $(OUTPUT)
	$(STRIP) --strip-all $(OUTPUT)

$(BUILDDIR)/%.c.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.cpp.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rfv build/

builddir:
	mkdir -p $(BUILDDIR)

zig-venv:
ifneq ($(SYSTEM_ZIG),)
	@echo "Using system Zig: $(SYSTEM_ZIG)"
else
	$(PYTHON) -m venv "$(ZIG_VENV)"
	"$(ZIG_VENV)/bin/python" -m pip install "ziglang==0.16.0"
endif

release:
	@"$(ZIG)" version >/dev/null 2>&1 || { \
		echo "Zig not found; run 'make zig-venv' first." >&2; \
		exit 1; \
	}
	$(MAKE) clean
	$(MAKE) DEBUG=0 -j$(JOBS) \
		CC="$(ZIG) cc -target x86_64-linux-musl" \
		CXX="$(ZIG) c++ -target x86_64-linux-musl" \
		STRIP="$(ZIG) llvm-strip" \
		TARGET="x86_64-linux-musl" \
		OPT="-Oz -flto -ffunction-sections -fdata-sections -static"
	$(MAKE) DEBUG=0 -j$(JOBS) \
		CC="$(ZIG) cc -target aarch64-linux-musl" \
		CXX="$(ZIG) c++ -target aarch64-linux-musl" \
		STRIP="$(ZIG) llvm-strip" \
		TARGET="aarch64-linux-musl" \
		OPT="-Oz -flto -ffunction-sections -fdata-sections -static"
	$(MAKE) DEBUG=0 -j$(JOBS) \
		CC="$(ZIG) cc -target x86_64-windows-gnu" \
		CXX="$(ZIG) c++ -target x86_64-windows-gnu" \
		STRIP="$(ZIG) llvm-strip" \
		TARGET="x86_64-windows-gnu.exe" \
		OPT="-Oz -ffunction-sections -fdata-sections -static"
	$(MAKE) DEBUG=0 -j$(JOBS) \
		CC="$(ZIG) cc -target aarch64-macos" \
		CXX="$(ZIG) c++ -target aarch64-macos" \
		STRIP="$(ZIG) llvm-strip" \
		TARGET="aarch64-macos" \
		OPT="-Oz -ffunction-sections -fdata-sections"

.PHONY: all clean builddir release zig-venv strip

-include $(DEPS)
