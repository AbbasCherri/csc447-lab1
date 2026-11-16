#include <mpi.h>
#include "common.h"

#define CHECK_ALLOC(p) do { if (!(p)) { fprintf(stderr, "allocation failed\n"); \
                            MPI_Abort(MPI_COMM_WORLD, 1); } } while (0)

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    Dataset ds = {0, 0, NULL, NULL};
    char filename[1024];
    int dims[2] = {-1, 0};
    if (rank == 0) {
        if (argc > 1)
            snprintf(filename, sizeof filename, "%s", argv[1]);
        else
            ask_filename(filename);
        if (load_csv(filename, &ds) == 0) {
            dims[0] = ds.rooms;
            dims[1] = ds.cols;
            printf("Loaded %d rooms with %d readings each, P = %d\n", ds.rooms, ds.cols, P);
        }
    }
    MPI_Bcast(dims, 2, MPI_INT, 0, MPI_COMM_WORLD);
    if (dims[0] < 0) {
        MPI_Finalize();
        return 1;
    }
    int R = dims[0], C = dims[1];

    int *rooms_cnt  = (int *)malloc(P * sizeof(int));
    int *rooms_disp = (int *)malloc(P * sizeof(int));
    int *temp_cnt   = (int *)malloc(P * sizeof(int));
    int *temp_disp  = (int *)malloc(P * sizeof(int));
    CHECK_ALLOC(rooms_cnt && rooms_disp && temp_cnt && temp_disp);
    for (int i = 0, s = 0; i < P; i++) {
        rooms_cnt[i]  = R / P + (i < R % P);
        rooms_disp[i] = s;
        temp_cnt[i]   = rooms_cnt[i] * C;
        temp_disp[i]  = s * C;
        s += rooms_cnt[i];
    }
    int mine = rooms_cnt[rank];
    if (rank == 0 && R <= 20)
        for (int i = 0; i < P; i++)
            printf("rank %d: %d room(s) starting at room index %d\n", i, rooms_cnt[i], rooms_disp[i]);

    int *loc_temps = (int *)malloc(((size_t)mine * C + 1) * sizeof(int));
    int *loc_hot   = (int *)malloc((mine + 1) * sizeof(int));
    int *loc_run   = (int *)malloc((mine + 1) * sizeof(int));
    int *loc_cat   = (int *)malloc((mine + 1) * sizeof(int));
    CHECK_ALLOC(loc_temps && loc_hot && loc_run && loc_cat);

    int *all_hot = NULL, *all_run = NULL, *all_cat = NULL;
    if (rank == 0) {
        all_hot = (int *)malloc(R * sizeof(int));
        all_run = (int *)malloc(R * sizeof(int));
        all_cat = (int *)malloc(R * sizeof(int));
        CHECK_ALLOC(all_hot && all_run && all_cat);
    }

    long totals[4];
    double times[NUM_RUNS];
    for (int run = 0; run < NUM_RUNS; run++) {
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();

        memset(loc_hot, 0, (mine + 1) * sizeof(int));
        memset(loc_run, 0, (mine + 1) * sizeof(int));
        memset(loc_cat, 0, (mine + 1) * sizeof(int));
        if (rank == 0) {
            memset(all_hot, 0, R * sizeof(int));
            memset(all_run, 0, R * sizeof(int));
            memset(all_cat, 0, R * sizeof(int));
        }

        MPI_Scatterv(ds.temps, temp_cnt, temp_disp, MPI_INT,
                     loc_temps, mine * C, MPI_INT, 0, MPI_COMM_WORLD);

        long local[4] = {0, 0, 0, 0};
        for (int r = 0; r < mine; r++) {
            RoomResult rr;
            analyze_room(loc_temps + (size_t)r * C, C, &rr);
            loc_hot[r] = rr.hot_count;
            loc_run[r] = rr.longest_run;
            loc_cat[r] = rr.category;
            local[rr.category]++;
            local[3] += rr.hot_count;
        }

        MPI_Gatherv(loc_hot, mine, MPI_INT, all_hot, rooms_cnt, rooms_disp, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Gatherv(loc_run, mine, MPI_INT, all_run, rooms_cnt, rooms_disp, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Gatherv(loc_cat, mine, MPI_INT, all_cat, rooms_cnt, rooms_disp, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Reduce(local, totals, 4, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        double elapsed = MPI_Wtime() - start, longest;
        MPI_Reduce(&elapsed, &longest, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        times[run] = longest;
    }

    if (rank == 0) {
        RoomResult *res = (RoomResult *)malloc(R * sizeof(RoomResult));
        CHECK_ALLOC(res);
        for (int r = 0; r < R; r++) {
            res[r].hot_count   = all_hot[r];
            res[r].longest_run = all_run[r];
            res[r].category    = all_cat[r];
        }
        Totals t = {totals[0], totals[1], totals[2], totals[3]};
        print_small_table(&ds, res);
        printf("\n");
        print_totals(&t, R);
        write_results("results_mpi.csv", &ds, res);
        report_times("mpi", filename, P, times);
        free(res);
        free(all_hot); free(all_run); free(all_cat);
        free(ds.ids); free(ds.temps);
    }

    free(loc_temps); free(loc_hot); free(loc_run); free(loc_cat);
    free(rooms_cnt); free(rooms_disp); free(temp_cnt); free(temp_disp);
    MPI_Finalize();
    return 0;
}
