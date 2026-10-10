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
TEST_OBJS := $(BUILDDIR)/bigdigits_test.c.o
TEST_OUTPUT := $(BUILDDIR)/bigdigits_test
KEY_TEST_OBJS := $(BUILDDIR)/tdo_keys_test.c.o $(BUILDDIR)/tdo_keys_ndebug.c.o
KEY_TEST_OUTPUT := $(BUILDDIR)/tdo_keys_test
DEPS = $(OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(KEY_TEST_OBJS:.o=.d)


all: $(OUTPUT)

$(OUTPUT): builddir $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(OUTPUT) $(OBJS) $(LDFLAGS)

test: $(OUTPUT) $(TEST_OUTPUT) $(KEY_TEST_OUTPUT)
	UBSAN_OPTIONS="$(UBSAN_OPTIONS):halt_on_error=1" $(TEST_OUTPUT)
	$(PYTHON) tests/signing_test.py $(OUTPUT)
	$(PYTHON) tests/workspace_test.py $(OUTPUT)
	$(PYTHON) tests/tdo_keys_test.py $(KEY_TEST_OUTPUT)

$(TEST_OUTPUT): $(TEST_OBJS) $(BUILDDIR)/bigdigits.c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_OBJS): tests/bigdigits_test.c | builddir
	$(CC) $(CPPFLAGS) $(CFLAGS) -UNDEBUG -Isrc -c $< -o $@

$(KEY_TEST_OUTPUT): $(KEY_TEST_OBJS) $(BUILDDIR)/bigd.c.o $(BUILDDIR)/bigdigits.c.o $(BUILDDIR)/str.c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/tdo_keys_test.c.o: tests/tdo_keys_test.c | builddir
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNDEBUG -Isrc -c $< -o $@

$(BUILDDIR)/tdo_keys_ndebug.c.o: src/tdo_keys.c | builddir
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNDEBUG -Isrc -c $< -o $@

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
	"$(ZIG_VENV)/bin/python" -m pip install "ziglang==0.17.0"
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

install-release:
	@test -n "$(TDO_DEVKIT_PATH)" || { \
		echo "3do-devkit environment not sourced; source /path/to/3do-devkit/activate-env first." >&2; \
		exit 1; \
	}
	$(MAKE) release
	install -Dm755 "build/$(FILENAME)_x86_64-linux-musl" "$(DESTDIR)$(TDO_DEVKIT_PATH)/bin/tools/linux/$(FILENAME)"
	install -Dm755 "build/$(FILENAME)_x86_64-windows-gnu.exe" "$(DESTDIR)$(TDO_DEVKIT_PATH)/bin/tools/win/$(FILENAME).exe"

.PHONY: all test clean builddir release zig-venv strip install-release

-include $(DEPS)
