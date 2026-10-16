# CSC447 Lab 1

Three versions of the same program: sequential, Pthreads and OpenMP. Each reads a CSV of room temperatures, one room per row, and sorts every room into a category. A reading above 30 is hot. A room with no hot readings is Normal, a room with 3 or more hot readings in a row is Urgent, and every other room is Watch.

Each program times 6 runs, drops the first as a warm-up and reports the median of the other five. The parallel versions split the rooms evenly across threads. The benchmark runs the parallel versions at 2, 4 and 8 threads on the medium and large files and checks each result against the sequential output and `data/expected_summary.csv`.

## Layout

    c/         common.h, sequential.c, pthreads.c, openmp.c, CMakeLists.txt
    python/    bench.py, make_results.py, make_figures.py
    docker/    Dockerfile, compose.yaml
    runners/   run.sh, entrypoint.sh
    docs/      README_DOCKER.md
    data/      input CSVs
    results/   benchmark output

## Data

The medium and large files are too big for GitHub, so they are not in the repo. Put `temperatures_medium.csv` and `temperatures_large.csv` in `data/`. The small file and the expected-output files are already there.

## Run everything

You need Docker with Compose. From the project root:

    ./runners/run.sh

This builds the image, checks the three programs on the small file, runs the full benchmark, then writes the tables and speedup graphs. Everything lands in `results/`:

- `benchmark_results.csv`: five sorted times, median, speedup and a correctness flag for every case
- `system_info.txt`: CPU and compiler details from inside the container
- `report_results/`: results table and summary
- `figures/`: speedup graphs for the medium and large files

Give Docker all your CPU cores before running, or the 8-thread runs will share cores and the timings will be off. `docs/README_DOCKER.md` has more on this and on running single programs by hand.

## Build without Docker

    cmake -S c -B build && cmake --build build
    ./build/sequential

All programs are built with `gcc -O2` (set in `c/CMakeLists.txt`, including the Release build the Docker image uses). The OpenMP version uses `schedule(static)`, disables dynamic teams, and records the real team size, which `bench.py` writes to `threads_actual`.

Each program asks for a file name, and the parallel ones also ask for a thread count.

## Edge-case tests

    tests/edge_cases.sh

Checks all three programs on a CRLF file with no final newline, a room of exactly 30s, an all-hot room, a one-room file, more threads than rooms, and a row with too few readings. It also runs ThreadSanitizer on the Pthreads and OpenMP programs. TSan reports races for OpenMP because libgomp's barrier is not instrumented. The reported writes and reads are separated by the parallel region's implicit barrier, so these are false positives and the script only fails on Pthreads reports.
