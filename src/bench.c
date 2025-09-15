#define _POSIX_C_SOURCE 200809L
#include "bench.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BENCH_RECORDS 256

static bench_rec g_recs[MAX_BENCH_RECORDS];
static int g_rec_count = 0;
static int g_enabled = 0;
static int g_sample_rate = 1;
static char g_outpath[4096] = "";

typedef struct cookie_s {
    struct timespec start;
    const char* name;
} cookie;

int bench_init(const char* outpath, int enabled, int sample_rate) {
    g_enabled = enabled ? 1 : 0;
    if (outpath && outpath[0]) {
        strncpy(g_outpath, outpath, sizeof(g_outpath) - 1);
        g_outpath[sizeof(g_outpath) - 1] = '\0';
    }
    g_sample_rate = sample_rate > 0 ? sample_rate : 1;
    g_rec_count = 0;
    for (int i = 0; i < MAX_BENCH_RECORDS; i++) {
        g_recs[i].name = NULL;
        g_recs[i].count = 0;
        g_recs[i].total_ms = 0.0;
        g_recs[i].min_ms = 0.0;
        g_recs[i].max_ms = 0.0;
    }
    return 0;
}

void bench_shutdown(void) {
    if (!g_enabled) return;
    FILE* f = stdout;
    if (g_outpath[0]) {
        FILE* nf = fopen(g_outpath, "w");
        if (nf) f = nf;
    }
    bench_report(f);
    if (f != stdout) fclose(f);
}

static bench_rec* find_or_create_rec(const char* name) {
    for (int i = 0; i < g_rec_count; i++) {
        if (strcmp(g_recs[i].name, name) == 0) return &g_recs[i];
    }
    if (g_rec_count >= MAX_BENCH_RECORDS) return NULL;
    bench_rec* r = &g_recs[g_rec_count++];
    r->name = strdup(name);
    r->count = 0;
    r->total_ms = 0.0;
    r->min_ms = 0.0;
    r->max_ms = 0.0;
    return r;
}

void bench_record_start(const char* name, void** out_cookie) {
    if (!g_enabled) {
        if (out_cookie) *out_cookie = NULL;
        return;
    }
    cookie* c = malloc(sizeof(cookie));
    if (!c) {
        if (out_cookie) *out_cookie = NULL;
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &c->start);
    c->name = name;
    if (out_cookie) *out_cookie = c;
}

void bench_record_end(void* cookie_ptr) {
    if (!g_enabled || !cookie_ptr) return;
    cookie* c = (cookie*)cookie_ptr;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double ms = (end.tv_sec - c->start.tv_sec) * 1000.0 + (end.tv_nsec - c->start.tv_nsec) / 1e6;
    bench_record_ms(c->name, ms);
    free(c);
}

void bench_record_ms(const char* name, double ms) {
    if (!g_enabled) return;
    bench_rec* r = find_or_create_rec(name);
    if (!r) return;
    r->count++;
    r->total_ms += ms;
    if (r->count == 1) {
        r->min_ms = ms;
        r->max_ms = ms;
    }
    else {
        if (ms < r->min_ms) r->min_ms = ms;
        if (ms > r->max_ms) r->max_ms = ms;
    }
}

int bench_report(FILE* f) {
    if (!g_enabled) return -1;
    if (!f) f = stdout;
    fprintf(f, "name,count,total_ms,avg_ms,min_ms,max_ms\n");
    for (int i = 0; i < g_rec_count; i++) {
        bench_rec* r = &g_recs[i];
        double avg = r->count ? (r->total_ms / r->count) : 0.0;
        fprintf(f, "%s,%llu,%.3f,%.3f,%.3f,%.3f\n", r->name, (unsigned long long)r->count, r->total_ms, avg, r->min_ms, r->max_ms);
    }
    return 0;
}

int bench_is_enabled(void) { return g_enabled; }
