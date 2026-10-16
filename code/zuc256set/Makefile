# ZUC-256 set: 256-NEA6 / 256-NIA6 / 256-NCA6 (TS 35.246), level L1.
CC      ?= gcc
CXX     ?= g++
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
SRC     = nxa6.c zuc256.c ../common/aes256.c ../common/polyval.c ../common/aead1.c
SPEC     ?= 35246-k00.docx
TESTDATA ?= 35247-k00.docx

all: libnxa6.a

libnxa6.a: $(SRC)
	rm -f *.o
	for f in $(SRC); do $(CC) $(CFLAGS) -c $$f -o $$(basename $$f .c).o || exit 1; done
	ar rcs $@ nxa6.o zuc256.o aes256.o polyval.o aead1.o

# Official implementers' test data (your copy of TS 35.247).
kat_cli: kat_cli.c libnxa6.a
	$(CC) $(CFLAGS) -o $@ kat_cli.c libnxa6.a

official: kat_cli
	python3 test_official.py "$(TESTDATA)" ./kat_cli

# Cross-check against the informative reference code (your copy of TS 35.246).
annex_ref.cpp:
	python3 ../common/extract_annexb.py "$(SPEC)" $@

crosscheck_annex: crosscheck_annex.cpp annex_ref.cpp libnxa6.a
	$(CXX) -O2 -std=c++17 -w -o $@ crosscheck_annex.cpp libnxa6.a

crosscheck: crosscheck_annex
	./crosscheck_annex 5000

clean:
	rm -f *.o libnxa6.a kat_cli crosscheck_annex annex_ref.cpp

.PHONY: all official crosscheck clean
