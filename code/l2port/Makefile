# Portable L2 for 32-bit CPUs (ESP32 device tier). Plain C99, no intrinsics.
#   make check          host build + L2-vs-L1 equivalence test
#   make check32        the same as a 32-bit (i386) build, if gcc-multilib is present
# The ESP32 build compiles these files inside ../esp32 with ESP-IDF.
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
L2SRC   = tab_l2p.c aead1_l2p.c nxa5_l2p.c nxa4_l2p.c ../l2x86/nxa6_l2.c
L1SRC   = ../aes256set/nxa5.c ../snow5gset/nxa4.c ../snow5gset/snow5g.c \
          ../zuc256set/nxa6.c ../zuc256set/zuc256.c \
          ../common/aes256.c ../common/polyval.c ../common/aead1.c

test_l2: ../l2x86/test_l2.c $(L2SRC) $(L1SRC) *.h
	$(CC) $(CFLAGS) -o $@ ../l2x86/test_l2.c $(L2SRC) $(L1SRC)

check: test_l2
	./test_l2 20000

check32:
	$(CC) -m32 $(CFLAGS) -o test_l2_32 ../l2x86/test_l2.c $(L2SRC) $(L1SRC) && ./test_l2_32 20000

clean:
	rm -f test_l2 test_l2_32

.PHONY: check check32 clean
