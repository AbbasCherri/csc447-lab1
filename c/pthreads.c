#include "common.h"
#include <pthread.h>

typedef struct {
    const Dataset *ds;
    RoomResult    *res;
    int            first;
    int            last;
} Job;

static void *worker(void *arg)
{
    Job *job = (Job *)arg;
    const Dataset *ds = job->ds;

    for (int r = job->first; r < job->last; r++)
        analyze_room(ds->temps + (size_t)r * ds->cols, ds->cols, &job->res[r]);
    return NULL;
}

int main(void)
{
    char filename[1024];
    ask_filename(filename);
    int P = ask_threads();

    Dataset ds;
    if (load_csv(filename, &ds) != 0)
        return 1;
    printf("Loaded %d rooms with %d readings each\n", ds.rooms, ds.cols);
    printf("Using %d worker threads\n", P);

    RoomResult *res  = malloc(ds.rooms * sizeof(RoomResult));
    Job        *jobs = malloc(P * sizeof(Job));
    pthread_t  *tids = malloc(P * sizeof(pthread_t));
    if (!res || !jobs || !tids) {
        perror("malloc");
        return 1;
    }
    Totals tot;

    double times[NUM_RUNS];
    for (int run = 0; run < NUM_RUNS; run++) {
        double start = now_seconds();

        int base  = ds.rooms / P;
        int extra = ds.rooms % P;
        int next  = 0;
        for (int t = 0; t < P; t++) {
            int count = base;
            if (t < extra)
                count++;
            jobs[t].ds    = &ds;
            jobs[t].res   = res;
            jobs[t].first = next;
            jobs[t].last  = next + count;
            next += count;
            if (pthread_create(&tids[t], NULL, worker, &jobs[t]) != 0) {
                fprintf(stderr, "pthread_create failed for thread %d\n", t);
                return 1;
            }
        }
        for (int t = 0; t < P; t++) {
            if (pthread_join(tids[t], NULL) != 0) {
                fprintf(stderr, "pthread_join failed for thread %d\n", t);
                return 1;
            }
        }

        compute_totals(&tot, res, ds.rooms);
        times[run] = now_seconds() - start;
    }

    print_small_table(&ds, res);
    printf("\n");
    print_totals(&tot, ds.rooms);
    write_results("results_pthreads.csv", &ds, res);

    report_times("pthreads", filename, P, times);

    free(res);
    free(jobs);
    free(tids);
    free(ds.ids);
    free(ds.temps);
    return 0;
}
