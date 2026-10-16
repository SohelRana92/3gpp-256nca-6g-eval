# Shared building blocks for R4 (level L1, reference portable C).
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

all: test_common

test_common: test_common.c aes256.c polyval.c aes256.h polyval.h
	$(CC) $(CFLAGS) -o $@ test_common.c aes256.c polyval.c

check: test_common
	./test_common
	python3 verify_common.py ./test_common 200

clean:
	rm -f test_common

.PHONY: all check clean
