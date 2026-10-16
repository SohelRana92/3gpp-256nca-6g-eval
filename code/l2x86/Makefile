# L2 for x86-64 (AES-NI, PCLMULQDQ, SSSE3/SSE4.1).
#   make check    build libnxa_l2.a and run the L2-vs-L1 equivalence test
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
ISA     = -maes -mpclmul -mssse3 -msse4.1
L2SRC   = aead1_l2.c nxa5_l2.c nxa4_l2.c nxa6_l2.c
L1SRC   = ../aes256set/nxa5.c ../snow5gset/nxa4.c ../snow5gset/snow5g.c \
          ../zuc256set/nxa6.c ../zuc256set/zuc256.c \
          ../common/aes256.c ../common/polyval.c ../common/aead1.c

all: libnxa_l2.a

libnxa_l2.a: $(L2SRC) nxa6_l2ct.c *.h ../zuc256set/zuc_sbox.h
	rm -f *.o
	for f in $(L2SRC); do $(CC) $(CFLAGS) $(ISA) -c $$f || exit 1; done
	$(CC) $(CFLAGS) $(ISA) -mavx512f -mavx512bw -mavx512vbmi -c nxa6_l2ct.c
	ar rcs $@ $(L2SRC:.c=.o) nxa6_l2ct.o

test_l2: test_l2.c libnxa_l2.a $(L1SRC)
	$(CC) $(CFLAGS) -o $@ test_l2.c libnxa_l2.a $(L1SRC)

check: test_l2
	./test_l2 20000

clean:
	rm -f *.o libnxa_l2.a test_l2

.PHONY: all check clean
