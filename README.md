# CS149 - PARALLEL COMPUTING

Implementations of `sin(x)` via Taylor series, used to compare scalar, multi-threaded,
and SIMD execution.

All commands assume **gcc (MSYS2 / MinGW-w64)** on Windows PowerShell, run from the
repository root. gcc appends `.exe` automatically.

## Files

| File | Description |
|---|---|
| [sinx_single_thread.c](sinx_single_thread.c) | Scalar baseline, single thread |
| [sinx_multi_thread.c](sinx_multi_thread.c) | Splits the array across 2 pthreads |
| [sinx_avx2.c](sinx_avx2.c) | AVX intrinsics, 8 floats per iteration |
| [thread_launch.cpp](thread_launch.cpp) | Thread launch overhead: serial calls vs one thread per task vs a thread pool |
| [ispc/main.cpp](ispc/main.cpp) | ISPC correctness check against `std::sin` |
| [ispc/bench.cpp](ispc/bench.cpp) | Times all three ISPC variants vs a scalar reference |

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

**Thread launch overhead**

```powershell
g++ -O3 -std=c++23 thread_launch.cpp -o thread_launch -pthread ; .\thread_launch.exe
```

**ISPC correctness check** (`ispc/main.cpp`)

```powershell
cd ispc ; ispc -O2 --arch=x86-64 --target=avx2-i32x8 -h sinx_interleaved.h -o sinx_interleaved.obj sinx_interleaved.ispc ; g++ -O3 -std=c++17 -o ispc_demo main.cpp sinx_interleaved.obj ; .\ispc_demo.exe
```

**ISPC three-way benchmark** (`ispc/bench.cpp`)

```powershell
cd ispc ; foreach ($v in 'interleaved','blocked','foreach') { ispc -O2 --arch=x86-64 --target=avx2-i32x8 -h "sinx_$v.h" -o "sinx_$v.obj" "sinx_$v.ispc" } ; g++ -O3 -fno-tree-vectorize -std=c++17 -o ispc_bench bench.cpp sinx_interleaved.obj sinx_blocked.obj sinx_foreach.obj ; .\ispc_bench.exe
```

Already built? Just run them:

```powershell
cd ispc ; .\ispc_demo.exe
cd ispc ; .\ispc_bench.exe 33554432 5 5
```

See [ISPC](#ispc-ispc) below for what the variants do and the gotchas.

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

## ISPC (`ispc/`)

Three SPMD variants of the same kernel, each exporting a **different** symbol so all
three can link into one binary and be compared head to head.

| File | Exported function | Work assignment |
|---|---|---|
| [ispc/sinx_interleaved.ispc](ispc/sinx_interleaved.ispc) | `ispc_sinx` | lane `i` takes `i, i+programCount, ...` |
| [ispc/sinx_blocked.ispc](ispc/sinx_blocked.ispc) | `ispc_sinx_v2` | lane `i` takes a contiguous block |
| [ispc/sinx_foreach.ispc](ispc/sinx_foreach.ispc) | `ispc_sinx_foreach` | `foreach`, compiler decides |

Building is two stages: `ispc` emits an object file **plus** a C++ header, then g++
compiles the host code and links the object.

**Correctness demo** ([ispc/main.cpp](ispc/main.cpp)) — prints ISPC vs `std::sin`:

```powershell
cd ispc ; ispc -O2 --arch=x86-64 --target=avx2-i32x8 -h sinx_interleaved.h -o sinx_interleaved.obj sinx_interleaved.ispc ; g++ -O3 -std=c++17 -o ispc_demo main.cpp sinx_interleaved.obj ; .\ispc_demo.exe
```

**Three-way benchmark** ([ispc/bench.cpp](ispc/bench.cpp)) — times all variants against a
scalar reference and checks they agree:

```powershell
cd ispc ; foreach ($v in 'interleaved','blocked','foreach') { ispc -O2 --arch=x86-64 --target=avx2-i32x8 -h "sinx_$v.h" -o "sinx_$v.obj" "sinx_$v.ispc" } ; g++ -O3 -fno-tree-vectorize -std=c++17 -o ispc_bench bench.cpp sinx_interleaved.obj sinx_blocked.obj sinx_foreach.obj ; .\ispc_bench.exe
```

Takes optional `N terms reps` arguments, e.g. `.\ispc_bench.exe 33554432 5 5`.

### Result (N = 33.5M, terms = 5, avx2-i32x8)

```
scalar (reference)     0.2396 s    140.1 Melem/s    1.00x
ispc interleaved       0.0351 s    954.7 Melem/s    6.82x
ispc blocked           0.0494 s    679.2 Melem/s    4.85x
ispc foreach           0.0344 s    976.2 Melem/s    6.97x
```

Interleaved and `foreach` land together near the 8x ceiling of an 8-wide target;
**blocked is ~30% slower** because giving each lane a contiguous block makes the lanes
read non-adjacent addresses. ispc says so at compile time:

```
sinx_blocked.ispc:17:27: Performance Warning: Gather required to load value.
sinx_blocked.ispc:31:13: Performance Warning: Scatter required to store value.
```

That warning is the whole point of the exercise — watch for it.

### Gotchas

- **`uniform float* x` is a *varying pointer* to uniform float.** The qualifier binds to
  the pointee. Exported functions need `uniform float* uniform x`, or ispc rejects it with
  *"Varying pointer type parameter is illegal in an exported function"*.
- **The generated header wraps declarations in `namespace ispc`**, so call
  `ispc::ispc_sinx(...)` or add `using namespace ispc;`.
- **`Warning: corrupt .drectve at end of def file` is benign** — MinGW reading MSVC-style
  directives in ispc's COFF object. It links and runs correctly.
- **Keep `terms <= 5`.** `denom` is `uniform int` and holds `(2j+1)!`, so it overflows
  int32 at `j = 6` (`13! = 6.2e9`). Widening to `int64` does not rescue `terms = 10`
  (`21! = 5.1e19` overflows int64 as well) and costs ~5x speed, because 64-bit
  integer-to-float conversion has no efficient AVX2 vector form and ispc scalarizes it.
  For more terms, make `denom` a `float`/`double`.


### Reference

https://gfxcourses.stanford.edu/cs149/fall25