#define _POSIX_C_SOURCE 199309L
#include "../slog.h" 
#include <stdio.h>
#include <time.h>

#define ITERATIONS 1000000

void fake_print(const char *buffer) {
}

double get_elapsed_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) +
           (end.tv_nsec - start.tv_nsec) / 1e9;
}

void bench_simple() {
    struct timespec start, end;

    fprintf(stdout, "Benchmarking SIMPLE log (1M iterations)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        SLOG(SLOG_INFO, "bench_simple", SLOG_INT("id", i));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = get_elapsed_time(start, end);

    fprintf(stdout, "Simple Log: %.4f seconds | %.0f Ops/sec\n", elapsed,
            ITERATIONS / elapsed);
}

void bench_complex() {
    struct timespec start, end;

    fprintf(stdout, "Benchmarking COMPLEX log (1M iterations)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    const char *user = "a";

    for (int i = 0; i < ITERATIONS; i++) {
        SLOG(SLOG_INFO, "user_action", SLOG_STRING("username", user),
             SLOG_INT("session_id", i), SLOG_BOOL("is_active", true),
             SLOG_OBJECT("meta", SLOG_STRING("ip", "127.0.0.1"),
                 SLOG_FLOAT("load", 0.45)));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = get_elapsed_time(start, end);

    fprintf(stdout, "Complex Log: %.4f seconds | %.0f Ops/sec\n", elapsed,
            ITERATIONS / elapsed);
}

int main() {
    SLOG_SET_HANDLER(fake_print);

    bench_simple();
    fprintf(stdout, "--------------------------------\n");
    bench_complex();

    return 0;
}
