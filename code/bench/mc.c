/*
 * mc.c - multi-core scaling of the R4 algorithms (Linux: Zen 4, Raspberry Pi 5).
 *
 * N threads, each pinned to its own logical CPU, encrypt packets of one size
 * with one algorithm for a fixed time. Each thread has its own key, buffers,
 * counter and library context, so the threads share nothing but the code and
 * the read-only tables. We report the summed throughput and the clock that
 * each thread saw (core cycles / wall time).
 *
 *   ./mc --cpus 2,3,4,5 [--seconds 2] [--reps 3] [--sizes 1500,65536]
 *        [--algs L3:256-NCA5,base:OpenSSL-AES-256-GCM] [--platform zen4] [-o out.csv]
 *
 * Before timing, every thread computes a fixed packet and must reproduce the
 * tag of the main thread; otherwise the program stops (exit 3).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
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

#define TAG 16
#define MAXBYTES 65536u
#define MAXT 64

typedef struct {
    uint8_t *in, *out, key[32], xiv[6], mac[TAG];
    uint32_t count;
#ifdef WITH_OPENSSL
    EVP_CIPHER_CTX *ctx;
    EVP_CIPHER *gcm, *chacha;                 /* fetched once per thread (OpenSSL 3 explicit fetch) */
#endif
} tctx;

typedef void (*op_fn)(tctx *t, uint32_t n);

/* ---- the operations, one per algorithm (same calls as bench.c) ---------- */
#if defined(WITH_L2) && !defined(WITH_L3)
static void o2_nca5(tctx *t, uint32_t n) { nca5_256_l2(0, t->count++, 3, 1, t->xiv, t->key, NULL, 0, t->in, n * 8, t->out, TAG, t->mac); }
static void o2_nca4(tctx *t, uint32_t n) { nca4_256_l2(0, t->count++, 3, 1, t->xiv, t->key, NULL, 0, t->in, n * 8, t->out, TAG, t->mac); }
static void o2_nca6(tctx *t, uint32_t n) { nca6_256_l2(0, t->count++, 3, 1, t->xiv, t->key, NULL, 0, t->in, n * 8, t->out, TAG, t->mac); }
#endif
#ifdef WITH_L3
static void o3_nca5(tctx *t, uint32_t n) { nca5_256_l3(0, t->count++, 3, 1, t->xiv, t->key, NULL, 0, t->in, n * 8, t->out, TAG, t->mac); }
static void o3_nca4(tctx *t, uint32_t n) { nca4_256_l3(0, t->count++, 3, 1, t->xiv, t->key, NULL, 0, t->in, n * 8, t->out, TAG, t->mac); }
static void o3_nca6(tctx *t, uint32_t n) { nca6_256_l3(0, t->count++, 3, 1, t->xiv, t->key, NULL, 0, t->in, n * 8, t->out, TAG, t->mac); }
#endif
#ifdef WITH_OPENSSL
static void o_gcm(tctx *t, uint32_t n)
{
    uint8_t iv[12] = {0}; int l;
    memcpy(iv, &t->count, 4); t->count++;
    EVP_EncryptInit_ex(t->ctx, EVP_aes_256_gcm(), NULL, t->key, iv);
    EVP_EncryptUpdate(t->ctx, t->out, &l, t->in, (int)n);
    EVP_EncryptFinal_ex(t->ctx, t->out + l, &l);
    EVP_CIPHER_CTX_ctrl(t->ctx, EVP_CTRL_GCM_GET_TAG, TAG, t->mac);
}
static void o_chacha(tctx *t, uint32_t n)
{
    uint8_t iv[12] = {0}; int l;
    memcpy(iv, &t->count, 4); t->count++;
    EVP_EncryptInit_ex(t->ctx, EVP_chacha20_poly1305(), NULL, t->key, iv);
    EVP_EncryptUpdate(t->ctx, t->out, &l, t->in, (int)n);
    EVP_EncryptFinal_ex(t->ctx, t->out + l, &l);
    EVP_CIPHER_CTX_ctrl(t->ctx, EVP_CTRL_AEAD_GET_TAG, TAG, t->mac);
}
/* Same operations, but with a cipher object fetched once per thread instead of the
 * implicit fetch of EVP_aes_256_gcm() / EVP_chacha20_poly1305() on every packet.
 * The key is still set up once per packet. */
static void o_gcm_f(tctx *t, uint32_t n)
{
    uint8_t iv[12] = {0}; int l;
    memcpy(iv, &t->count, 4); t->count++;
    EVP_EncryptInit_ex2(t->ctx, t->gcm, t->key, iv, NULL);
    EVP_EncryptUpdate(t->ctx, t->out, &l, t->in, (int)n);
    EVP_EncryptFinal_ex(t->ctx, t->out + l, &l);
    EVP_CIPHER_CTX_ctrl(t->ctx, EVP_CTRL_GCM_GET_TAG, TAG, t->mac);
}
static void o_chacha_f(tctx *t, uint32_t n)
{
    uint8_t iv[12] = {0}; int l;
    memcpy(iv, &t->count, 4); t->count++;
    EVP_EncryptInit_ex2(t->ctx, t->chacha, t->key, iv, NULL);
    EVP_EncryptUpdate(t->ctx, t->out, &l, t->in, (int)n);
    EVP_EncryptFinal_ex(t->ctx, t->out + l, &l);
    EVP_CIPHER_CTX_ctrl(t->ctx, EVP_CTRL_AEAD_GET_TAG, TAG, t->mac);
}
#endif
#ifdef WITH_AEGIS
static void o_aegis(tctx *t, uint32_t n)
{
    uint8_t npub[32] = {0};
    memcpy(npub, &t->count, 4); t->count++;
    aegis256_encrypt_detached(t->out, t->mac, TAG, t->in, n, NULL, 0, npub, t->key);
}
#endif

typedef struct { const char *level, *name; op_fn fn; } alg_t;
static const alg_t ALGS[] = {
#ifdef WITH_L3
    {"L3", "256-NCA5", o3_nca5}, {"L3", "256-NCA4", o3_nca4}, {"L3", "256-NCA6", o3_nca6},
#elif defined(WITH_L2)
    {"L2", "256-NCA5", o2_nca5}, {"L2", "256-NCA4", o2_nca4}, {"L2", "256-NCA6", o2_nca6},
#endif
#ifdef WITH_OPENSSL
    {"base", "OpenSSL-AES-256-GCM", o_gcm}, {"base", "OpenSSL-ChaCha20-Poly1305", o_chacha},
    {"base", "OpenSSL-AES-256-GCM-fetch", o_gcm_f}, {"base", "OpenSSL-ChaCha20-Poly1305-fetch", o_chacha_f},
#endif
#ifdef WITH_AEGIS
    {"base", "libaegis-AEGIS-256", o_aegis},
#endif
};
#define NALGS (sizeof ALGS / sizeof ALGS[0])

/* ---- shared run state ---------------------------------------------------- */
static int NT, CPUS[MAXT];
static tctx T[MAXT];
static const alg_t *CUR;
static uint32_t CURN;
static atomic_int PHASE;                      /* 0 warm-up, 1 timed, 2 stop */
static pthread_barrier_t BAR;
typedef struct { uint64_t calls, cycles; double sec; int pin_ok; } tres;
static tres R[MAXT];
static uint8_t REFTAG[TAG];
static int CHECK_BAD;

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}

static int perf_open_self(void)
{
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof pe);
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof pe;
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    return (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);   /* this thread, any CPU */
}
static uint64_t perf_read(int fd)
{
    uint64_t v = 0;
    if (fd < 0 || read(fd, &v, sizeof v) != (ssize_t)sizeof v) v = 0;
    return v;
}

static void *worker(void *arg)
{
    int id = (int)(intptr_t)arg;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(CPUS[id], &cs);
    R[id].pin_ok = pthread_setaffinity_np(pthread_self(), sizeof cs, &cs) == 0;
    int fd = perf_open_self();
    tctx *t = &T[id];
    uint32_t n = CURN;

    pthread_barrier_wait(&BAR);
    while (atomic_load_explicit(&PHASE, memory_order_relaxed) == 0) CUR->fn(t, n);   /* warm-up */
    uint64_t c0 = perf_read(fd), k = 0;
    double t0 = now();
    while (atomic_load_explicit(&PHASE, memory_order_relaxed) == 1) { CUR->fn(t, n); k++; }
    double t1 = now();
    uint64_t c1 = perf_read(fd);
    R[id].calls = k;
    R[id].sec = t1 - t0;
    R[id].cycles = c1 - c0;
    if (fd >= 0) close(fd);
    return NULL;
}

/* Every thread computes packet 0 of the current algorithm; all tags must agree. */
static void *checker(void *arg)
{
    int id = (int)(intptr_t)arg;
    tctx *t = &T[id];
    uint32_t keep = t->count;
    t->count = 7;
    CUR->fn(t, 1500);
    if (memcmp(t->mac, REFTAG, TAG) != 0) CHECK_BAD = 1;
    t->count = keep;
    return NULL;
}

static int parse_list(const char *s, int *v, int max)
{
    int n = 0;
    while (*s && n < max) { v[n++] = atoi(s); while (*s && *s != ',') s++; if (*s == ',') s++; }
    return n;
}

int main(int argc, char **argv)
{
    char cpus_arg[512] = "", algs_arg[512] = "", sizes_arg[128] = "1500,65536", plat[64] = "linux", *outp = NULL;
    double secs = 2.0, warm = 0.3;
    int reps = 3;
    static struct option lo[] = {{"cpus", 1, 0, 'c'}, {"algs", 1, 0, 'a'}, {"sizes", 1, 0, 's'}, {"seconds", 1, 0, 't'},
                                 {"reps", 1, 0, 'r'}, {"platform", 1, 0, 'p'}, {"list", 0, 0, 'l'}, {0, 0, 0, 0}};
    int o;
    while ((o = getopt_long(argc, argv, "o:", lo, NULL)) != -1) {
        switch (o) {
        case 'c': snprintf(cpus_arg, sizeof cpus_arg, "%s", optarg); break;
        case 'a': snprintf(algs_arg, sizeof algs_arg, "%s", optarg); break;
        case 's': snprintf(sizes_arg, sizeof sizes_arg, "%s", optarg); break;
        case 't': secs = atof(optarg); break;
        case 'r': reps = atoi(optarg); break;
        case 'p': snprintf(plat, sizeof plat, "%s", optarg); break;
        case 'o': outp = optarg; break;
        case 'l': for (size_t a = 0; a < NALGS; a++) printf("%s:%s\n", ALGS[a].level, ALGS[a].name); return 0;
        default: fprintf(stderr, "usage: %s --cpus a,b,... [--algs L:name,...] [--sizes ..] [--seconds S] [--reps R] [-o csv]\n", argv[0]); return 2;
        }
    }
    NT = parse_list(cpus_arg, CPUS, MAXT);
    if (NT < 1) { fprintf(stderr, "--cpus is required\n"); return 2; }
    int sizes[16], ns = parse_list(sizes_arg, sizes, 16);
    for (int i = 0; i < ns; i++) if (sizes[i] < 1 || (uint32_t)sizes[i] > MAXBYTES) { fprintf(stderr, "size out of range\n"); return 2; }

#ifdef WITH_AEGIS
    if (aegis_init() != 0) return 1;
#endif
    srand(12345);
    uint8_t key[32];
    for (int i = 0; i < 32; i++) key[i] = (uint8_t)rand();
    for (int i = 0; i < NT; i++) {
        uint8_t *arena = aligned_alloc(4096, 2 * (MAXBYTES + 4096) + 4096);   /* own pages per thread */
        if (!arena) return 1;
        T[i].in = arena;
        T[i].out = arena + MAXBYTES + 4096 + 2048;                           /* output at 2048 mod 4096, as in bench */
        for (uint32_t j = 0; j < MAXBYTES; j++) T[i].in[j] = (uint8_t)(j * 131 + 7);
        memcpy(T[i].key, key, 32);
        for (int j = 0; j < 6; j++) T[i].xiv[j] = (uint8_t)(j + 1);
        T[i].count = (uint32_t)i << 24;
#ifdef WITH_OPENSSL
        T[i].ctx = EVP_CIPHER_CTX_new();
        T[i].gcm = EVP_CIPHER_fetch(NULL, "AES-256-GCM", NULL);
        T[i].chacha = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
        if (!T[i].gcm || !T[i].chacha) { fprintf(stderr, "EVP_CIPHER_fetch failed\n"); return 1; }
#endif
    }

    char cpus_csv[512];                                   /* CPU list for the CSV, with '+' instead of ',' */
    snprintf(cpus_csv, sizeof cpus_csv, "%s", cpus_arg);
    for (char *q = cpus_csv; *q; q++) if (*q == ',') *q = '+';
    FILE *out = outp ? fopen(outp, "a") : stdout;
    if (!out) { perror(outp); return 1; }
    if (outp && ftell(out) == 0)
        fprintf(out, "platform,level,alg,bytes,threads,cpus,rep,seconds,calls,gbps_total,gbps_per_thread,eff_ghz_min,eff_ghz_max,cycles_per_byte_mean\n");

    for (size_t a = 0; a < NALGS; a++) {
        if (algs_arg[0]) {
            char key2[96], tmp[520];
            snprintf(key2, sizeof key2, ",%s:%s,", ALGS[a].level, ALGS[a].name);
            snprintf(tmp, sizeof tmp, ",%s,", algs_arg);
            if (!strstr(tmp, key2)) continue;
        }
        CUR = &ALGS[a];
        /* self-check: main thread computes the reference tag, then every worker repeats it */
        {
            tctx ref = T[0];
            uint8_t *rin = aligned_alloc(4096, MAXBYTES + 4096), *rout = aligned_alloc(4096, MAXBYTES + 4096);
            memcpy(rin, T[0].in, MAXBYTES);
            ref.in = rin; ref.out = rout; ref.count = 7;
#ifdef WITH_OPENSSL
            ref.ctx = EVP_CIPHER_CTX_new();
            ref.gcm = T[0].gcm; ref.chacha = T[0].chacha;
#endif
            CUR->fn(&ref, 1500);
            memcpy(REFTAG, ref.mac, TAG);
#ifdef WITH_OPENSSL
            EVP_CIPHER_CTX_free(ref.ctx);
#endif
            free(rin); free(rout);
            pthread_t th[MAXT];
            CHECK_BAD = 0;
            for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, checker, (void *)(intptr_t)i);
            for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
            if (CHECK_BAD) { fprintf(stderr, "thread self-check FAILED for %s\n", CUR->name); return 3; }
        }
        for (int s = 0; s < ns; s++) {
            CURN = (uint32_t)sizes[s];
            for (int r = 0; r < reps; r++) {
                pthread_t th[MAXT];
                pthread_barrier_init(&BAR, NULL, (unsigned)NT + 1);
                atomic_store(&PHASE, 0);
                for (int i = 0; i < NT; i++) pthread_create(&th[i], NULL, worker, (void *)(intptr_t)i);
                pthread_barrier_wait(&BAR);
                struct timespec w = {(time_t)warm, (long)((warm - (int)warm) * 1e9)};
                nanosleep(&w, NULL);
                atomic_store(&PHASE, 1);
                struct timespec d = {(time_t)secs, (long)((secs - (int)secs) * 1e9)};
                nanosleep(&d, NULL);
                atomic_store(&PHASE, 2);
                for (int i = 0; i < NT; i++) pthread_join(th[i], NULL);
                pthread_barrier_destroy(&BAR);

                double gb = 0, gmin = 1e9, gmax = 0, cpb = 0, smax = 0;
                uint64_t calls = 0;
                for (int i = 0; i < NT; i++) {
                    if (!R[i].pin_ok) { fprintf(stderr, "could not pin to CPU %d\n", CPUS[i]); return 1; }
                    double bytes = (double)R[i].calls * CURN;
                    gb += bytes * 8 / R[i].sec / 1e9;
                    double g = (double)R[i].cycles / R[i].sec / 1e9;
                    if (g < gmin) gmin = g;
                    if (g > gmax) gmax = g;
                    cpb += bytes > 0 ? (double)R[i].cycles / bytes : 0;
                    calls += R[i].calls;
                    if (R[i].sec > smax) smax = R[i].sec;
                }
                fprintf(out, "%s,%s,%s,%u,%d,%s,%d,%.3f,%llu,%.3f,%.3f,%.3f,%.3f,%.4f\n", plat, CUR->level, CUR->name, CURN,
                        NT, cpus_csv, r, smax, (unsigned long long)calls, gb, gb / NT, gmin, gmax, cpb / NT);
                fflush(out);
                fprintf(stderr, "%-4s %-26s %6u B  %2d thr  rep %d  %8.2f Gbit/s  clock %.2f-%.2f GHz\n", CUR->level, CUR->name,
                        CURN, NT, r, gb, gmin, gmax);
            }
        }
    }
    if (outp) fclose(out);
    return 0;
}
