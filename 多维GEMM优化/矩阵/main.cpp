#include "MatMatMultiply.h"
#include "Timer.h"
#include "Utilities.h"

#include <iostream>
#include <iomanip>

int main(int argc, char *argv[])
{
    float *Araw = static_cast<float*>( AlignedAllocate( DIM_M * DIM_K * sizeof(float), 64 ) );
    float *Braw = static_cast<float*>( AlignedAllocate( DIM_K * DIM_N * sizeof(float), 64 ) );
    float *Craw = static_cast<float*>( AlignedAllocate( DIM_M * DIM_N * sizeof(float), 64 ) );
    float *referenceCraw = static_cast<float*>( AlignedAllocate( DIM_M * DIM_N * sizeof(float), 64 ) );

    using matrix_A_t = float (&) [DIM_M][DIM_K];
    using matrix_B_t = float (&) [DIM_K][DIM_N];
    using matrix_C_t = float (&) [DIM_M][DIM_N];

    matrix_A_t A = reinterpret_cast<matrix_A_t>(*Araw);
    matrix_B_t B = reinterpret_cast<matrix_B_t>(*Braw);
    matrix_C_t C = reinterpret_cast<matrix_C_t>(*Craw);
    matrix_C_t referenceC = reinterpret_cast<matrix_C_t>(*referenceCraw);

    InitializeMatrices(A, B);
    Timer timer;

    std::cout << "Testing Dimensions: M=" << DIM_M << ", K=" << DIM_K << ", N=" << DIM_N << std::endl;

    // Correctness test
    std::cout << "Running candidate kernel for correctness test ... " << std::flush;
    timer.Start();
    MatMatMultiply(A, B, C);
    timer.Stop("Elapsed time : ");
    
    std::cout << "Running reference kernel for correctness test ... " << std::flush;
    timer.Start();
    MatMatMultiplyReference(A, B, referenceC);
    timer.Stop("Elapsed time : ");

    float discrepancy = MatrixMaxDifference(C, referenceC);
    std::cout << "Discrepancy between two methods : " << discrepancy << std::endl;
    
    for(int test = 1; test <= 20; test++)
    {
        std::cout << "Running kernel for performance run #" << std::setw(2) << test << " ... ";
        timer.Start();
        MatMatMultiply(A, B, C);
        timer.Stop("Elapsed time : ");
    }
    
    return 0;
}
