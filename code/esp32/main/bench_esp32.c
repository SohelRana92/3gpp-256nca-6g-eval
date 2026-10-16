/*
 * bench_esp32.c - R4 device-tier benchmark (ESP32, ESP-IDF).
 *
 * Same method as the Linux harness (../../bench/bench.c): each measurement
 * times one call of the public API for one packet; size 0 gives the fixed
 * setup cost. A batch of K calls lasts at least 2 ms (calibrated on the
 * fastest of 3 trial batches); then 5 warm-up and 31 timed batches. We report
 * the median, quartiles and minimum per call.
 *
 * Clocks: the Xtensa CCOUNT cycle counter, and esp_timer (1 us) for wall
 * time; their ratio is the effective clock. The benchmark runs as the only
 * task on core 1 (the APP CPU); Wi-Fi and Bluetooth are never started.
 *
 * Levels: L1 (reference C), L2 (portable 32-bit C, ../../l2port), HW (the
 * AES-256 set on the AES accelerator), base (mbedTLS AES-256-CTR and
 * AES-256-GCM as shipped with ESP-IDF, which use the same accelerator).
 * The run starts with the self-tests and stops if any check fails.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_private/esp_clk.h"
#include "sdkconfig.h"
#include "mbedtls/gcm.h"
#include "mbedtls/aes.h"
#include "mbedtls/chachapoly.h"
#include "aegis.h"
#include "aegis256.h"

#include "../../aes256set/nxa5.h"
#include "../../snow5gset/nxa4.h"
#include "../../zuc256set/nxa6.h"
#include "../../l2x86/nxa_l2.h"
#include "nxa5_hw.h"
#include "selftest.h"

#define MAXBYTES 16384u
#define TAG 16
#define REPS 31
#define MIN_US 2000

static uint32_t W_IN[(MAXBYTES + 64) / 4], W_OUT[(MAXBYTES + 64) / 4];   /* word-aligned, fixed place in DRAM */
static uint8_t *const IN = (uint8_t *)W_IN, *const OUT = (uint8_t *)W_OUT;
static uint8_t KEY[32], XIV[6] = {1, 2, 3, 4, 5, 6}, MAC[TAG];
static uint32_t COUNT;
static volatile uint8_t SINK;

typedef void (*op_fn)(uint32_t bytes);

static void op_nea5(uint32_t n) { nea5_256(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op_nia5(uint32_t n) { nia5_256(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op_nca5(uint32_t n) { nca5_256(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op_nea4(uint32_t n) { nea4_256(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op_nia4(uint32_t n) { nia4_256(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op_nca4(uint32_t n) { nca4_256(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op_nea6(uint32_t n) { nea6_256(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op_nia6(uint32_t n) { nia6_256(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op_nca6(uint32_t n) { nca6_256(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op2_nea5(uint32_t n) { nea5_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2_nia5(uint32_t n) { nia5_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2_nca5(uint32_t n) { nca5_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op2_nea4(uint32_t n) { nea4_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2_nia4(uint32_t n) { nia4_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2_nca4(uint32_t n) { nca4_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op2_nea6(uint32_t n) { nea6_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2_nia6(uint32_t n) { nia6_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2_nca6(uint32_t n) { nca6_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void oph_nea5(uint32_t n) { nea5_256_hw(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void oph_nia5(uint32_t n) { nia5_256_hw(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void oph_nca5(uint32_t n) { nca5_256_hw(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }

/* Production baselines: mbedTLS from ESP-IDF, one key setup per packet. */
static void op_mbed_ctr(uint32_t n)
{
    mbedtls_aes_context a;
    uint8_t nonce[16] = {0}, stream[16];
    size_t off = 0;
    memcpy(nonce, &COUNT, 4); COUNT++;
    mbedtls_aes_init(&a);
    mbedtls_aes_setkey_enc(&a, KEY, 256);
    mbedtls_aes_crypt_ctr(&a, n, &off, nonce, stream, IN, OUT);
    mbedtls_aes_free(&a);
}
static void op_mbed_gcm(uint32_t n)
{
    mbedtls_gcm_context g;
    uint8_t iv[12] = {0};
    memcpy(iv, &COUNT, 4); COUNT++;
    mbedtls_gcm_init(&g);
    mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, KEY, 256);
    mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, n, iv, 12, NULL, 0, IN, OUT, TAG, MAC);
    mbedtls_gcm_free(&g);
}

/* ChaCha20-Poly1305 (RFC 8439) from mbedTLS of ESP-IDF, in software: no ESP32 accelerator covers it. */
static void op_mbed_chacha(uint32_t n)
{
    mbedtls_chachapoly_context c;
    uint8_t iv[12] = {0};
    memcpy(iv, &COUNT, 4); COUNT++;
    mbedtls_chachapoly_init(&c);
    mbedtls_chachapoly_setkey(&c, KEY);
    mbedtls_chachapoly_encrypt_and_tag(&c, n, iv, NULL, 0, IN, OUT, MAC);
    mbedtls_chachapoly_free(&c);
}
/* AEGIS-256 (RFC 10032) from libaegis 0.9.1, portable C with table-based AES rounds. */
static void op_aegis256(uint32_t n)
{
    uint8_t npub[32] = {0};
    memcpy(npub, &COUNT, 4); COUNT++;
    aegis256_encrypt_detached(OUT, MAC, TAG, IN, n, NULL, 0, npub, KEY);
}

/* Known-answer tests of the two reference schemes. */
static int unhex(const char *h, uint8_t *o) { int n = 0; unsigned v; while (h[0] && h[1] && sscanf(h, "%2x", &v) == 1) { o[n++] = (uint8_t)v; h += 2; } return n; }
static int ref_selftest(void)
{
    int bad = 0;
    {   /* RFC 8439, Section 2.8.2 */
        static const char pt[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
        uint8_t k[32], iv[12], ad[12], c[sizeof pt], t[16], et[16];
        for (int i = 0; i < 32; i++) k[i] = (uint8_t)(0x80 + i);
        unhex("070000004041424344454647", iv); unhex("50515253c0c1c2c3c4c5c6c7", ad);
        unhex("1ae10b594f09e26a7e902ecbd0600691", et);
        mbedtls_chachapoly_context x;
        mbedtls_chachapoly_init(&x);
        mbedtls_chachapoly_setkey(&x, k);
        mbedtls_chachapoly_encrypt_and_tag(&x, sizeof pt - 1, iv, ad, 12, (const uint8_t *)pt, c, t);
        mbedtls_chachapoly_free(&x);
        int ok = c[0] == 0xd3 && c[1] == 0x1a && memcmp(t, et, 16) == 0;
        printf("# KAT ChaCha20-Poly1305 (RFC 8439 2.8.2): %s\n", ok ? "ok" : "FAIL"); bad |= !ok;
    }
    {   /* RFC 10032, Appendix A.3.2, test vector 1 */
        uint8_t k[32], npub[32], m[16] = {0}, c[16], t[16], ec[16], et[16];
        unhex("1001000000000000000000000000000000000000000000000000000000000000", k);
        unhex("1000020000000000000000000000000000000000000000000000000000000000", npub);
        unhex("754fc3d8c973246dcc6d741412a4b236", ec); unhex("3fe91994768b332ed7f570a19ec5896e", et);
        aegis256_encrypt_detached(c, t, 16, m, 16, NULL, 0, npub, k);
        int ok = memcmp(c, ec, 16) == 0 && memcmp(t, et, 16) == 0;
        printf("# KAT AEGIS-256 (RFC 10032 A.3.2 #1): %s\n", ok ? "ok" : "FAIL"); bad |= !ok;
    }
    return bad;
}

typedef struct { const char *level, *name, *set; op_fn fn; } alg_t;
static const alg_t ALGS[] = {
    {"L1", "256-NEA5", "AES-256", op_nea5}, {"L1", "256-NIA5", "AES-256", op_nia5}, {"L1", "256-NCA5", "AES-256", op_nca5},
    {"L1", "256-NEA4", "Snow 5G", op_nea4}, {"L1", "256-NIA4", "Snow 5G", op_nia4}, {"L1", "256-NCA4", "Snow 5G", op_nca4},
    {"L1", "256-NEA6", "ZUC-256", op_nea6}, {"L1", "256-NIA6", "ZUC-256", op_nia6}, {"L1", "256-NCA6", "ZUC-256", op_nca6},
    {"L2", "256-NEA5", "AES-256", op2_nea5}, {"L2", "256-NIA5", "AES-256", op2_nia5}, {"L2", "256-NCA5", "AES-256", op2_nca5},
    {"L2", "256-NEA4", "Snow 5G", op2_nea4}, {"L2", "256-NIA4", "Snow 5G", op2_nia4}, {"L2", "256-NCA4", "Snow 5G", op2_nca4},
    {"L2", "256-NEA6", "ZUC-256", op2_nea6}, {"L2", "256-NIA6", "ZUC-256", op2_nia6}, {"L2", "256-NCA6", "ZUC-256", op2_nca6},
    {"HW", "256-NEA5", "AES-256", oph_nea5}, {"HW", "256-NIA5", "AES-256", oph_nia5}, {"HW", "256-NCA5", "AES-256", oph_nca5},
    {"base", "mbedTLS-AES-256-CTR", "baseline", op_mbed_ctr}, {"base", "mbedTLS-AES-256-GCM", "baseline", op_mbed_gcm},
    {"base", "mbedTLS-ChaCha20-Poly1305", "baseline", op_mbed_chacha}, {"base",
#ifdef R4_AEGIS_TTAB
     "libaegis-AEGIS-256-ttab",
#else
     "libaegis-AEGIS-256",
#endif
     "baseline", op_aegis256},
};
#define NALGS (sizeof ALGS / sizeof ALGS[0])

static const uint32_t SIZES[] = {0, 16, 40, 64, 128, 256, 576, 1024, 1500, 4096, 9000, 16384};
#define NSIZES (sizeof SIZES / sizeof SIZES[0])

static void batch(op_fn f, uint32_t n, uint32_t k, double *us, double *cyc)
{
    int64_t t0 = esp_timer_get_time();
    uint32_t c0 = esp_cpu_get_cycle_count();
    for (uint32_t i = 0; i < k; i++) { f(n); SINK ^= OUT[0] ^ MAC[0]; }
    uint32_t c1 = esp_cpu_get_cycle_count();
    int64_t t1 = esp_timer_get_time();
    *us = (double)(t1 - t0);
    *cyc = (double)(uint32_t)(c1 - c0);                /* < 2^32 cycles: about 17 s at 240 MHz */
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

typedef struct { double med, q1, q3, min; } stats_t;
static stats_t stats(double *v, int n)
{
    qsort(v, (size_t)n, sizeof *v, cmp_d);
    stats_t s = {v[n / 2], v[n / 4], v[(3 * n) / 4], v[0]};
    return s;
}

static void bench_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    uint32_t flash = 0;
    esp_flash_get_size(NULL, &flash);
    printf("\n# R4 ESP32 benchmark\n");
    printf("# chip=%s cores=%d revision=v%d.%d cpu_mhz=%d flash_mb=%lu idf=%s compiler=%s opt=%s\n",
           CONFIG_IDF_TARGET, ci.cores, ci.revision / 100, ci.revision % 100, esp_clk_cpu_freq() / 1000000,
           (unsigned long)(flash >> 20), esp_get_idf_version(), __VERSION__,
#if CONFIG_COMPILER_OPTIMIZATION_PERF
           "O2"
#else
           "not-O2"
#endif
           );
    printf("# code placement: all measured code in IRAM, tables in DRAM (main/linker.lf)\n");
#ifdef R4_AEGIS_TTAB
    printf("# libaegis soft AES: plain T-tables (FAVOR_PERFORMANCE)\n");
#else
    printf("# libaegis soft AES: default cache-hardened path (SOFTAES_STRIDE 16)\n");
#endif
    printf("# task on core %d; free heap %lu bytes\n", xPortGetCoreID(), (unsigned long)esp_get_free_heap_size());

    printf("# self-tests ...\n");
    int64_t t0 = esp_timer_get_time();
    int bad = r4_selftest(200);
    if (aegis_init() != 0) bad = 1;
    bad |= ref_selftest();
    printf("# self-tests took %.1f s\n", (double)(esp_timer_get_time() - t0) / 1e6);
    if (bad) {
        printf("R4-STOP: self-test failed, no timing done\n");
        for (;;) vTaskDelay(portMAX_DELAY);
    }

    srand(12345);
    for (uint32_t i = 0; i < MAXBYTES + 64; i++) IN[i] = (uint8_t)rand();
    for (int i = 0; i < 32; i++) KEY[i] = (uint8_t)rand();

    static double vus[REPS], vcy[REPS];
    printf("R4-CSV-BEGIN\n");
    printf("platform,level,set,alg,bytes,iters,reps,med_ns,q1_ns,q3_ns,min_ns,"
           "med_cycles,q1_cycles,q3_cycles,min_cycles,cpb,eff_ghz,med_tsc\n");
    for (size_t a = 0; a < NALGS; a++) {
#ifdef R4_ONLY_BASE
        if (strcmp(ALGS[a].level, "base") != 0) continue;
#endif
        for (size_t s = 0; s < NSIZES; s++) {
            uint32_t n = SIZES[s], k = 1;
            double us, cy;
            for (;;) {                                  /* calibrate on the fastest of 3 batches */
                double best = 1e300;
                for (int t = 0; t < 3; t++) { batch(ALGS[a].fn, n, k, &us, &cy); if (us < best) best = us; }
                if (best >= MIN_US || k >= (1u << 20)) break;
                k *= 2;
            }
            for (int w = 0; w < 5; w++) batch(ALGS[a].fn, n, k, &us, &cy);
            for (int r = 0; r < REPS; r++) {
                batch(ALGS[a].fn, n, k, &vus[r], &vcy[r]);
                vus[r] = vus[r] * 1000.0 / k;          /* ns per call */
                vcy[r] /= k;                            /* cycles per call */
            }
            stats_t sn = stats(vus, REPS), sc = stats(vcy, REPS);
            printf("esp32,%s,%s,%s,%lu,%lu,%d,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,",
                   ALGS[a].level, ALGS[a].set, ALGS[a].name, (unsigned long)n, (unsigned long)k, REPS,
                   sn.med, sn.q1, sn.q3, sn.min, sc.med, sc.q1, sc.q3, sc.min);
            if (n) printf("%.3f,", sc.med / n); else printf(",");
            printf("%.4f,\n", sc.med / sn.med);
            vTaskDelay(1);                             /* let the idle task run between points */
        }
    }
    printf("R4-CSV-END\n");
    printf("R4-DONE\n");
    for (;;) vTaskDelay(portMAX_DELAY);
}

void app_main(void)
{
    xTaskCreatePinnedToCore(bench_task, "r4bench", 16384, NULL, configMAX_PRIORITIES - 1, NULL, 1);
}
