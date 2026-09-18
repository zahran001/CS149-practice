#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    int N;
    int terms;
    float* x;
    float* y;
} args;

void sinx(int N, int terms, float* x, float* y){
    for (int i=0; i<N; i++){
        float value = x[i];
        float numer = x[i] * x[i] * x[i];
        long long denom = 6; // 3!
        int sign = -1;

        for(int j=1; j<=terms; j++){
            value += sign * numer / denom;
            numer *= x[i] * x[i];
            denom *= (2*j+2) * (2*j+3);
            sign *= -1;
        }

        y[i] = value;
    
    }
}

// pthreads require a void* return type and a void* argument
void* thread_func(void* arg){
    args* argsT = (args*)arg;
    sinx(argsT->N, argsT->terms, argsT->x, argsT->y);
    return NULL;
}

void paralled_sinx(int N, int terms, float* x, float* y){
    pthread_t myThread;
    args argsP;
    
    int half_N = N / 2;

    argsP.N = half_N;
    argsP.terms = terms;
    argsP.x = x;
    argsP.y = y;

    // Spawn thread for the first half
    pthread_create(&myThread, NULL, thread_func, &argsP);

    // Run the second half on the main thread (pointer arithmetic advances the start)
    sinx(N-half_N, terms, x + half_N, y + half_N);
    
    // Wait for the spawned thread to finish
    pthread_join(myThread, NULL);
}


int main() {
    // C1
    // int N = 1'000'000'000;
    // int terms = 5;

    // C2
    int N = 10000;
    int terms = 10;
    
    float* x = (float*)malloc(N * sizeof(float));
    float* y = (float*)malloc(N * sizeof(float));

    // Initialize array with dummy data
    for (int i = 0; i < N; i++) {
        x[i] = (float)i / N;
    }

    struct timespec start, end;

    // Start wall-clock timer
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Execute the multi-threaded payload
    paralled_sinx(N, terms, x, y);

    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate time elapsed in seconds
    double time_taken = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Parallel execution time: %f seconds\n", time_taken);

    free(x);
    free(y);
    
    return 0;
}