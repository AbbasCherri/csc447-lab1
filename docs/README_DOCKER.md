# CSC447 Lab 1 in Docker

Folder layout:

    csc447-lab1/
      c/         common.h  sequential.c  pthreads.c  openmp.c  CMakeLists.txt
      python/    bench.py  make_results.py  make_figures.py
      docker/    Dockerfile  compose.yaml
      runners/   run.sh  entrypoint.sh
      data/      <- put the csv files here (small is already in)
      results/   <- output lands here

Everything in one command (build, small check, benchmark, tables, figures):

    ./runners/run.sh

Copy `temperatures_medium.csv`, `temperatures_large.csv`, `expected_summary.csv`
(and the others if missing) into `data/`.

## 1. Build the image

    docker build -f docker/Dockerfile -t csc447-lab1 .

## 2. Check correctness on the small file

    docker run --rm -v "$(pwd)/data:/data" -v "$(pwd)/results:/results" csc447-lab1 small

(Windows PowerShell: replace `$(pwd)` with `${PWD}`.)

## 3. Run the full experiment (medium + large, 2/4/8 threads, 6 runs each)

    docker run --rm -v "$(pwd)/data:/data" -v "$(pwd)/results:/results" csc447-lab1

You get `results/benchmark_results.csv` (five sorted times, median, speedup,
correctness flag for every case) and `results/system_info.txt`.

## 4. Run one program by hand

    docker run --rm -it -v "$(pwd)/data:/data" csc447-lab1 openmp

Then type `/data/temperatures_large.csv` and a thread count when asked.
Same for `sequential` and `pthreads`.

## 5. Without Docker (plain CMake)

    cmake -S c -B build && cmake --build build
    ./build/sequential

## Things to know for fair timings

- **Docker Desktop (Windows/Mac) runs in a VM.** Open Settings > Resources and
  give it at least 8 CPUs, otherwise 8 threads will time-share fewer cores.
  `system_info.txt` shows how many CPUs the container really sees.
- **Don't cap the container** with `--cpus`, and close other apps while it runs.
- On Docker Desktop the CPU model shown is the host's, but the core count is
  what the VM was given. For the report, state your real physical cores and
  logical processors from your own OS (Task Manager / `lscpu` / About This Mac),
  and mention the programs ran inside a Linux container.
- Timing inside a container has a tiny overhead versus bare metal; say so in
  the report if you use these numbers.
