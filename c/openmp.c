#include "common.h"
#include <omp.h>

int main(void)
{
    char filename[1024];
    ask_filename(filename);
    int P = ask_threads();

    Dataset ds;
    if (load_csv(filename, &ds) != 0)
        return 1;
    printf("Loaded %d rooms with %d readings each\n", ds.rooms, ds.cols);
    printf("Using %d OpenMP threads\n", P);

    /* Measure the real team size outside the timed section. */
    omp_set_dynamic(0);
    int actual_threads = 0;
    #pragma omp parallel num_threads(P)
    {
        #pragma omp single
        actual_threads = omp_get_num_threads();
    }
    printf("Requested: %d  Actual team size: %d\n", P, actual_threads);
    printf("ACTUAL_THREADS,%d\n", actual_threads);

    RoomResult *res = malloc(ds.rooms * sizeof(RoomResult));
    if (!res) {
        perror("malloc");
        return 1;
    }
    Totals tot;

    double times[NUM_RUNS];
    for (int run = 0; run < NUM_RUNS; run++) {
        double start = omp_get_wtime();

        #pragma omp parallel for num_threads(P) schedule(static)
        for (int r = 0; r < ds.rooms; r++)
            analyze_room(ds.temps + (size_t)r * ds.cols, ds.cols, &res[r]);

        compute_totals(&tot, res, ds.rooms);
        times[run] = omp_get_wtime() - start;
    }

    print_small_table(&ds, res);
    printf("\n");
    print_totals(&tot, ds.rooms);
    write_results("results_openmp.csv", &ds, res);

    report_times("openmp", filename, P, times);

    free(res);
    free(ds.ids);
    free(ds.temps);
    return 0;
}
