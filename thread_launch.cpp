#include <cstdio>
#include <pthread.h>
#include <atomic>
#include <algorithm>
#include <chrono>

// Define the number of threads for the thread pool test
#define NUM_WORKER_THREADS 4

namespace CycleTimer {
    inline double currentSeconds() {
        // Use steady_clock to guarantee monotonic timing for benchmarks
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }
}

/* Using `noinline` and `optnone` attributes to prevent compiler from
 * inlining this function or optimizing away calls to it completely */
__attribute__((noinline)) __attribute__((optnone))
void* dowork(void* arg) {
    /* Do nothing. This is the world's simplest task! */
    return NULL;
}

/*
 * run_test0 --
 *
 * Makes num_tasks calls to dowork(). Use of noinline attribute on
 * dowork() should prevent it from being inlined, so the function
 * calls will actually be made.
 */
double run_test0(int num_tasks) {
    int count = 0;

    printf("\n");
    printf("Running test 0: Calling dowork() %d times sequentially from one thread\n", num_tasks);

    double start_time = CycleTimer::currentSeconds();

    for (int i=0; i<num_tasks; i++) {
        dowork(NULL);
        count++;
    }

    double end_time = CycleTimer::currentSeconds();
    double elapsed = end_time - start_time;

    printf("Test 0 Results: ========================================\n");
    printf("Total time:        %.3f ms\n", elapsed * 1000.f);
    
    return elapsed;
}

/*
 * This is meant to show off the cost of spawning one thread per task.
 * Note that since the master thread spawns, then joins threads
 * serially, this is not exactly a true test of the cost of spawning
 * one thread per task.
 */
double run_test1(int num_tasks) {
    pthread_t thread;

    printf("Running test 1: spawning one thread per task (%d tasks)\n", num_tasks);

    double min_time = 1e10;
    double max_time = -1e10;
    double start_time = CycleTimer::currentSeconds();

    for (int i=0; i<num_tasks; i++) {
        double iter_start_time = CycleTimer::currentSeconds();

        pthread_create(&thread, NULL, &dowork, NULL);
        pthread_join(thread, NULL);

        double iter_end_time = CycleTimer::currentSeconds();
        double elapsed = iter_end_time - iter_start_time;

        min_time = std::min(elapsed, min_time);
        max_time = std::max(elapsed, max_time);
    }

    double end_time = CycleTimer::currentSeconds();
    double elapsed = end_time - start_time;

    printf("Test 1 Results: ========================================\n");
    printf("Total time:        %.2f sec\n", elapsed);
    printf("Time per spawn: %.4f ms (min: %.4f ms, max: %.4f ms)\n",
           1000.f * elapsed / num_tasks, 1000.f * min_time, 1000.f * max_time);
    printf("Task completions/sec:   %.2f K\n\n", static_cast<double>(num_tasks) / elapsed / 1000.f );

    return static_cast<double>(num_tasks) / elapsed;
}

struct worker_info {
    int num_tasks;
    std::atomic<int> next_task;
};

void* worker2(void* arg) {
    worker_info* thread_data = static_cast<worker_info*>(arg);

    int num_tasks_performed = 0;

    while (thread_data->next_task < thread_data->num_tasks) {
        thread_data->next_task++;
        dowork(NULL);
        num_tasks_performed++;
    }

    printf("Num tasks performed by worker thread: %d\n", num_tasks_performed);

    return NULL;
}

double run_test2(int num_tasks) {
    worker_info thread_data;
    thread_data.num_tasks = num_tasks;
    thread_data.next_task = 0;

    pthread_t thread[NUM_WORKER_THREADS];

    printf("\nRunning test 2: spawning a pool of %d threads, threads loop grabbing next task until done.\n", NUM_WORKER_THREADS);

    double start_time = CycleTimer::currentSeconds();

    for (int i=0; i<NUM_WORKER_THREADS; i++) {
        pthread_create(&thread[i], NULL, &worker2, &thread_data);
    }

    for (int i=0; i<NUM_WORKER_THREADS; i++) {
        pthread_join(thread[i], NULL);
    }

    double end_time = CycleTimer::currentSeconds();
    double elapsed = end_time - start_time;

    printf("Test 2 Results: ========================================\n");
    printf("Total time:        %.3f ms\n", elapsed * 1000.f);
    
    return elapsed;
}

int main() {
    // 100,000 tasks is a good balance. High enough to measure the 
    // thread pool efficiently, but low enough so Test 1 doesn't hang.
    int num_tasks = 100000; 

    run_test0(num_tasks);
    run_test1(num_tasks);
    run_test2(num_tasks);
    
    return 0;
}