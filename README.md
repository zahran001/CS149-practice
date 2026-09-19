# CS149 — Throughput Computing

Implementations of `sin(x)` via Taylor series, used to compare scalar, multi-threaded,
and SIMD execution.

All commands assume **gcc (MSYS2 / MinGW-w64)** on Windows PowerShell, run from the
repository root. gcc appends `.exe` automatically.

## Files

| File | Description |
|---|---|
| [sinx_single_thread.c](sinx_single_thread.c) | Scalar baseline, single thread |
| [sinx_multi_thread.c](sinx_multi_thread.c) | Splits the array across 2 pthreads |
| [sinx_avx2.c](sinx_avx2.c) | Hand-written AVX intrinsics, 8 floats per iteration |

## Build and run

**Single-threaded scalar baseline**

```powershell
gcc -O3 -fno-tree-vectorize -std=c23 -o single_thread sinx_single_thread.c ; .\single_thread.exe
```

**Multi-threaded (pthreads), scalar**

```powershell
gcc -O3 -fno-tree-vectorize -pthread -std=c23 -o multi_thread sinx_multi_thread.c ; .\multi_thread.exe
```

**AVX SIMD intrinsics**

```powershell
gcc -O3 -march=native -std=c23 -o sinx_avx sinx_avx2.c ; .\sinx_avx.exe
```

## Flags

| Flag | Why |
|---|---|
| `-O3` | Optimization; without it the comparisons are meaningless |
| `-fno-tree-vectorize` | Disables auto-vectorization so the scalar and threaded builds are genuinely scalar. Without it gcc emits 4-wide SSE even with no `-march` flag |
| `-march=native` | Enables AVX2 on this CPU. **Required** for `sinx_avx2.c`, which uses `__m256` intrinsics |
| `-pthread` | Correct pthread linking. MSYS2 links winpthreads implicitly, but this is the portable form |
| `-std=c23` | Needed for the `1'000'000'000` digit separators in the C1 config |

`-fno-tree-vectorize` only disables *auto*-vectorization. It has no effect on the
hand-written intrinsics in `sinx_avx2.c`, which is why that build does not use it.

## Confirming SIMD is actually off

**Method A — ask the compiler what it vectorized.** Compiles to `nul` and prints a report:

```powershell
gcc -O3 -fno-tree-vectorize -std=c23 -fopt-info-vec -S -o nul sinx_single_thread.c
```

Silence means no loop was vectorized. Drop `-fno-tree-vectorize` and it reports instead:

```
sinx_single_thread.c:8:23: optimized: loop vectorized using 16 byte vectors and unroll factor 4
```

16-byte vectors = 128-bit = SSE, 4 floats at a time. (With `-march=native` this becomes
32 byte vectors = AVX2, 8 floats.)

**Method B — count packed instructions in the assembly.** Scalar SSE uses the `ss`
(scalar single) suffix; vector uses `ps` (packed single):

```powershell
(gcc -O3 -fno-tree-vectorize -std=c23 -S -o - sinx_single_thread.c | Select-String -Pattern 'divps|mulps|addps').Count
```

| Build | `-fopt-info-vec` | packed count |
|---|---|---|
| `-O3` | reports 2 vectorized loops | 32 |
| `-O3 -fno-tree-vectorize` | silent | **0** |

**Method C — the runtime should get slower.** On this machine at the C2 size the scalar
build is ~3x slower (0.000083 s → 0.000261 s). If disabling SIMD changes nothing, it
was not enabled to begin with.

## Switching problem size (C1 / C2)

`sinx_single_thread.c` and `sinx_multi_thread.c` each contain two configurations in
`main()`. Uncomment one and comment out the other, then rebuild:

```c
// C1  — large, timing-dominated
// int N = 1'000'000'000;
// int terms = 5;

// C2  — small, fits in cache
int N = 10000;
int terms = 10;
```

`sinx_avx2.c` uses a fixed `N = 10000000` (must stay a multiple of 8).

## Notes on measuring

- **C1 allocates ~8 GB** (two 4 GB float arrays). With less free RAM than that the timings
  are dominated by paging and swing by 2x or more between runs. Prefer `N = 2e8`.
- **Timing a single run is unreliable.** Take the best of several.
- `y` is never read back, so the compiler is free to delete the stores. Checksum it after
  the timer if you change the kernel.
- The multi-threaded version can measure *slower* than the serial one. This is a
  measurement artifact, not a threading cost: the serial build inlines `sinx` into `main`
  and constant-folds `terms`, unrolling the inner loop, while the threaded build receives
  `terms` from a struct at runtime and gets the generic loop. Making `terms` a
  compile-time constant on both paths removes the gap.
