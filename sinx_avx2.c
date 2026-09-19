#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <immintrin.h>

void sinx(int N, int terms, float *x, float *y)
{
    float three_fact = 6.0f;

    // Assumes N is a multiple of 8
    for (int i = 0; i < N; i += 8)
    {
        // Use 'loadu' (unaligned) to prevent segfaults if malloc doesn't 32-byte align
        __m256 origx = _mm256_loadu_ps(&x[i]);
        __m256 value = origx;
        __m256 numer = _mm256_mul_ps(origx, _mm256_mul_ps(origx, origx));
        __m256 denom = _mm256_set1_ps(three_fact);
        float sign = -1.0f;

        for (int j = 1; j <= terms; j++)
        {
            __m256 tmp = _mm256_div_ps(_mm256_mul_ps(_mm256_set1_ps(sign), numer), denom);
            value = _mm256_add_ps(value, tmp);

            numer = _mm256_mul_ps(numer, _mm256_mul_ps(origx, origx));

            // Compute the scalar math first, then broadcast to the vector
            float next_denom = (float)((2 * j + 2) * (2 * j + 3));
            denom = _mm256_mul_ps(denom, _mm256_set1_ps(next_denom));
            sign *= -1.0f;
        }
        // Use 'storeu' (unaligned)
        _mm256_storeu_ps(&y[i], value);
    }
}

int main()
{
    int N = 10000000; // 10 million (divisible by 8)
    int terms = 10;

    float *x = (float *)malloc(N * sizeof(float));
    float *y = (float *)malloc(N * sizeof(float));

    for (int i = 0; i < N; i++)
    {
        x[i] = (float)i * 0.000001f;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    sinx(N, terms, x, y);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_taken = (end.tv_sec - start.tv_sec) +
                        (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("AVX SIMD execution time: %f seconds\n", time_taken);

    free(x);
    free(y);

    return 0;
}