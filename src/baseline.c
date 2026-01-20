#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

static inline double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void fill(double *M, int N, unsigned seed) {
    srand(seed);
    for (long i = 0; i < (long)N * N; i++) {
        M[i] = (double) (rand() % 1000) / 1000.0;
    }
}

// Check
static double checksum(const double *M, int N) {
    double s = 0.0;
    for (long i = 0; i < (long)N*N; i += 97) s += M[i];
    return s; 
}

// Baseline: unfriendly visiting mode
// C = A * B
static void matmul_bad(const double *A, const double *B, double *C, int N) {
    // C == 0 (init)
    memset(C, 0, (size_t)N * N * sizeof(double));

    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            double sum = 0.0;
            for (int k = 0; k < N; k++) {
                // A: A[i][K]
                // B: B[K][j] visit by row
                sum += A[(long)i * N + k] * B[(long)k * N + j];
            }
            C[(long)i * N + j] = sum;
        }
    }
}

int main(int argc, char **argv) {
    int N = (argc >= 2) ? atoi(argv[1]) : 1024;

    size_t bytes = (size_t)N * N * sizeof(double);
    double *A = aligned_alloc(64, bytes);
    double *B = aligned_alloc(64, bytes);
    double *C = aligned_alloc(64, bytes);
    
    // Check allocate
    if (!A || !B || !C) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }

    fill(A, N, 1);
    fill(B, N, 2);

    matmul_bad(A, B, C, N);

    double t0 = now_sec();
    matmul_bad(A, B, C, N);
    double t1 = now_sec();

    printf("N=%d time=%.6f sec checksum=%.6f\n", N, (t1 - t0), checksum(C,N));

    free(A); free(B); free(C);
    return 0;
}
