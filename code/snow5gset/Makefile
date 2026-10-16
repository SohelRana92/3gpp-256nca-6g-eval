# Snow 5G set: 256-NEA4 / 256-NIA4 / 256-NCA4 (TS 35.240), level L1.
CC      ?= gcc
CXX     ?= g++
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
SRC     = nxa4.c snow5g.c ../common/aes256.c ../common/polyval.c ../common/aead1.c
SPEC     ?= 35240-k10.docx
TESTDATA ?= 35241-k00.docx

all: libnxa4.a

libnxa4.a: $(SRC)
	rm -f *.o
	for f in $(SRC); do $(CC) $(CFLAGS) -c $$f -o $$(basename $$f .c).o || exit 1; done
	ar rcs $@ nxa4.o snow5g.o aes256.o polyval.o aead1.o

# Official implementers' test data (your copy of TS 35.241).
kat_cli: kat_cli.c libnxa4.a
	$(CC) $(CFLAGS) -o $@ kat_cli.c libnxa4.a

official: kat_cli
	python3 test_official.py "$(TESTDATA)" ./kat_cli

# Cross-check against the informative reference code (your copy of TS 35.240).
annex_ref.cpp:
	python3 ../common/extract_annexb.py "$(SPEC)" $@

crosscheck_annex: crosscheck_annex.cpp annex_ref.cpp libnxa4.a
	$(CXX) -O2 -std=c++17 -w -o $@ crosscheck_annex.cpp libnxa4.a

crosscheck: crosscheck_annex
	./crosscheck_annex 5000

clean:
	rm -f *.o libnxa4.a kat_cli crosscheck_annex annex_ref.cpp

.PHONY: all official crosscheck clean
