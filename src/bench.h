#ifndef RECAP_BENCH_H
#define RECAP_BENCH_H

#include <stdint.h>
#include <stdio.h>

typedef struct bench_rec_s {
    const char* name;
    uint64_t count;
    double total_ms;
    double min_ms;
    double max_ms;
} bench_rec;

int bench_init(const char* outpath, int enabled, int sample_rate);
void bench_shutdown(void);

void bench_record_start(const char* name, void** out_cookie);
void bench_record_end(void* cookie);

void bench_record_ms(const char* name, double ms);

int bench_report(FILE* f);

int bench_is_enabled(void);

#endif
