# L3 for x86-64 with AVX-512 + VAES + VPCLMULQDQ (Zen 4, Ice Lake and later).
#   make check    build libnxa_l3.a and run the L3-vs-L1 equivalence test
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
ISA     = -maes -mpclmul -mssse3 -msse4.1 -mavx2 -mbmi2 -mavx512f -mavx512bw -mavx512vl -mvaes -mvpclmulqdq
L1SRC   = ../aes256set/nxa5.c ../snow5gset/nxa4.c ../snow5gset/snow5g.c \
          ../zuc256set/nxa6.c ../zuc256set/zuc256.c \
          ../common/aes256.c ../common/polyval.c ../common/aead1.c

all: libnxa_l3.a

libnxa_l3.a: nxa_l3.c nxa_l3.h ../l2x86/ksg_l2.h
	$(MAKE) -C ../l2x86 libnxa_l2.a CFLAGS="$(CFLAGS)"
	$(CC) $(CFLAGS) $(ISA) -c nxa_l3.c
	cp ../l2x86/libnxa_l2.a $@
	ar rs $@ nxa_l3.o

test_l3: test_l3.c libnxa_l3.a $(L1SRC)
	$(CC) $(CFLAGS) -o $@ test_l3.c libnxa_l3.a $(L1SRC)

check: test_l3
	./test_l3 20000

clean:
	rm -f *.o libnxa_l3.a test_l3

.PHONY: all check clean
