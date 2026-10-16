# L2 for ARMv8-A with the cryptography extension (Raspberry Pi 5, Cortex-A76).
#   make check    build libnxa_l2.a and run the L2-vs-L1 equivalence test
# ZUC-256 L2 is portable C and is shared with x86 (../l2x86/nxa6_l2.c).
# Cross-build from x86:  make CC=aarch64-linux-gnu-gcc RUN=qemu-aarch64 check
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
ISA     = -march=armv8-a+crypto
RUN     ?=
L2SRC   = aead1_l2a.c nxa5_l2a.c nxa4_l2a.c ../l2x86/nxa6_l2.c ../l2x86/nxa6_l2ct.c
L1SRC   = ../aes256set/nxa5.c ../snow5gset/nxa4.c ../snow5gset/snow5g.c \
          ../zuc256set/nxa6.c ../zuc256set/zuc256.c \
          ../common/aes256.c ../common/polyval.c ../common/aead1.c

all: libnxa_l2.a

libnxa_l2.a: $(L2SRC) ../l2x86/*.h ../l2x86/nxa6_l2.c ../zuc256set/zuc_sbox.h
	rm -f *.o
	for f in $(L2SRC); do $(CC) $(CFLAGS) $(ISA) -c $$f -o $$(basename $$f .c).o || exit 1; done
	ar rcs $@ aead1_l2a.o nxa5_l2a.o nxa4_l2a.o nxa6_l2.o nxa6_l2ct.o

test_l2: ../l2x86/test_l2.c libnxa_l2.a $(L1SRC)
	$(CC) $(CFLAGS) -o $@ ../l2x86/test_l2.c libnxa_l2.a $(L1SRC)

check: test_l2
	$(RUN) ./test_l2 20000

clean:
	rm -f *.o libnxa_l2.a test_l2

.PHONY: all check clean
