CC      ?= gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -I src

TESTS   := test/test_midi2_msg \
           test/test_midi2_proc \
           test/test_midi2_ci \
           test/test_midi2_conv \
           test/test_midi2_dispatch \
           test/test_midi2_ci_msg \
           test/test_midi2_ci_dispatch \
           test/test_midi2_amalgam

.PHONY: all test clean

all: test

test: $(TESTS)
	@echo ""
	@for t in $(TESTS); do \
		echo "--- $$t ---"; \
		./$$t || exit 1; \
	done
	@echo ""
	@echo "All tests passed."

test/test_midi2_msg: test/test_midi2_msg.c src/midi2_msg.h
	$(CC) $(CFLAGS) -o $@ $<

test/test_midi2_proc: test/test_midi2_proc.c src/midi2_proc.c src/midi2_proc.h src/midi2_msg.h
	$(CC) $(CFLAGS) -o $@ test/test_midi2_proc.c src/midi2_proc.c

test/test_midi2_ci: test/test_midi2_ci.c src/midi2_ci.c src/midi2_ci.h src/midi2_proc.c src/midi2_proc.h src/midi2_msg.h
	$(CC) $(CFLAGS) -o $@ test/test_midi2_ci.c src/midi2_proc.c src/midi2_ci.c

test/test_midi2_conv: test/test_midi2_conv.c src/midi2_conv.c src/midi2_conv.h src/midi2_msg.h
	$(CC) $(CFLAGS) -o $@ test/test_midi2_conv.c src/midi2_conv.c

test/test_midi2_dispatch: test/test_midi2_dispatch.c src/midi2_dispatch.c src/midi2_dispatch.h src/midi2_msg.h
	$(CC) $(CFLAGS) -o $@ test/test_midi2_dispatch.c src/midi2_dispatch.c

test/test_midi2_ci_msg: test/test_midi2_ci_msg.c src/midi2_ci_msg.h
	$(CC) $(CFLAGS) -o $@ test/test_midi2_ci_msg.c

test/test_midi2_ci_dispatch: test/test_midi2_ci_dispatch.c src/midi2_ci_dispatch.c src/midi2_ci_dispatch.h src/midi2_ci_msg.h
	$(CC) $(CFLAGS) -o $@ test/test_midi2_ci_dispatch.c src/midi2_ci_dispatch.c

test/test_midi2_amalgam: test/test_midi2_amalgam.c dist/midi2.h
	$(CC) $(CFLAGS) -Idist -o $@ $<

clean:
	rm -f $(TESTS)
