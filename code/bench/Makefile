# R4 benchmark harness (Linux: Zen 4, Raspberry Pi 5).
#   make            build ./bench with L1, the fast levels of this CPU, and OpenSSL
#   make OPENSSL=0  without the OpenSSL baselines
#   x86-64 : L1, L2 (../l2x86), L3 (../l3x86; skipped at run time without AVX-512/VAES)
#   aarch64: L1, L2 (../l2arm)
CC      ?= gcc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
L1SRC = ../aes256set/nxa5.c ../snow5gset/nxa4.c ../snow5gset/snow5g.c \
        ../zuc256set/nxa6.c ../zuc256set/zuc256.c \
        ../common/aes256.c ../common/polyval.c ../common/aead1.c

OPENSSL ?= $(shell test -f /usr/include/openssl/evp.h && echo 1 || echo 0)
ifeq ($(OPENSSL),1)
  XDEF += -DWITH_OPENSSL
  XLIB += -lcrypto
endif

# AEGIS-256 reference scheme from the bundled libaegis 0.9.1 (MIT), built with
# its own release flags (-O3). make AEGIS=0 leaves it out.
AEGIS ?= 1
AEGIS_DIR = ../third_party/libaegis/src
ifeq ($(AEGIS),1)
  AEGIS_SRC = $(wildcard $(AEGIS_DIR)/common/*.c $(AEGIS_DIR)/aegis128l/*.c $(AEGIS_DIR)/aegis128x2/*.c \
              $(AEGIS_DIR)/aegis128x4/*.c $(AEGIS_DIR)/aegis256/*.c $(AEGIS_DIR)/aegis256x2/*.c $(AEGIS_DIR)/aegis256x4/*.c)
  XDEF += -DWITH_AEGIS -I$(AEGIS_DIR)/include
  XLIB := libaegis.a $(XLIB)
  AEGIS_LIB = libaegis.a
endif

ARCH ?= $(shell uname -m)
ifeq ($(ARCH),x86_64)
  XDEF += -DWITH_L2 -DWITH_L3
  XLIB := ../l3x86/libnxa_l3.a $(XLIB)
  LIBDIR = ../l3x86
  LIBTGT = libnxa_l3.a
endif
ifeq ($(ARCH),aarch64)
  XDEF += -DWITH_L2
  XLIB := ../l2arm/libnxa_l2.a $(XLIB)
  LIBDIR = ../l2arm
  LIBTGT = libnxa_l2.a
endif

bench: bench.c $(L1SRC) ../*/*.h fastlib $(AEGIS_LIB)
	$(CC) $(CFLAGS) -DBENCH_CFLAGS='"$(CFLAGS)"' $(XDEF) -o $@ bench.c $(L1SRC) $(XLIB)

fastlib:
ifneq ($(LIBDIR),)
	$(MAKE) -C $(LIBDIR) $(LIBTGT) CC="$(CC)" CFLAGS="$(CFLAGS)"
endif

mc: mc.c $(L1SRC) ../*/*.h fastlib $(AEGIS_LIB)
	$(CC) $(CFLAGS) -pthread $(XDEF) -o $@ mc.c $(L1SRC) $(XLIB)

ct: ct.c $(L1SRC) ../*/*.h fastlib $(AEGIS_LIB)
	$(CC) $(CFLAGS) $(XDEF) -o $@ ct.c $(L1SRC) $(XLIB) -lm

libaegis.a: $(AEGIS_SRC)
	rm -rf aegis_obj && mkdir -p aegis_obj
	cd aegis_obj && $(CC) -O3 -std=c11 -w -I../$(AEGIS_DIR)/include -c $(addprefix ../,$(AEGIS_SRC))
	ar rcs $@ aegis_obj/*.o && rm -rf aegis_obj

clean:
	rm -f bench mc ct libaegis.a

.PHONY: clean fastlib
