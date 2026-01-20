# Cache-Aware Matrix Multiplication Optimization

This project explores performance optimization of a memory-intensive C matrix
multiplication by improving cache locality and data access patterns. The focus
is on understanding how different code expressions interact with the memory
hierarchy and compiler optimizations.

---

## Overview

The baseline implementation performs matrix multiplication with an unfavorable
memory access pattern that repeatedly traverses matrix B by column. This results
in poor cache utilization and long runtimes for large matrices.

Several optimized variants are implemented and evaluated to study the impact of
loop ordering, data layout, and cache blocking.

---

## Files

### Source Code
- `baseline.c`  
  Baseline matrix multiplication with column-wise access to matrix B.

- `opt1_reorder.c`  
  Optimized version using loop reordering (i–k–j order) to enable contiguous
  memory access and significantly reduce cache misses.

- `opt2_blocked.c`  
  Experimental implementations using matrix transposition and cache blocking to
  explore performance trade-offs.

### Executables (built locally)
- `baseline`  
  Compiled baseline executable.

- `opt1`  
  Loop-reordered optimized version.

- `opt2a`  
  Transpose-only version (no cache blocking), used as an ablation study to isolate
  the effect of transposed data layout.

- `opt2_32`, `opt2_64`  
  Transpose + cache-blocked versions using different block sizes.

---

## How to Build

```bash
gcc -O3 -march=native -Wall -Wextra -o baseline baseline.c
gcc -O3 -march=native -Wall -Wextra -o opt1 opt1_reorder.c

# transpose-only
gcc -O3 -march=native -Wall -Wextra -o opt2a opt2_blocked.c

# transpose + blocking (different block sizes)
gcc -O3 -march=native -Wall -Wextra -DBLOCK=32 -o opt2_32 opt2_blocked.c
gcc -O3 -march=native -Wall -Wextra -DBLOCK=64 -o opt2_64 opt2_blocked.c