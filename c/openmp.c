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

    omp_set_dynamic(0);

    RoomResult *res = malloc(ds.rooms * sizeof(RoomResult));
    if (!res) {
        perror("malloc");
        return 1;
    }
    Totals tot;

    double times[NUM_RUNS];
    int team_min = P, team_max = P;
    for (int run = 0; run < NUM_RUNS; run++) {
        int team = 0;
        double start = omp_get_wtime();

        #pragma omp parallel num_threads(P)
        {
            #pragma omp single
            team = omp_get_num_threads();

            #pragma omp for schedule(static)
            for (int r = 0; r < ds.rooms; r++)
                analyze_room(ds.temps + (size_t)r * ds.cols, ds.cols, &res[r]);
        }

        compute_totals(&tot, res, ds.rooms);
        times[run] = omp_get_wtime() - start;

        if (run == 0 || team < team_min)
            team_min = team;
        if (run == 0 || team > team_max)
            team_max = team;
    }

    printf("Requested: %d  Team size over %d timed runs: %d to %d\n",
           P, NUM_RUNS, team_min, team_max);
    printf("ACTUAL_THREADS,%d,%d\n", team_min, team_max);

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
