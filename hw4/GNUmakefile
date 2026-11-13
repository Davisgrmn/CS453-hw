CC = gcc
CXX = g++
CPPFLAGS += -D_GNU_SOURCE -I.
CFLAGS ?= -std=c11 -g -O2 -Wall -Wextra -Werror
CXXFLAGS ?= -std=c++11 -g -O2 -Wall -Wextra -Werror
CFLAGS += -pthread
CXXFLAGS += -pthread
BUILD ?= build
FLTK_CONFIG ?= $(shell command -v fltk-config 2>/dev/null)
LOCAL_FLTK ?= $(HOME)/.local/opt/cs453-hw4-fltk

# Prefer the system installation (including onyx). A user-local extraction
# lets this Ubuntu machine build without sudo or temporary-directory paths.
ifneq ($(strip $(FLTK_CONFIG)),)
FLTK_CXXFLAGS ?= $(shell $(FLTK_CONFIG) --cxxflags)
FLTK_LIBS ?= $(shell $(FLTK_CONFIG) --ldflags)
else ifneq ($(wildcard $(LOCAL_FLTK)/usr/include/FL/Fl.H),)
LOCAL_FLTK_LIBRARY := $(firstword $(wildcard $(LOCAL_FLTK)/usr/lib/*/libfltk.so $(LOCAL_FLTK)/usr/lib/libfltk.so))
LOCAL_FLTK_LIBDIR := $(patsubst %/,%,$(dir $(LOCAL_FLTK_LIBRARY)))
FLTK_CXXFLAGS ?= -isystem $(LOCAL_FLTK)/usr/include
FLTK_LIBS ?= -L$(LOCAL_FLTK_LIBDIR) -Wl,-rpath,$(LOCAL_FLTK_LIBDIR) -lfltk
else
FLTK_CXXFLAGS ?=
FLTK_LIBS ?= -lfltk -lX11
endif

QUEUE = $(BUILD)/deq.o $(BUILD)/mtq.o $(BUILD)/threads.o
APP = $(QUEUE) $(BUILD)/main.o $(BUILD)/mole.o $(BUILD)/lawn.o $(BUILD)/lawnimp.o
TESTS = $(BUILD)/test_deq $(BUILD)/test_mtq
VG = valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect,possible --error-exitcode=1

.PHONY: all test unit test-simulation run valgrind racecheck sanitize clean
all: wam

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Preserve HW1 exactly, including its unchecked asprintf return value.
$(BUILD)/deq.o: deq.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-result -MMD -MP -c $< -o $@

$(BUILD)/lawnimp.o: lawnimp.cc | $(BUILD)
	@printf '#include <FL/Fl.H>\n' | $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FLTK_CXXFLAGS) -x c++ -E - >/dev/null 2>&1 || { \
	  printf '%s\n' 'FLTK development headers were not found.' \
	    'Ubuntu/Debian: sudo apt install libfltk1.3-dev libx11-dev' \
	    'For a custom installation, set FLTK_CXXFLAGS and FLTK_LIBS (see README.md).' >&2; \
	  exit 1; }
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(FLTK_CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/test_%.o: tests/test_%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

wam: $(APP)
	$(CXX) $(LDFLAGS) -o $@ $^ $(FLTK_LIBS) -pthread $(LDLIBS)

$(BUILD)/test_deq: $(BUILD)/test_deq.o $(BUILD)/deq.o
	$(CC) $(LDFLAGS) -o $@ $^ -pthread $(LDLIBS)

$(BUILD)/test_mtq: $(BUILD)/test_mtq.o $(QUEUE)
	$(CC) $(LDFLAGS) -o $@ $^ -pthread $(LDLIBS)

unit: $(TESTS)
	$(BUILD)/test_deq
	$(BUILD)/test_mtq

test-simulation: wam
	python3 tests/test_simulation.py ./wam

test: unit test-simulation

run: wam
	./wam $(args)

valgrind: $(TESTS) wam
	$(VG) $(BUILD)/test_deq
	$(VG) $(BUILD)/test_mtq
	DISPLAY= WAM_TICK_MS=1 $(VG) --suppressions=fltk.supp ./wam 30 1

racecheck: $(BUILD)/test_mtq wam
	valgrind --tool=drd --error-exitcode=1 $(BUILD)/test_mtq
	DISPLAY= WAM_TICK_MS=1 valgrind --tool=drd --error-exitcode=1 ./wam 30 1

sanitize:
	$(MAKE) BUILD=build/sanitize CFLAGS='-std=c11 -g -O1 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' build/sanitize/test_deq build/sanitize/test_mtq
	ASAN_OPTIONS=detect_leaks=1 build/sanitize/test_deq
	ASAN_OPTIONS=detect_leaks=1 build/sanitize/test_mtq

clean:
	rm -rf build wam *.o *.d *.i try

-include $(wildcard $(BUILD)/*.d)
