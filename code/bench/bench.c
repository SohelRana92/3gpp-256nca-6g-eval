/*
 * bench.c - R4 per-packet benchmark for the 3GPP 256-bit algorithm sets on
 *           Linux hosts (Zen 4, Raspberry Pi 5). The ESP32 uses its own harness.
 *
 * Each measurement times one call of the public API (256-NEA/NIA/NCA x), as
 * a protocol stack would make it for one packet: IV construction, KSG
 * initialisation, H/Q/P generation, keystream, MAC. Size 0 therefore gives
 * the fixed per-packet setup cost.
 *
 * Method: calibrate a batch of K calls that lasts at least --min-ms, run 5
 * warm-up batches, then --reps timed batches. Per-call values are batch / K.
 * We report the median, quartiles and minimum over the batches.
 *
 * Clocks: CLOCK_MONOTONIC_RAW (ns) always; core cycles from the Linux perf
 * counter when available (needs kernel.perf_event_paranoid <= 2); the x86
 * TSC as an extra column. The effective clock (cycles / ns) is printed so
 * that a drifting frequency is visible in the data.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#endif
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define HAVE_TSC 1
#endif

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

#ifndef BENCH_CFLAGS
#define BENCH_CFLAGS "unknown"
#endif

#ifndef MAXBYTES
#define MAXBYTES 1048576u   /* 1 MiB; 3GPP lengths are 32-bit bit counts, so this is well in range */
#endif
#define TAG 16

static uint8_t KEY[32], XIV[6] = {1, 2, 3, 4, 5, 6}, AAD[64];
static uint8_t *IN, *OUT, MAC[TAG];
static uint32_t COUNT = 0;
static volatile uint8_t SINK;

/* ---- the measured operations ------------------------------------------- */
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
#ifdef WITH_L2
static void op2_nea5(uint32_t n) { nea5_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2_nia5(uint32_t n) { nia5_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2_nca5(uint32_t n) { nca5_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op2_nea4(uint32_t n) { nea4_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2_nia4(uint32_t n) { nia4_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2_nca4(uint32_t n) { nca4_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op2_nea6(uint32_t n) { nea6_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2_nia6(uint32_t n) { nia6_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2_nca6(uint32_t n) { nca6_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
/* ZUC-256 with the constant-time S-box layer (level L2ct) */
static void op2c_nea6(uint32_t n) { nea6ct_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op2c_nia6(uint32_t n) { nia6ct_256_l2(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op2c_nca6(uint32_t n) { nca6ct_256_l2(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
#endif
#ifdef WITH_L3
static void op3_nea5(uint32_t n) { nea5_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op3_nia5(uint32_t n) { nia5_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op3_nca5(uint32_t n) { nca5_256_l3(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op3_nea4(uint32_t n) { nea4_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op3_nia4(uint32_t n) { nia4_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op3_nca4(uint32_t n) { nca4_256_l3(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op3_nea6(uint32_t n) { nea6_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op3_nia6(uint32_t n) { nia6_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op3_nca6(uint32_t n) { nca6_256_l3(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
static void op3c_nea6(uint32_t n) { nea6ct_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, OUT); }
static void op3c_nia6(uint32_t n) { nia6ct_256_l3(COUNT++, 3, 1, XIV, KEY, IN, n * 8, TAG, MAC); }
static void op3c_nca6(uint32_t n) { nca6ct_256_l3(0, COUNT++, 3, 1, XIV, KEY, NULL, 0, IN, n * 8, OUT, TAG, MAC); }
#endif

#ifdef WITH_OPENSSL
/* Production baselines: OpenSSL with AES-NI/VAES, one key setup per packet. */
static EVP_CIPHER_CTX *CTX;
static void op_ossl_ctr(uint32_t n)
{
    uint8_t iv[16] = {0}; int l;
    memcpy(iv, &COUNT, 4); COUNT++;
    EVP_EncryptInit_ex(CTX, EVP_aes_256_ctr(), NULL, KEY, iv);
    EVP_EncryptUpdate(CTX, OUT, &l, IN, (int)n);
    EVP_EncryptFinal_ex(CTX, OUT + l, &l);
}
static void op_ossl_gcm(uint32_t n)
{
    uint8_t iv[12] = {0}; int l;
    memcpy(iv, &COUNT, 4); COUNT++;
    EVP_EncryptInit_ex(CTX, EVP_aes_256_gcm(), NULL, KEY, iv);
    EVP_EncryptUpdate(CTX, OUT, &l, IN, (int)n);
    EVP_EncryptFinal_ex(CTX, OUT + l, &l);
    EVP_CIPHER_CTX_ctrl(CTX, EVP_CTRL_GCM_GET_TAG, TAG, MAC);
}
/* ChaCha20-Poly1305 (RFC 8439): 256-bit key, 96-bit nonce, 16-byte tag. */
static void op_ossl_chacha(uint32_t n)
{
    uint8_t iv[12] = {0}; int l;
    memcpy(iv, &COUNT, 4); COUNT++;
    EVP_EncryptInit_ex(CTX, EVP_chacha20_poly1305(), NULL, KEY, iv);
    EVP_EncryptUpdate(CTX, OUT, &l, IN, (int)n);
    EVP_EncryptFinal_ex(CTX, OUT + l, &l);
    EVP_CIPHER_CTX_ctrl(CTX, EVP_CTRL_AEAD_GET_TAG, TAG, MAC);
}
#endif

#ifdef WITH_AEGIS
/* AEGIS-256 (RFC 10032) from libaegis 0.9.1: 256-bit key, 256-bit nonce, 16-byte tag.
 * libaegis picks AES-NI/VAES on x86-64 and the ARMv8 AES instructions on aarch64. */
#include "aegis.h"
#include "aegis256.h"
static void op_aegis256(uint32_t n)
{
    uint8_t npub[32] = {0};
    memcpy(npub, &COUNT, 4); COUNT++;
    aegis256_encrypt_detached(OUT, MAC, TAG, IN, n, NULL, 0, npub, KEY);
}
#endif

/* Known-answer tests of the reference schemes, run before every measurement. */
static int unhex(const char *h, uint8_t *o) { int n = 0; unsigned v; while (h[0] && h[1] && sscanf(h, "%2x", &v) == 1) { o[n++] = (uint8_t)v; h += 2; } return n; }
static int base_selftest(void)
{
    int bad = 0;
#ifdef WITH_OPENSSL
    {   /* RFC 8439, Section 2.8.2 */
        uint8_t k[32], iv[12], ad[12], c[114], t[16], et[16];
        const char *pt = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
        int l, n = (int)strlen(pt);
        for (int i = 0; i < 32; i++) k[i] = (uint8_t)(0x80 + i);
        unhex("070000004041424344454647", iv); unhex("50515253c0c1c2c3c4c5c6c7", ad);
        unhex("1ae10b594f09e26a7e902ecbd0600691", et);
        EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(x, EVP_chacha20_poly1305(), NULL, k, iv);
        EVP_EncryptUpdate(x, NULL, &l, ad, 12);
        EVP_EncryptUpdate(x, c, &l, (const uint8_t *)pt, n);
        EVP_EncryptFinal_ex(x, c + l, &l);
        EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_AEAD_GET_TAG, 16, t);
        EVP_CIPHER_CTX_free(x);
        int ok = (c[0] == 0xd3 && c[1] == 0x1a && memcmp(t, et, 16) == 0);
        fprintf(stderr, "KAT ChaCha20-Poly1305 (RFC 8439 2.8.2): %s\n", ok ? "ok" : "FAIL"); bad |= !ok;
    }
#endif
#ifdef WITH_AEGIS
    {   /* RFC 10032, Appendix A.3.2, test vector 1 */
        uint8_t k[32], npub[32], m[16] = {0}, c[16], t[16], ec[16], et[16];
        unhex("1001000000000000000000000000000000000000000000000000000000000000", k);
        unhex("1000020000000000000000000000000000000000000000000000000000000000", npub);
        unhex("754fc3d8c973246dcc6d741412a4b236", ec); unhex("3fe91994768b332ed7f570a19ec5896e", et);
        aegis256_encrypt_detached(c, t, 16, m, 16, NULL, 0, npub, k);
        int ok = memcmp(c, ec, 16) == 0 && memcmp(t, et, 16) == 0;
        fprintf(stderr, "KAT AEGIS-256 (RFC 10032 A.3.2 #1): %s\n", ok ? "ok" : "FAIL"); bad |= !ok;
    }
#endif
    return bad;
}

typedef struct { const char *level, *name, *set; op_fn fn; } alg_t;
static const alg_t ALGS[] = {
    {"L1", "256-NEA5", "AES-256", op_nea5}, {"L1", "256-NIA5", "AES-256", op_nia5}, {"L1", "256-NCA5", "AES-256", op_nca5},
    {"L1", "256-NEA4", "Snow 5G", op_nea4}, {"L1", "256-NIA4", "Snow 5G", op_nia4}, {"L1", "256-NCA4", "Snow 5G", op_nca4},
    {"L1", "256-NEA6", "ZUC-256", op_nea6}, {"L1", "256-NIA6", "ZUC-256", op_nia6}, {"L1", "256-NCA6", "ZUC-256", op_nca6},
#ifdef WITH_L2
    {"L2", "256-NEA5", "AES-256", op2_nea5}, {"L2", "256-NIA5", "AES-256", op2_nia5}, {"L2", "256-NCA5", "AES-256", op2_nca5},
    {"L2", "256-NEA4", "Snow 5G", op2_nea4}, {"L2", "256-NIA4", "Snow 5G", op2_nia4}, {"L2", "256-NCA4", "Snow 5G", op2_nca4},
    {"L2", "256-NEA6", "ZUC-256", op2_nea6}, {"L2", "256-NIA6", "ZUC-256", op2_nia6}, {"L2", "256-NCA6", "ZUC-256", op2_nca6},
    {"L2ct", "256-NEA6", "ZUC-256", op2c_nea6}, {"L2ct", "256-NIA6", "ZUC-256", op2c_nia6}, {"L2ct", "256-NCA6", "ZUC-256", op2c_nca6},
#endif
#ifdef WITH_L3
    {"L3", "256-NEA5", "AES-256", op3_nea5}, {"L3", "256-NIA5", "AES-256", op3_nia5}, {"L3", "256-NCA5", "AES-256", op3_nca5},
    {"L3", "256-NEA4", "Snow 5G", op3_nea4}, {"L3", "256-NIA4", "Snow 5G", op3_nia4}, {"L3", "256-NCA4", "Snow 5G", op3_nca4},
    {"L3", "256-NEA6", "ZUC-256", op3_nea6}, {"L3", "256-NIA6", "ZUC-256", op3_nia6}, {"L3", "256-NCA6", "ZUC-256", op3_nca6},
    {"L3ct", "256-NEA6", "ZUC-256", op3c_nea6}, {"L3ct", "256-NIA6", "ZUC-256", op3c_nia6}, {"L3ct", "256-NCA6", "ZUC-256", op3c_nca6},
#endif
#ifdef WITH_OPENSSL
    {"base", "OpenSSL-AES-256-CTR", "baseline", op_ossl_ctr}, {"base", "OpenSSL-AES-256-GCM", "baseline", op_ossl_gcm},
    {"base", "OpenSSL-ChaCha20-Poly1305", "baseline", op_ossl_chacha},
#endif
#ifdef WITH_AEGIS
    {"base", "libaegis-AEGIS-256", "baseline", op_aegis256},
#endif
};
#define NALGS (sizeof ALGS / sizeof ALGS[0])

/* ---- clocks ------------------------------------------------------------- */
static int PERF_FD = -1;

static void perf_open(void)
{
#ifdef __linux__
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof pe);
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof pe;
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    PERF_FD = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
#endif
}

static uint64_t cycles_now(void)
{
    uint64_t v = 0;
    if (PERF_FD >= 0 && read(PERF_FD, &v, sizeof v) != (ssize_t)sizeof v) v = 0;
    return v;
}

static uint64_t ns_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

static uint64_t tsc_now(void)
{
#ifdef HAVE_TSC
    return __rdtsc();
#else
    return 0;
#endif
}

/* ---- statistics --------------------------------------------------------- */
static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

typedef struct { double med, q1, q3, min; } stats_t;

static stats_t stats(double *v, int n)
{
    qsort(v, (size_t)n, sizeof *v, cmp_d);
    stats_t s = { v[n / 2], v[n / 4], v[(3 * n) / 4], v[0] };
    if (n % 2 == 0) s.med = 0.5 * (v[n / 2 - 1] + v[n / 2]);
    return s;
}

/* ---- one (algorithm, size) point --------------------------------------- */
static void batch(op_fn f, uint32_t bytes, uint64_t k, double *ns, double *cyc, double *tsc)
{
    uint64_t c0 = cycles_now(), t0 = tsc_now(), n0 = ns_now();
    for (uint64_t i = 0; i < k; i++) { f(bytes); SINK ^= OUT[0] ^ MAC[0]; }
    uint64_t n1 = ns_now(), t1 = tsc_now(), c1 = cycles_now();
    *ns = (double)(n1 - n0) / (double)k;
    *cyc = (double)(c1 - c0) / (double)k;
    *tsc = (double)(t1 - t0) / (double)k;
}

static void usage(void)
{
    fprintf(stderr,
        "usage: bench [options]\n"
        "  --platform NAME   platform label for the CSV (default: host name)\n"
        "  --reps N          timed batches per point (default 31)\n"
        "  --min-ms X        minimum batch length in ms (default 2)\n"
        "  --sizes a,b,...   payload sizes in bytes (default 0,16,40,64,128,256,576,1024,1500,4096,9000,16384,65536,262144,1048576)\n"
        "  --algs a,b,...    subset of algorithms, e.g. 256-NEA4,256-NCA6 (default all)\n"
        "  --levels a,b,...  subset of levels: L1, L2, L3, base (default all built in)\n"
        "  --cpu N           pin to this logical CPU (default: no pinning)\n"
        "  --out-offset N    output buffer position mod 4096 bytes (default 2048; input is at 0)\n"
        "  --run-seconds S   energy mode: run the one selected algorithm at the one size for S seconds\n"
        "                    and print calls, bytes and seconds (used by energy_pi.py)\n"
        "  -o FILE           CSV output (default stdout)\n");
}

int main(int argc, char **argv)
{
    char platform[64] = "", sizes_arg[512] = "0,16,40,64,128,256,576,1024,1500,4096,9000,16384,65536,262144,1048576";
    char algs_arg[512] = "", levels_arg[64] = "";
    int reps = 31, cpu = -1, out_offset = 2048;
    double min_ms = 2.0, run_seconds = 0;
    const char *outpath = NULL;

    static struct option lo[] = {
        {"platform", 1, 0, 'p'}, {"reps", 1, 0, 'r'}, {"min-ms", 1, 0, 'm'}, {"sizes", 1, 0, 's'},
        {"algs", 1, 0, 'a'}, {"levels", 1, 0, 'l'}, {"cpu", 1, 0, 'c'}, {"out-offset", 1, 0, 'f'}, {"run-seconds", 1, 0, 'u'}, {"help", 0, 0, 'h'}, {0, 0, 0, 0}};
    int ch;
    while ((ch = getopt_long(argc, argv, "o:h", lo, NULL)) != -1) {
        switch (ch) {
        case 'p': snprintf(platform, sizeof platform, "%s", optarg); break;
        case 'r': reps = atoi(optarg); break;
        case 'm': min_ms = atof(optarg); break;
        case 's': snprintf(sizes_arg, sizeof sizes_arg, "%s", optarg); break;
        case 'a': snprintf(algs_arg, sizeof algs_arg, "%s", optarg); break;
        case 'l': snprintf(levels_arg, sizeof levels_arg, "%s", optarg); break;
        case 'c': cpu = atoi(optarg); break;
        case 'f': out_offset = atoi(optarg) & 4095; break;
        case 'o': outpath = optarg; break;
        case 'u': run_seconds = atof(optarg); break;
        default: usage(); return ch == 'h' ? 0 : 2;
        }
    }
    if (reps < 5) reps = 5;

    struct utsname un;
    uname(&un);
    if (!platform[0]) snprintf(platform, sizeof platform, "%.63s", un.nodename);

    if (cpu >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if (sched_setaffinity(0, sizeof set, &set) != 0) {
            fprintf(stderr, "cannot pin to CPU %d: %s\n", cpu, strerror(errno));
            return 1;
        }
    }

    uint32_t sizes[64];
    int nsizes = 0;
    for (char *t = strtok(sizes_arg, ","); t && nsizes < 64; t = strtok(NULL, ",")) {
        long v = atol(t);
        if (v < 0 || v > (long)MAXBYTES) { fprintf(stderr, "size %ld out of range\n", v); return 2; }
        sizes[nsizes++] = (uint32_t)v;
    }

    /* Fixed buffer layout, the same in every build: the input starts on a page
     * boundary and the output sits out_offset bytes into a page. Leaving this
     * to malloc made the layout depend on the buffer size (small blocks come
     * from the heap, large ones from fresh pages), and on Zen 4 the relative
     * position of input and output changed L3 AES-256 results by up to 17%. */
    size_t span = ((size_t)MAXBYTES + 64 + 4095) & ~(size_t)4095;
    uint8_t *arena = aligned_alloc(4096, 2 * span + 4096);
    if (!arena) return 1;
    IN = arena;
    OUT = arena + span + (size_t)out_offset;
    srand(12345);
    for (uint32_t i = 0; i < MAXBYTES + 64; i++) IN[i] = (uint8_t)rand();
    for (int i = 0; i < 32; i++) KEY[i] = (uint8_t)rand();
    for (int i = 0; i < 64; i++) AAD[i] = (uint8_t)rand();
#ifdef WITH_OPENSSL
    CTX = EVP_CIPHER_CTX_new();
#endif
#ifdef WITH_AEGIS
    if (aegis_init() != 0) { fprintf(stderr, "aegis_init failed\n"); return 1; }
#endif
    if (base_selftest()) { fprintf(stderr, "reference-scheme self-test FAILED, no measurement\n"); return 3; }

    if (run_seconds > 0) {                 /* energy mode: one algorithm, one size, steady load */
        const alg_t *sel = NULL;
        for (size_t a = 0; a < NALGS; a++) {
            char tmp[520], key[64], ltmp[80], lkey[16];
            snprintf(tmp, sizeof tmp, ",%s,", algs_arg); snprintf(key, sizeof key, ",%s,", ALGS[a].name);
            snprintf(ltmp, sizeof ltmp, ",%s,", levels_arg); snprintf(lkey, sizeof lkey, ",%s,", ALGS[a].level);
            if (strstr(tmp, key) && strstr(ltmp, lkey)) { sel = &ALGS[a]; break; }
        }
        if (!sel || nsizes != 1) { fprintf(stderr, "--run-seconds needs one --algs, one --levels and one --sizes\n"); return 2; }
        uint32_t n = sizes[0];
        uint64_t calls = 0, t0 = ns_now(), t1 = t0, limit = (uint64_t)(run_seconds * 1e9);
        while (t1 - t0 < limit) {
            for (int i = 0; i < 64; i++) { sel->fn(n); SINK ^= OUT[0] ^ MAC[0]; }
            calls += 64;
            t1 = ns_now();
        }
        printf("level=%s alg=%s bytes=%u calls=%llu payload_bytes=%llu seconds=%.6f\n", sel->level, sel->name, n,
               (unsigned long long)calls, (unsigned long long)(calls * n), (double)(t1 - t0) / 1e9);
        return 0;
    }

    perf_open();
    const char *csrc = PERF_FD >= 0 ? "perf" : "none";
    if (PERF_FD < 0)
        fprintf(stderr, "note: perf cycle counter unavailable (try: sudo sysctl kernel.perf_event_paranoid=1); "
                        "reporting time only\n");

    FILE *out = outpath ? fopen(outpath, "w") : stdout;
    if (!out) { perror(outpath); return 1; }

    time_t now = time(NULL);
    char date[32];
    strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%S", localtime(&now));
    fprintf(out, "# R4 benchmark, %s\n# platform=%s levels=%s kernel=%s %s machine=%s\n",
            date, platform, levels_arg[0] ? levels_arg : "all", un.sysname, un.release, un.machine);
    fprintf(out, "# compiler=%s cflags=\"%s\"\n", __VERSION__, BENCH_CFLAGS);
    fprintf(out, "# cpu_pin=%d reps=%d min_batch_ms=%.2f cycle_source=%s tag_bytes=%d aad_bytes=0 in_mod4096=0 out_mod4096=%d\n",
            cpu, reps, min_ms, csrc, TAG, out_offset);
    fprintf(out, "platform,level,set,alg,bytes,iters,reps,med_ns,q1_ns,q3_ns,min_ns,"
                 "med_cycles,q1_cycles,q3_cycles,min_cycles,cpb,eff_ghz,med_tsc\n");

    double *vns = malloc(sizeof(double) * (size_t)reps), *vcy = malloc(sizeof(double) * (size_t)reps),
           *vts = malloc(sizeof(double) * (size_t)reps);

    fprintf(stderr, "%-27s", PERF_FD >= 0 ? "cycles/byte (median)" : "ns/byte (median)");
    for (int s = 0; s < nsizes; s++) if (sizes[s]) fprintf(stderr, " %7u", sizes[s]);
    fprintf(stderr, "   | setup (size 0)\n");

    for (size_t a = 0; a < NALGS; a++) {
        if (algs_arg[0]) {
            char tmp[520];
            snprintf(tmp, sizeof tmp, ",%s,", algs_arg);
            char key[64];
            snprintf(key, sizeof key, ",%s,", ALGS[a].name);
            if (!strstr(tmp, key)) continue;
        }
#ifdef WITH_L3
        if (!strcmp(ALGS[a].level, "L3") && !r4_l3_supported()) continue;   /* CPU lacks AVX-512/VAES */
#endif
#if defined(__x86_64__)
        if (strstr(ALGS[a].level, "ct") && !__builtin_cpu_supports("avx512vbmi")) continue;   /* needs VPERMI2B */
#endif
        if (levels_arg[0]) {
            char tmp[80], key[16];
            snprintf(tmp, sizeof tmp, ",%s,", levels_arg);
            snprintf(key, sizeof key, ",%s,", ALGS[a].level);
            if (!strstr(tmp, key)) continue;
        }
        fprintf(stderr, "%-4s %-22s", ALGS[a].level, ALGS[a].name);
        double setup_ns = -1, setup_cy = -1;
        for (int s = 0; s < nsizes; s++) {
            uint32_t n = sizes[s];
            double ns, cy, ts;
            uint64_t k = 1;
            for (;;) {                       /* calibrate on the fastest of 3 batches, */
                double best = 1e300;         /* so one interrupt cannot stop it early  */
                for (int t = 0; t < 3; t++) {
                    batch(ALGS[a].fn, n, k, &ns, &cy, &ts);
                    if (ns < best) best = ns;
                }
                if (best * (double)k >= min_ms * 1e6 || k >= (1ull << 30)) break;
                k *= 2;
            }
            for (int w = 0; w < 5; w++) batch(ALGS[a].fn, n, k, &ns, &cy, &ts);   /* warm-up */
            for (int r = 0; r < reps; r++) batch(ALGS[a].fn, n, k, &vns[r], &vcy[r], &vts[r]);
            stats_t sn = stats(vns, reps), sc = stats(vcy, reps), st = stats(vts, reps);

            fprintf(out, "%s,%s,%s,%s,%u,%llu,%d,%.2f,%.2f,%.2f,%.2f,", platform, ALGS[a].level, ALGS[a].set,
                    ALGS[a].name, n, (unsigned long long)k, reps, sn.med, sn.q1, sn.q3, sn.min);
            if (PERF_FD >= 0) {
                fprintf(out, "%.1f,%.1f,%.1f,%.1f,", sc.med, sc.q1, sc.q3, sc.min);
                if (n) fprintf(out, "%.3f,", sc.med / n); else fprintf(out, ",");
                fprintf(out, "%.3f,", sc.med / sn.med);
            } else {
                fprintf(out, ",,,,,,");
            }
            if (st.med > 0) fprintf(out, "%.1f\n", st.med); else fprintf(out, "\n");
            fflush(out);

            if (n == 0) { setup_ns = sn.med; setup_cy = sc.med; }
            else if (PERF_FD >= 0) fprintf(stderr, " %7.2f", sc.med / n);
            else fprintf(stderr, " %7.2f", sn.med / n);
        }
        if (setup_ns >= 0) {
            if (PERF_FD >= 0) fprintf(stderr, "   | %.0f cycles", setup_cy);
            fprintf(stderr, " (%.0f ns)", setup_ns);
        }
        fprintf(stderr, "\n");
    }
    if (out != stdout) fclose(out);
    return SINK == 0x5A ? 0 : 0;   /* use SINK so the work is not removed */
}
