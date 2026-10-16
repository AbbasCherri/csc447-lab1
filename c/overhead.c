#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <omp.h>

#define NUM_RUNS 6

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void *empty_worker(void *arg)
{
    return arg;
}

static void sort_doubles(double *a, int n)
{
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (a[j] < a[i]) {
                double tmp = a[i];
                a[i] = a[j];
                a[j] = tmp;
            }
}

int main(void)
{
    int sizes[] = { 2, 4, 8 };
    printf("version,threads,median_ms\n");

    for (int s = 0; s < 3; s++) {
        int P = sizes[s];
        pthread_t tids[8];
        double t[NUM_RUNS];

        for (int run = 0; run < NUM_RUNS; run++) {
            double start = now_seconds();
            for (int i = 0; i < P; i++)
                if (pthread_create(&tids[i], NULL, empty_worker, NULL) != 0)
                    return 1;
            for (int i = 0; i < P; i++)
                pthread_join(tids[i], NULL);
            t[run] = now_seconds() - start;
        }
        sort_doubles(t + 1, NUM_RUNS - 1);
        printf("pthreads,%d,%.4f\n", P, t[3] * 1000);

        for (int run = 0; run < NUM_RUNS; run++) {
            double start = omp_get_wtime();
            #pragma omp parallel num_threads(P)
            {
                __asm__ volatile("" ::: "memory");
            }
            t[run] = omp_get_wtime() - start;
        }
        sort_doubles(t + 1, NUM_RUNS - 1);
        printf("openmp,%d,%.4f\n", P, t[3] * 1000);
    }
    return 0;
}
