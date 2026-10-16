#!/bin/bash
set -e
BIN=/app/build
mkdir -p /results && cd /results
for p in sequential pthreads openmp overhead; do ln -sf $BIN/$p ./$p; done

case "${1:-all}" in
  small)
    for prog in sequential pthreads openmp; do
      echo "=== $prog ==="
      if [ $prog = sequential ]; then printf "/data/temperatures_small.csv\n" | ./$prog | sed -n '/room  hot/,/Total hot/p'
      else printf "/data/temperatures_small.csv\n4\n" | ./$prog | sed -n '/room  hot/,/Total hot/p'; fi
    done
    python3 - << 'PY'
import csv
e={r['room_id']:(r['hot_readings'],r['longest_hot_run'],r['category']) for r in csv.DictReader(open('/data/expected_small.csv'))}
for v in ['sequential','pthreads','openmp']:
    g={r['room_id']:(r['hot_readings'],r['longest_hot_run'],r['category']) for r in csv.DictReader(open(f'results_{v}.csv'))}
    print(f"{v}: matches expected_small.csv -> {g==e}")
PY
    ;;
  edge)
    DATA_DIR=/data /app/tests/edge_cases.sh $BIN
    ;;
  all)
    {
      echo "CPU model     : $(lscpu | sed -n 's/^Model name: *//p')"
      echo "Logical CPUs  : $(nproc)   (CPUs visible inside the container)"
      echo "Threads/core  : $(lscpu | sed -n 's/^Thread(s) per core: *//p')"
      echo "Cores/socket  : $(lscpu | sed -n 's/^Core(s) per socket: *//p')"
      echo "OS            : $(. /etc/os-release; echo $PRETTY_NAME) (container)"
      echo "Compiler      : $(gcc --version | head -1)"
    } | tee system_info.txt
    for f in temperatures_medium.csv temperatures_large.csv expected_summary.csv; do
      [ -f /data/$f ] || { echo "Missing /data/$f - mount your csv folder with -v"; exit 1; }
    done
    DATA_DIR=/data /app/tests/edge_cases.sh $BIN
    python3 /app/python/bench.py
    ./overhead | tee overhead.csv
    cd /
    python3 /app/python/make_results.py
    python3 /app/python/make_figures.py
    ;;
  sequential|pthreads|openmp) exec ./$1 ;;
  shell) exec bash ;;
  *) echo "unknown command: $1"; exit 1 ;;
esac
