/*
 * ct.c - timing-leakage test of the R4 implementations in the style of dudect
 *        (Reparaz, Balasch, Verbauwhede, DATE 2017). Linux: Zen 4, Raspberry Pi 5.
 *
 * For each implementation we time single calls of 256-NCA (or the reference AEAD)
 * on two classes of inputs, chosen at random for every measurement:
 *   test "key":  class 0 = a fixed all-zero key,      class 1 = a fresh random key
 *                (plaintext random in both classes)
 *   test "data": class 0 = an all-zero plaintext,     class 1 = a fresh random plaintext
 *                (one fixed random key in both classes)
 * Welch's t-test compares the two timing distributions. As in dudect, we also apply
 * it to the measurements below 100 percentiles (cropping), and we report the largest
 * |t|. |t| > 4.5 counts as evidence of leakage, |t| > 10 as strong evidence.
 * Two controls check the harness: a loop with a key-dependent early exit (must fail)
 * and a branch-free loop over the key (must pass).
 *
 * Timer: rdtsc (x86-64) or the user-mode core-cycle counter via perf_event (aarch64).
 *
 *   ./ct [--seconds S] [--max N] [--only L2:256-NCA5,...] [--platform zen4] [-o out.csv]
 */
#define _GNU_SOURCE
#include <getopt.h>
#include <math.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

#include "../aes256set/nxa5.h"
#include "../snow5gset/nxa4.h"
#include "../zuc256set/nxa6.h"
#ifdef WITH_L2
#include "../l2x86/nxa_l2.h"
#endif
#ifdef WITH_L3
#include "../l3x86/nxa_l3.h"
#endif
#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#endif
#ifdef WITH_AEGIS
#include "aegis.h"
#include "aegis256.h"
#endif
#if defined(__x86_64__)
#include <x86intrin.h>
#endif

#define TAG 16
#define MAXN 1024
static const uint8_t XIV[6] = {1, 2, 3, 4, 5, 6};
static uint8_t OUT[MAXN + 64], MAC[TAG];
#ifdef WITH_OPENSSL
static EVP_CIPHER_CTX *CTX;
#endif

typedef void (*op_fn)(const uint8_t *key, const uint8_t *in, uint32_t n);

static void l1_5(const uint8_t *k, const uint8_t *in, uint32_t n) { nca5_256(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l1_4(const uint8_t *k, const uint8_t *in, uint32_t n) { nca4_256(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l1_6(const uint8_t *k, const uint8_t *in, uint32_t n) { nca6_256(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
#ifdef WITH_L2
static void l2_5(const uint8_t *k, const uint8_t *in, uint32_t n) { nca5_256_l2(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l2_4(const uint8_t *k, const uint8_t *in, uint32_t n) { nca4_256_l2(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l2_6(const uint8_t *k, const uint8_t *in, uint32_t n) { nca6_256_l2(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l2c_6(const uint8_t *k, const uint8_t *in, uint32_t n) { nca6ct_256_l2(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
#endif
#ifdef WITH_L3
static void l3_5(const uint8_t *k, const uint8_t *in, uint32_t n) { nca5_256_l3(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l3_4(const uint8_t *k, const uint8_t *in, uint32_t n) { nca4_256_l3(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l3_6(const uint8_t *k, const uint8_t *in, uint32_t n) { nca6_256_l3(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
static void l3c_6(const uint8_t *k, const uint8_t *in, uint32_t n) { nca6ct_256_l3(0, 7, 3, 1, XIV, k, NULL, 0, in, n * 8, OUT, TAG, MAC); }
#endif
#ifdef WITH_OPENSSL
static void b_gcm(const uint8_t *k, const uint8_t *in, uint32_t n)
{
    static const uint8_t iv[12] = {7};
    int l;
    EVP_EncryptInit_ex(CTX, EVP_aes_256_gcm(), NULL, k, iv);
    EVP_EncryptUpdate(CTX, OUT, &l, in, (int)n);
    EVP_EncryptFinal_ex(CTX, OUT + l, &l);
    EVP_CIPHER_CTX_ctrl(CTX, EVP_CTRL_GCM_GET_TAG, TAG, MAC);
}
static void b_chacha(const uint8_t *k, const uint8_t *in, uint32_t n)
{
    static const uint8_t iv[12] = {7};
    int l;
    EVP_EncryptInit_ex(CTX, EVP_chacha20_poly1305(), NULL, k, iv);
    EVP_EncryptUpdate(CTX, OUT, &l, in, (int)n);
    EVP_EncryptFinal_ex(CTX, OUT + l, &l);
    EVP_CIPHER_CTX_ctrl(CTX, EVP_CTRL_AEAD_GET_TAG, TAG, MAC);
}
#endif
#ifdef WITH_AEGIS
static void b_aegis(const uint8_t *k, const uint8_t *in, uint32_t n)
{
    static const uint8_t npub[32] = {7};
    aegis256_encrypt_detached(OUT, MAC, TAG, in, n, NULL, 0, npub, k);
}
#endif
/* Controls. leak: stops at the first zero key byte, so the all-zero key is fast.
 * ct: touches every key byte without branching on it. */
static volatile uint32_t SINK;
static __attribute__((noinline)) void c_leak(const uint8_t *k, const uint8_t *in, uint32_t n)
{
    (void)in; (void)n;
    uint32_t a = 0;
    for (int r = 0; r < 8; r++)
        for (int i = 0; i < 32; i++) { if (k[i] == 0) break; a = a * 31 + k[i]; }
    SINK = a;
}
static __attribute__((noinline)) void c_ct(const uint8_t *k, const uint8_t *in, uint32_t n)
{
    (void)in; (void)n;
    uint32_t a = 0;
    for (int r = 0; r < 8; r++)
        for (int i = 0; i < 32; i++) a = a * 31 + k[i];
    SINK = a;
}

typedef struct { const char *level, *name; op_fn fn; uint32_t bytes; } impl_t;
static const impl_t IMPL[] = {
    {"ctrl", "leaky-early-exit", c_leak, 64}, {"ctrl", "branch-free", c_ct, 64},
    {"L1", "256-NCA5", l1_5, 64}, {"L1", "256-NCA4", l1_4, 64}, {"L1", "256-NCA6", l1_6, 64},
#ifdef WITH_L2
    {"L2", "256-NCA5", l2_5, 64}, {"L2", "256-NCA4", l2_4, 64}, {"L2", "256-NCA6", l2_6, 64},
    {"L2ct", "256-NCA6", l2c_6, 64},
#endif
#ifdef WITH_L3
    /* L3 runs the L2 code below 512 bytes, so we test it at 1024 bytes */
    {"L3", "256-NCA5", l3_5, 1024}, {"L3", "256-NCA4", l3_4, 1024}, {"L3", "256-NCA6", l3_6, 1024},
    {"L3ct", "256-NCA6", l3c_6, 1024},
#endif
#ifdef WITH_OPENSSL
    {"base", "OpenSSL-AES-256-GCM", b_gcm, 64}, {"base", "OpenSSL-ChaCha20-Poly1305", b_chacha, 64},
#endif
#ifdef WITH_AEGIS
    {"base", "libaegis-AEGIS-256", b_aegis, 64},
#endif
};
#define NIMPL (sizeof IMPL / sizeof IMPL[0])

/* ---- timer -------------------------------------------------------------- */
#if defined(__x86_64__)
static inline uint64_t tick(void) { _mm_lfence(); uint64_t t = __rdtsc(); _mm_lfence(); return t; }
static const char *TIMER = "rdtsc";
static void timer_init(void) {}
#else
static int PFD = -1;
static const char *TIMER = "perf-user-cycles";
static void timer_init(void)
{
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof pe);
    pe.type = PERF_TYPE_HARDWARE; pe.size = sizeof pe; pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.exclude_kernel = 1; pe.exclude_hv = 1;                       /* the read() itself is not counted */
    PFD = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (PFD < 0) { fprintf(stderr, "perf_event_open failed: run sudo ./clock_lock.sh first\n"); exit(1); }
}
static inline uint64_t tick(void) { uint64_t v = 0; if (read(PFD, &v, sizeof v) != sizeof v) v = 0; return v; }
#endif

/* ---- random numbers (not cryptographic; only to pick classes and inputs) --- */
static uint64_t RS = 0x9E3779B97F4A7C15ull;
static inline uint64_t rnd(void) { RS ^= RS << 13; RS ^= RS >> 7; RS ^= RS << 17; return RS; }
static void rfill(uint8_t *p, size_t n) { for (size_t i = 0; i < n; i += 8) { uint64_t r = rnd(); memcpy(p + i, &r, n - i < 8 ? n - i : 8); } }

/* ---- Welch t-test, online (Welford) ------------------------------------ */
typedef struct { double m[2], m2[2], n[2]; } ttest;
static void t_push(ttest *t, double x, int c)
{
    t->n[c] += 1; double d = x - t->m[c]; t->m[c] += d / t->n[c]; t->m2[c] += d * (x - t->m[c]);
}
static double t_val(const ttest *t)
{
    if (t->n[0] < 2 || t->n[1] < 2) return 0;
    double v0 = t->m2[0] / (t->n[0] - 1), v1 = t->m2[1] / (t->n[1] - 1);
    double den = sqrt(v0 / t->n[0] + v1 / t->n[1]);
    return den > 0 ? (t->m[0] - t->m[1]) / den : 0;
}

#define NCROP 100
#define BATCH 10000
static int cmp_u64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }

typedef struct { double tmax, traw; int imax; uint64_t n; double med0, med1; } result;

static result run_test(const impl_t *im, int test, double secs, uint64_t maxn)
{
    static uint8_t keys[BATCH][32], ins[BATCH][MAXN];
    static uint8_t cls[BATCH];
    static uint64_t dt[BATCH], srt[BATCH];
    uint8_t fixkey[32];
    rfill(fixkey, 32);
    ttest tt[NCROP + 1];
    memset(tt, 0, sizeof tt);
    double thr[NCROP];
    int have_thr = 0;
    result r = {0};
    uint64_t sum_med[2] = {0, 0}, nmed = 0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        /* prepare one batch of inputs outside the timed region */
        for (int i = 0; i < BATCH; i++) {
            cls[i] = rnd() & 1;
            if (test == 0) { if (cls[i]) rfill(keys[i], 32); else memset(keys[i], 0, 32); rfill(ins[i], im->bytes); }
            else { memcpy(keys[i], fixkey, 32); if (cls[i]) rfill(ins[i], im->bytes); else memset(ins[i], 0, im->bytes); }
        }
        for (int i = 0; i < BATCH; i++) {
            uint64_t a = tick();
            im->fn(keys[i], ins[i], im->bytes);
            uint64_t b = tick();
            dt[i] = b - a;
        }
        if (!have_thr) {                          /* first batch: warm-up, sets the cropping thresholds */
            memcpy(srt, dt, sizeof dt);
            qsort(srt, BATCH, sizeof srt[0], cmp_u64);
            for (int p = 0; p < NCROP; p++) {
                double q = 1.0 - pow(0.5, 10.0 * (p + 1) / NCROP);
                thr[p] = (double)srt[(int)(q * (BATCH - 1))];
            }
            have_thr = 1;
            continue;
        }
        for (int i = 0; i < BATCH; i++) {
            double x = (double)dt[i];
            t_push(&tt[NCROP], x, cls[i]);
            for (int p = 0; p < NCROP; p++) if (x < thr[p]) t_push(&tt[p], x, cls[i]);
        }
        /* medians per class of this batch, for the report */
        {
            static uint64_t c0[BATCH], c1[BATCH]; int n0 = 0, n1 = 0;
            for (int i = 0; i < BATCH; i++) { if (cls[i]) c1[n1++] = dt[i]; else c0[n0++] = dt[i]; }
            qsort(c0, n0, sizeof c0[0], cmp_u64); qsort(c1, n1, sizeof c1[0], cmp_u64);
            if (n0 && n1) { sum_med[0] += c0[n0 / 2]; sum_med[1] += c1[n1 / 2]; nmed++; }
        }
        r.n += BATCH;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double el = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
        if (el >= secs || r.n >= maxn) break;
    }
    r.traw = t_val(&tt[NCROP]);
    r.tmax = fabs(r.traw); r.imax = -1;
    for (int p = 0; p < NCROP; p++) {
        if (tt[p].n[0] + tt[p].n[1] < 1000) continue;
        double v = fabs(t_val(&tt[p]));
        if (v > r.tmax) { r.tmax = v; r.imax = p; }
    }
    r.med0 = nmed ? (double)sum_med[0] / nmed : 0;
    r.med1 = nmed ? (double)sum_med[1] / nmed : 0;
    return r;
}

int main(int argc, char **argv)
{
    double secs = 20;
    uint64_t maxn = 20000000;
    int cpu = -1;
    char only[512] = "", plat[64] = "linux", *outp = NULL;
    static struct option lo[] = {{"seconds", 1, 0, 's'}, {"max", 1, 0, 'm'}, {"only", 1, 0, 'a'},
                                 {"platform", 1, 0, 'p'}, {"cpu", 1, 0, 'c'}, {0, 0, 0, 0}};
    int o;
    while ((o = getopt_long(argc, argv, "o:", lo, NULL)) != -1) {
        switch (o) {
        case 's': secs = atof(optarg); break;
        case 'm': maxn = strtoull(optarg, NULL, 10); break;
        case 'a': snprintf(only, sizeof only, "%s", optarg); break;
        case 'p': snprintf(plat, sizeof plat, "%s", optarg); break;
        case 'c': cpu = atoi(optarg); break;
        case 'o': outp = optarg; break;
        default: fprintf(stderr, "usage: %s [--seconds S] [--max N] [--only L:name,...] [--cpu N] [--platform P] [-o csv]\n", argv[0]); return 2;
        }
    }
    if (cpu >= 0) { cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(cpu, &cs); sched_setaffinity(0, sizeof cs, &cs); }
    timer_init();
#ifdef WITH_OPENSSL
    CTX = EVP_CIPHER_CTX_new();
#endif
#ifdef WITH_AEGIS
    if (aegis_init() != 0) return 1;
#endif
    FILE *out = outp ? fopen(outp, "w") : stdout;
    if (!out) { perror(outp); return 1; }
    fprintf(out, "# R4 timing-leakage test (dudect method), timer=%s, %.0f s or %llu measurements per test, cpu=%d\n",
            TIMER, secs, (unsigned long long)maxn, cpu);
    fprintf(out, "platform,level,alg,bytes,test,measurements,t_raw,t_max,crop_index,median_class0,median_class1,verdict\n");
    for (size_t a = 0; a < NIMPL; a++) {
        if (only[0]) {
            char k[96], tmp[520];
            snprintf(k, sizeof k, ",%s:%s,", IMPL[a].level, IMPL[a].name);
            snprintf(tmp, sizeof tmp, ",%s,", only);
            if (!strstr(tmp, k)) continue;
        }
        for (int test = 0; test < 2; test++) {
            result r = run_test(&IMPL[a], test, secs, maxn);
            const char *v = r.tmax > 10 ? "LEAK" : r.tmax > 4.5 ? "possible" : "pass";
            fprintf(out, "%s,%s,%s,%u,%s,%llu,%.2f,%.2f,%d,%.1f,%.1f,%s\n", plat, IMPL[a].level, IMPL[a].name,
                    IMPL[a].bytes, test ? "data" : "key", (unsigned long long)r.n, r.traw, r.tmax, r.imax, r.med0, r.med1, v);
            fflush(out);
            fprintf(stderr, "%-4s %-26s %5u B  %-4s  n=%9llu  |t|max=%8.2f  %s\n", IMPL[a].level, IMPL[a].name,
                    IMPL[a].bytes, test ? "data" : "key", (unsigned long long)r.n, r.tmax, v);
        }
    }
    if (outp) fclose(out);
    return 0;
}
