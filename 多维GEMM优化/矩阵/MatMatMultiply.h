#pragma once

#include "Parameters.h"

void MatMatMultiply(const float (&A)[DIM_M][DIM_K],
    const float (&B)[DIM_K][DIM_N], float (&C)[DIM_M][DIM_N]);

void MatMatMultiplyReference(const float (&A)[DIM_M][DIM_K],
    const float (&B)[DIM_K][DIM_N], float (&C)[DIM_M][DIM_N]);
