#include "common.h"

int main(void)
{
    char filename[1024];
    ask_filename(filename);

    Dataset ds;
    if (load_csv(filename, &ds) != 0)
        return 1;
    printf("Loaded %d rooms with %d readings each\n", ds.rooms, ds.cols);

    RoomResult *res = malloc(ds.rooms * sizeof(RoomResult));
    if (!res) {
        perror("malloc");
        return 1;
    }
    Totals tot;

    double times[NUM_RUNS];
    for (int run = 0; run < NUM_RUNS; run++) {
        double start = now_seconds();
        for (int r = 0; r < ds.rooms; r++)
            analyze_room(ds.temps + (size_t)r * ds.cols, ds.cols, &res[r]);
        compute_totals(&tot, res, ds.rooms);
        times[run] = now_seconds() - start;
    }

    print_small_table(&ds, res);
    printf("\n");
    print_totals(&tot, ds.rooms);
    write_results("results_sequential.csv", &ds, res);

    report_times("sequential", filename, 1, times);

    free(res);
    free(ds.ids);
    free(ds.temps);
    return 0;
}
