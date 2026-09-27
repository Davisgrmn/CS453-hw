CC = gcc
AR = ar
CPPFLAGS += -D_GNU_SOURCE -I. -I../hw1
CFLAGS ?= -std=c11 -g -O2 -Wall -Wextra -Werror -fno-builtin
DEPFLAGS = -MMD -MP
BUILD ?= build
CORE = $(addprefix $(BUILD)/,balloc.o freelist.o bbm.o bm.o utils.o)
TESTS = $(addprefix $(BUILD)/,allocator_test wrapper_test deq_test deq_strings_test)

.PHONY: all test check sanitize valgrind clean
all: $(BUILD)/libballoc.a $(TESTS)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/test_%.o: tests/test_%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# HW1 ignores asprintf's result; keep that source unchanged for step 5.
$(BUILD)/deq.o: ../hw1/deq.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wno-unused-result $(DEPFLAGS) -c $< -o $@

$(BUILD)/deq_main.o: ../hw1/main.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/wrapper_named.o: wrapper.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -Dmalloc=wrapped_malloc \
	  -Dfree=wrapped_free -Drealloc=wrapped_realloc -c $< -o $@

$(BUILD)/libballoc.a: $(CORE)
	$(AR) rcs $@ $^

$(BUILD)/allocator_test: $(BUILD)/test_allocator.o $(BUILD)/libballoc.a
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -Wl,--wrap=mmap,--wrap=munmap $(LDLIBS) -o $@

$(BUILD)/wrapper_test: $(BUILD)/test_wrapper.o $(BUILD)/wrapper_named.o $(BUILD)/libballoc.a
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/deq_test: $(BUILD)/deq_main.o $(BUILD)/deq.o $(BUILD)/wrapper.o $(BUILD)/libballoc.a
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/deq_strings_test: $(BUILD)/test_deq_strings.o $(BUILD)/deq.o $(BUILD)/wrapper.o $(BUILD)/libballoc.a
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test check: all
	$(BUILD)/allocator_test
	$(BUILD)/wrapper_test
	$(BUILD)/deq_test
	$(BUILD)/deq_strings_test

# ASan needs its own malloc, so sanitize the core and renamed wrapper tests.
sanitize:
	$(MAKE) BUILD=build/sanitize CFLAGS='-std=c11 -g -O1 -Wall -Wextra -Werror -fno-builtin -fsanitize=address,undefined -fno-omit-frame-pointer' build/sanitize/allocator_test build/sanitize/wrapper_test
	ASAN_OPTIONS=detect_leaks=1 build/sanitize/allocator_test
	ASAN_OPTIONS=detect_leaks=1 build/sanitize/wrapper_test

# Run queue interposition tests normally: Valgrind replaces malloc itself.
valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 $(BUILD)/allocator_test
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 $(BUILD)/wrapper_test

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
