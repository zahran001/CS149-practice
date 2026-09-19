#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <algorithm>

#include "sinx_interleaved.h"   // ispc_sinx          -- stride by programCount
#include "sinx_blocked.h"       // ispc_sinx_v2       -- contiguous block per lane
#include "sinx_foreach.h"       // ispc_sinx_foreach  -- compiler chooses

// Scalar reference. Build this file with -fno-tree-vectorize so it stays scalar
// and the ISPC speedups are measured against genuinely serial code.
static void sinx_scalar(int N, int terms, const float* x, float* y){
    for (int i=0; i<N; i++){
        float value = x[i];
        float numer = x[i]*x[i]*x[i];
        long long denom = 6;
        int sign = -1;
        for (int j=1; j<=terms; j++){
            value += sign * numer / denom;
            numer *= x[i]*x[i];
            denom *= (2*j+2)*(2*j+3);
            sign *= -1;
        }
        y[i] = value;
    }
}

using Clock = std::chrono::steady_clock;

template <typename F>
static double best_of(int reps, F&& fn){
    double best = 1e30;
    for (int r=0; r<reps; r++){
        auto t0 = Clock::now();
        fn();
        auto t1 = Clock::now();
        best = std::min(best, std::chrono::duration<double>(t1-t0).count());
    }
    return best;
}

// max abs difference against the scalar result -- all variants must agree
static double max_err(const float* a, const float* b, int N){
    double m = 0.0;
    for (int i=0;i<N;i++) m = std::max(m, (double)std::fabs(a[i]-b[i]));
    return m;
}

int main(int argc, char** argv){
    int N     = (argc>1) ? atoi(argv[1]) : 32*1024*1024;  // multiple of 8
    int terms = (argc>2) ? atoi(argv[2]) : 5;             // keep <= 5, see README
    int reps  = (argc>3) ? atoi(argv[3]) : 5;

    float* x   = (float*)malloc((size_t)N*sizeof(float));
    float* ref = (float*)malloc((size_t)N*sizeof(float));
    float* out = (float*)malloc((size_t)N*sizeof(float));
    if(!x||!ref||!out){ printf("alloc failed\n"); return 1; }

    for (int i=0;i<N;i++) x[i] = (float)i / (float)N * 3.14159265f;
    for (int i=0;i<N;i++) ref[i] = 0.0f;                  // pre-fault outputs
    for (int i=0;i<N;i++) out[i] = 0.0f;

    printf("N = %d, terms = %d, best of %d\n\n", N, terms, reps);

    double ts = best_of(reps, [&]{ sinx_scalar(N, terms, x, ref); });
    printf("%-22s %9.4f s   %8.1f Melem/s   %6s   max_err\n",
           "scalar (reference)", ts, N/ts/1e6, "1.00x");

    struct Variant { const char* name; void (*fn)(int32_t,int32_t,float*,float*); };
    const Variant vs[] = {
        { "ispc interleaved", ispc::ispc_sinx },
        { "ispc blocked",     ispc::ispc_sinx_v2 },
        { "ispc foreach",     ispc::ispc_sinx_foreach },
    };

    for (const auto& v : vs){
        double t = best_of(reps, [&]{ v.fn(N, terms, x, out); });
        printf("%-22s %9.4f s   %8.1f Melem/s   %5.2fx   %.3e\n",
               v.name, t, N/t/1e6, ts/t, max_err(ref, out, N));
    }

    free(x); free(ref); free(out);
    return 0;
}
