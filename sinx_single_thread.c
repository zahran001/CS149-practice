#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// N different values. Calculate sin() for each one.
void sinx(int N, int terms, float *x, float *y)
{
    for (int i = 0; i < N; i++)
    {
        float value = x[i];
        float numer = x[i] * x[i] * x[i];
        long long denom = 6; // 3!
        int sign = -1;

        for (int j = 1; j <= terms; j++)
        {
            value += sign * numer / denom;
            numer *= x[i] * x[i];
            denom *= (2 * j + 2) * (2 * j + 3);
            sign *= -1;
        }

        y[i] = value;
    }
}

int main()
{

    // using a large number to make the timing noticeable
    // C1
    // int N = 1'000'000'000;
    // int terms = 5; // determines the accuracy

    // C2
    int N = 10000;
    int terms = 10; // determines the accuracy

    float *x = (float *)malloc(N * sizeof(float)); // input values
    float *y = (float *)malloc(N * sizeof(float)); // output values

    // initialize array with dummy values between 0.0 and 1.0
    for (int i = 0; i < N; i++)
    {
        x[i] = (float)i / N;
    }

    struct timespec start, end;

    // Start wall-clock timer
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Execute the serial function
    sinx(N, terms, x, y);

    // Stop wall-clock timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    // calculate elapsed time in seconds
    double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Serial execution time: %f seconds\n", time_taken);

    free(x);
    free(y);

    return 0;
}