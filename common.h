#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HOT_LIMIT 30
#define NUM_RUNS  6

#define NORMAL 0
#define WATCH  1
#define URGENT 2

static const char *category_name[3] = { "Normal", "Watch", "Urgent" };

typedef struct {
    int hot_count;
    int longest_run;
    int category;
} RoomResult;

typedef struct {
    long normal, watch, urgent;
    long total_hot;
} Totals;

typedef struct {
    int  rooms;
    int  cols;
    int *ids;
    int *temps;
} Dataset;

static inline double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void analyze_room(const int *row, int cols, RoomResult *out)
{
    int hot = 0, run = 0, best = 0;

    for (int i = 0; i < cols; i++) {
        if (row[i] > HOT_LIMIT) {
            hot++;
            run++;
            if (run > best)
                best = run;
        } else {
            run = 0;
        }
    }

    out->hot_count   = hot;
    out->longest_run = best;
    if (hot == 0)
        out->category = NORMAL;
    else if (best >= 3)
        out->category = URGENT;
    else
        out->category = WATCH;
}

static void compute_totals(Totals *t, const RoomResult *res, int rooms)
{
    t->normal = 0;
    t->watch = 0;
    t->urgent = 0;
    t->total_hot = 0;
    for (int r = 0; r < rooms; r++) {
        if (res[r].category == NORMAL)
            t->normal++;
        else if (res[r].category == WATCH)
            t->watch++;
        else
            t->urgent++;
        t->total_hot += res[r].hot_count;
    }
}

static int load_csv(const char *filename, Dataset *ds)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror(filename);
        return -1;
    }

    char  *line = NULL;
    size_t cap  = 0;

    if (getline(&line, &cap, f) < 0) {
        fprintf(stderr, "%s is empty\n", filename);
        fclose(f);
        return -1;
    }

    int cols = 0;
    for (char *p = line; *p; p++)
        if (*p == ',')
            cols++;

    int rooms = 0;
    while (getline(&line, &cap, f) >= 0)
        if (line[0] >= '0' && line[0] <= '9')
            rooms++;

    ds->ids   = malloc(rooms * sizeof(int));
    ds->temps = malloc((size_t)rooms * cols * sizeof(int));
    if (!ds->ids || !ds->temps) {
        perror("malloc");
        return -1;
    }

    rewind(f);
    getline(&line, &cap, f);

    int r = 0;
    while (r < rooms && getline(&line, &cap, f) >= 0) {
        if (line[0] < '0' || line[0] > '9')
            continue;

        char *end;
        ds->ids[r] = (int)strtol(line, &end, 10);
        for (int i = 0; i < cols; i++)
            ds->temps[(size_t)r * cols + i] = (int)strtol(end + 1, &end, 10);
        r++;
    }

    free(line);
    fclose(f);
    ds->rooms = rooms;
    ds->cols  = cols;
    return 0;
}

static void write_results(const char *path, const Dataset *ds, const RoomResult *res)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "room_id,hot_readings,longest_hot_run,category\n");
    for (int r = 0; r < ds->rooms; r++)
        fprintf(f, "%d,%d,%d,%s\n", ds->ids[r], res[r].hot_count,
                res[r].longest_run, category_name[res[r].category]);
    fclose(f);
}

static void print_totals(const Totals *t, int rooms)
{
    printf("Normal: %ld   Watch: %ld   Urgent: %ld   (sum = %ld, rooms = %d)\n",
           t->normal, t->watch, t->urgent, t->normal + t->watch + t->urgent, rooms);
    printf("Total hot readings: %ld\n", t->total_hot);
}

static void print_small_table(const Dataset *ds, const RoomResult *res)
{
    if (ds->rooms > 20)
        return;
    printf("\nroom  hot  streak  category\n");
    for (int r = 0; r < ds->rooms; r++)
        printf("%4d  %3d  %6d  %s\n", ds->ids[r], res[r].hot_count,
               res[r].longest_run, category_name[res[r].category]);
}

static void report_times(const char *version, const char *filename,
                         int threads, const double *times)
{
    double sorted[5];
    for (int i = 0; i < 5; i++)
        sorted[i] = times[i + 1];
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (sorted[j] < sorted[i]) {
                double tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    double med = sorted[2];

    printf("\nTimes (seconds):\n");
    printf("  run 1 (warm-up, ignored): %.6f\n", times[0]);
    for (int i = 1; i < NUM_RUNS; i++)
        printf("  run %d: %.6f\n", i + 1, times[i]);
    printf("Sorted: %.6f %.6f %.6f %.6f %.6f\n",
           sorted[0], sorted[1], sorted[2], sorted[3], sorted[4]);
    printf("Median: %.6f s\n", med);

    printf("RESULT,%s,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", version, filename,
           threads, sorted[0], sorted[1], sorted[2], sorted[3], sorted[4], med);
}

static void ask_filename(char *name)
{
    printf("Input file name: ");
    fflush(stdout);
    if (scanf("%1023s", name) != 1) {
        fprintf(stderr, "no file name given\n");
        exit(1);
    }
}

static inline int ask_threads(void)
{
    int p;
    printf("Number of threads: ");
    fflush(stdout);
    if (scanf("%d", &p) != 1 || p < 1) {
        fprintf(stderr, "thread count must be a positive number\n");
        exit(1);
    }
    return p;
}

#endif
