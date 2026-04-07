#include "MatMatMultiply.h"
#include "mkl.h"

alignas(64) float localA[BLOCK_SIZE][BLOCK_SIZE];
alignas(64) float localB[BLOCK_SIZE][BLOCK_SIZE];
alignas(64) float localC[BLOCK_SIZE][BLOCK_SIZE];

#pragma omp threadprivate(localA, localB, localC)

void MatMatMultiply(const float (&A)[DIM_M][DIM_K],
    const float (&B)[DIM_K][DIM_N], float (&C)[DIM_M][DIM_N])
{
    static constexpr int MBLOCKS = DIM_M / BLOCK_SIZE;
    static constexpr int KBLOCKS = DIM_K / BLOCK_SIZE;
    static constexpr int NBLOCKS = DIM_N / BLOCK_SIZE;

    using blocked_A_t = float (&) [MBLOCKS][BLOCK_SIZE][KBLOCKS][BLOCK_SIZE];
    using blocked_B_t = float (&) [KBLOCKS][BLOCK_SIZE][NBLOCKS][BLOCK_SIZE];
    using blocked_C_t = float (&) [MBLOCKS][BLOCK_SIZE][NBLOCKS][BLOCK_SIZE];
    
    using const_blocked_A_t = const float (&) [MBLOCKS][BLOCK_SIZE][KBLOCKS][BLOCK_SIZE];
    using const_blocked_B_t = const float (&) [KBLOCKS][BLOCK_SIZE][NBLOCKS][BLOCK_SIZE];


    auto blockA = reinterpret_cast<const_blocked_A_t>(A[0][0]);
    auto blockB = reinterpret_cast<const_blocked_B_t>(B[0][0]);
    auto blockC = reinterpret_cast<blocked_C_t>(C[0][0]);

#pragma omp parallel for
    for (int bi = 0; bi < MBLOCKS; bi++)
    for (int bj = 0; bj < NBLOCKS; bj++) {
        
        for (int ii = 0; ii < BLOCK_SIZE; ii++)
            for (int jj = 0; jj < BLOCK_SIZE; jj++) {
                localC[ii][jj] = 0.;
            }

        for (int bk = 0; bk < KBLOCKS; bk++) { 

            for (int ii = 0; ii < BLOCK_SIZE; ii++)
            for (int jj = 0; jj < BLOCK_SIZE; jj++) {
                localA[ii][jj] = blockA[bi][ii][bk][jj];
                localB[ii][jj] = blockB[bk][ii][bj][jj];
            }

            for (int ii = 0; ii < BLOCK_SIZE; ii++)
                for (int kk = 0; kk < BLOCK_SIZE; kk++)
#pragma omp simd
                    for (int jj = 0; jj < BLOCK_SIZE; jj++)
                    localC[ii][jj] += localA[ii][kk] * localB[kk][jj];
        }

        for (int ii = 0; ii < BLOCK_SIZE; ii++)
        for (int jj = 0; jj < BLOCK_SIZE; jj++)                
            blockC[bi][ii][bj][jj] = localC[ii][jj];
    }
}

void MatMatMultiplyReference(const float (&A)[DIM_M][DIM_K],
    const float (&B)[DIM_K][DIM_N], float (&C)[DIM_M][DIM_N])
{
    cblas_sgemm(
        CblasRowMajor,
        CblasNoTrans,
        CblasNoTrans,
        DIM_M,
        DIM_N,
        DIM_K,
        1.,
        &A[0][0],
        DIM_K,
        &B[0][0],
        DIM_N,
        0.,
        &C[0][0],
        DIM_N
    );
}
