#include "Utilities.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <random>

void* AlignedAllocate(const std::size_t size, const std::size_t alignment)
{
    std::size_t capacity = size + alignment - 1;
    void *ptr = new char[capacity];
    auto result = std::align(alignment, size, ptr, capacity);
    if (result == nullptr) throw std::bad_alloc();
    if (capacity < size) throw std::bad_alloc();
    return ptr;
}

void InitializeMatrices(float (&A)[DIM_M][DIM_K],float (&B)[DIM_K][DIM_N])
{
    std::random_device rd; std::mt19937 gen(rd());
    std::uniform_real_distribution<float> uniform_dist(-1., 1.);
    for (int i = 0; i < DIM_M; i++) {
        for (int j = 0; j < DIM_K; j++) {
            A[i][j] = uniform_dist(gen);
        }
    }
    for (int i = 0; i < DIM_K; i++) {
        for (int j = 0; j < DIM_N; j++) {
            B[i][j] = uniform_dist(gen);
        }
    }
}

float MatrixMaxDifference(const float (&C)[DIM_M][DIM_N], const float (&C_ref)[DIM_M][DIM_N])
{
    float result = 0.;
    for (int i = 0; i < DIM_M; i++)
    for (int j = 0; j < DIM_N; j++)
        result = std::max( result, std::abs( C[i][j] - C_ref[i][j] ) );
    return result;
}
