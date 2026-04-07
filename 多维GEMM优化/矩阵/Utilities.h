#pragma once

#include "Parameters.h"

#include <cstdlib>

void* AlignedAllocate(const std::size_t size, const std::size_t alignment);
void InitializeMatrices(float (&A)[DIM_M][DIM_K], float (&B)[DIM_K][DIM_N]);
float MatrixMaxDifference(const float (&C)[DIM_M][DIM_N], const float (&C_ref)[DIM_M][DIM_N]);
