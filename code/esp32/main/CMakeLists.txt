# R4 benchmark component. The algorithm sources live in the shared code tree.
set(R4 "${CMAKE_CURRENT_LIST_DIR}/../..")
# AEGIS-256 reference scheme: libaegis 0.9.1 (MIT), portable C path on Xtensa.
set(AEGIS "${R4}/third_party/libaegis/src")
file(GLOB AEGIS_SRCS "${AEGIS}/common/*.c" "${AEGIS}/aegis128l/*.c" "${AEGIS}/aegis128x2/*.c"
     "${AEGIS}/aegis128x4/*.c" "${AEGIS}/aegis256/*.c" "${AEGIS}/aegis256x2/*.c" "${AEGIS}/aegis256x4/*.c")
idf_component_register(
    SRCS "bench_esp32.c" "selftest.c" "nxa5_hw.c"
         "${R4}/aes256set/nxa5.c" "${R4}/snow5gset/nxa4.c" "${R4}/snow5gset/snow5g.c"
         "${R4}/zuc256set/nxa6.c" "${R4}/zuc256set/zuc256.c"
         "${R4}/common/aes256.c" "${R4}/common/polyval.c" "${R4}/common/aead1.c"
         "${R4}/l2port/tab_l2p.c" "${R4}/l2port/aead1_l2p.c" "${R4}/l2port/nxa5_l2p.c"
         "${R4}/l2port/nxa4_l2p.c" "${R4}/l2x86/nxa6_l2.c"
         ${AEGIS_SRCS}
    INCLUDE_DIRS "." "${AEGIS}/include"
    LDFRAGMENTS "linker.lf"
    REQUIRES mbedtls esp_timer esp_hw_support hal spi_flash freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu11 -Wno-unused-function)
# Optional builds (idf.py -D...):
#   -DR4_AEGIS_TTAB=1  libaegis soft AES with plain T-tables (FAVOR_PERFORMANCE), the same
#                      table style as our L2 AES-256; the default is libaegis' cache-hardened path.
#   -DR4_ONLY_BASE=1   measure only the reference schemes (level "base").
if(R4_AEGIS_TTAB)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAVOR_PERFORMANCE R4_AEGIS_TTAB=1)
endif()
if(R4_ONLY_BASE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE R4_ONLY_BASE=1)
endif()
