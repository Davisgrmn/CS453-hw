prog=shell

# Use system Readline by default, or the active Conda environment when present.
# Override READLINE_PREFIX for another installed copy (or set it empty).
READLINE_PREFIX ?= $(CONDA_PREFIX)
ifneq ($(strip $(READLINE_PREFIX)),)
ccflags += -I$(READLINE_PREFIX)/include
ldflags += -L$(READLINE_PREFIX)/lib -Wl,-rpath,$(READLINE_PREFIX)/lib
endif
ldflags += -lreadline -lncurses

include ../GNUmakefile

.PHONY: test test-processes test-valgrind

test: $(prog)
	Test/run $(prog)

test-processes: $(prog)
	python3 Test/process_checks.py

test-valgrind: $(prog)
	TEST_TIMEOUT=30 Test/run Test/valgrind-shell
