# AES-256 set: 256-NEA5 / 256-NIA5 / 256-NCA5 (TS 35.243), level L1.
CC      ?= gcc
CXX     ?= g++
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
COMMON  = ../common/aes256.c ../common/polyval.c ../common/aead1.c
SPEC    ?= 35243-k00.docx

all: libnxa5.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libnxa5.a: nxa5.c $(COMMON)
	$(CC) $(CFLAGS) -c nxa5.c -o nxa5.o
	$(CC) $(CFLAGS) -c ../common/aes256.c -o aes256.o
	$(CC) $(CFLAGS) -c ../common/polyval.c -o polyval.o
	$(CC) $(CFLAGS) -c ../common/aead1.c -o aead1.o
	ar rcs $@ nxa5.o aes256.o polyval.o aead1.o

# Cross-check against Annex B. Needs your copy of TS 35.243 (SPEC=path/to/35243-k00.docx).
annexb_35243.cpp:
	python3 ../common/extract_annexb.py "$(SPEC)" $@

crosscheck_annexb: crosscheck_annexb.cpp annexb_35243.cpp libnxa5.a
	$(CXX) -O2 -std=c++17 -w -o $@ crosscheck_annexb.cpp libnxa5.a

crosscheck: crosscheck_annexb
	./crosscheck_annexb 5000

# Official implementers' test data. Needs your copy of TS 35.244 (TESTDATA=path/to/35244-k00.docx).
TESTDATA ?= 35244-k00.docx
kat_cli: kat_cli.c libnxa5.a
	$(CC) $(CFLAGS) -o $@ kat_cli.c libnxa5.a

official: kat_cli
	python3 test_official.py "$(TESTDATA)" ./kat_cli

clean:
	rm -f *.o libnxa5.a crosscheck_annexb annexb_35243.cpp kat_cli

.PHONY: all crosscheck official clean
