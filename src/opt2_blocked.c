#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#ifndef BLOCK
#define BLOCK 32
#endif

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

static void transpose(const double *B, double *BT, int N) {
      for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
          BT[(long)j * N + i] = B[(long)i * N + j];
        }
      }
    }

// Baseline: unfriendly visiting mode
// C = A * B
static void matmul_blocked_BT(const double *A, const double *BT, double *C, int N) {
    // C == 0 (init)
    memset(C, 0, (size_t)N * N * sizeof(double));

    for (int ii = 0; ii < N; ii += BLOCK) {
        for (int jj = 0; jj < N; jj += BLOCK) {
            for (int kk = 0; kk < N; kk += BLOCK) {
                int i_max = (ii + BLOCK < N) ? (ii + BLOCK) : N;
                int j_max = (jj + BLOCK < N) ? (jj + BLOCK) : N;
                int k_max = (kk + BLOCK < N) ? (kk + BLOCK) : N;

                for (int i = ii; i < i_max; i++) {
                    const double *Ai = &A[(long)i * N];
                    double *Ci = &C[(long)i * N];
                    for (int j = jj; j < j_max; j++) {
                        const double *BTj = &BT[(long)j * N];
                        double sum  = Ci[j];
                        for (int k = kk; k < k_max; k++) {
                            sum += Ai[k] * BTj[k];
                        }
                        Ci[j] = sum;
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    int N = (argc >= 2) ? atoi(argv[1]) : 1024;

    size_t bytes = (size_t)N * N * sizeof(double);
    double *A = aligned_alloc(64, bytes);
    double *B = aligned_alloc(64, bytes);
    double *C = aligned_alloc(64, bytes);
    double *BT = aligned_alloc(64, bytes);
    
    // Check allocate
    if (!A || !B || !C || !BT) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }

    fill(A, N, 1);
    fill(B, N, 2);
    transpose(B, BT, N);

    matmul_blocked_BT(A, BT, C, N);

    double t0 = now_sec();
    matmul_blocked_BT(A, BT, C, N);
    double t1 = now_sec();

    printf("N=%d time=%.6f sec checksum=%.6f\n", N, (t1 - t0), checksum(C,N));

    free(A); free(B); free(C); free(BT);
    return 0;
}