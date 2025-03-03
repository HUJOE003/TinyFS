CC = gcc
CFLAGS = -Wall -g

# Programs
PROG = tinyFSDemo
TEST_PROG1 = diskTest
TEST_PROG2 = tfsTest
TEST_PROG3 = fragTest

# Source files
SRCS = libTinyFS.c libDisk.c tinyFSDemo.c diskTest.c tfsTest.c
OBJS = $(SRCS:.c=.o)

# Dependencies
DEPS = libTinyFS.h tinyFS.h libDisk.h TinyFS_errno.h

# Build all programs
all: $(PROG) $(TEST_PROG1) $(TEST_PROG2)

# Compilation rule (generalized)
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

# Linking programs
$(PROG): tinyFSDemo.o libTinyFS.o libDisk.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_PROG1): diskTest.o libDisk.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_PROG2): tfsTest.o libTinyFS.o libDisk.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_PROG3): fragTest.o libTinyFS.o libDisk.o
	$(CC) $(CFLAGS) -o $@ $^

# Clean build artifacts
clean:
	rm -f $(PROG) $(TEST_PROG1) $(TEST_PROG2) $(OBJS) *.dsk tinyFSDisk defragTestDisk fragTest testDisk

# Custom targets
tfsTestGiven: clean $(TEST_PROG2)
	./$(TEST_PROG2)

diskTestGiven: clean $(TEST_PROG1)
	./$(TEST_PROG1)

.PHONY: all clean tfsTestGiven diskTestGiven
