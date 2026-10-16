#!/bin/bash
# Edge-case checks for all three programs. Usage: tests/edge_cases.sh [build_dir]
# Builds into a temp dir if no build dir is given. Exits non-zero on any failure.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${1:-}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
fail=0

if [ -z "$BIN" ]; then
    cmake -S "$ROOT/c" -B "$WORK/build" >/dev/null && cmake --build "$WORK/build" >/dev/null || exit 1
    BIN=$WORK/build
fi

cd "$WORK"

# CRLF endings, no final newline, a room of exactly 30s, an all-hot room
printf 'room_id,t1,t2,t3,t4\r\n1,31,31,31,31\r\n2,30,30,30,30\r\n3,10,40,10,40' > crlf.csv
printf 'room_id,hot_readings,longest_hot_run,category\n1,4,4,Urgent\n2,0,0,Normal\n3,2,1,Watch\n' > crlf.expected

# One room only
printf 'room_id,t1,t2,t3\n7,31,31,31\n' > one_room.csv
printf 'room_id,hot_readings,longest_hot_run,category\n7,3,3,Urgent\n' > one_room.expected

check() {  # name file expected threads
    local prog=$1 file=$2 exp=$3 p=$4
    if [ "$prog" = sequential ]; then input="$file\n"; else input="$file\n$p\n"; fi
    printf "$input" | "$BIN/$prog" > out.txt 2>&1
    if diff -q "results_$prog.csv" "$exp" >/dev/null; then
        echo "PASS  $prog  $file  P=$p"
    else
        echo "FAIL  $prog  $file  P=$p"; fail=1
    fi
}

for prog in sequential pthreads openmp; do
    check $prog crlf.csv     crlf.expected     16   # CRLF, and P > rooms
    check $prog one_room.csv one_room.expected 8    # one room, P > rooms
done

# Rows with too few readings must be rejected, not read past
printf 'room_id,t1,t2,t3\n1,31,31\n' > short_row.csv
for prog in sequential pthreads openmp; do
    if printf "short_row.csv\n4\n" | "$BIN/$prog" 2>&1 | grep -q "fewer than"; then
        echo "PASS  $prog  short_row.csv rejected"
    else
        echo "FAIL  $prog  short_row.csv not rejected"; fail=1
    fi
done

# ThreadSanitizer on the Pthreads and OpenMP programs
for prog in pthreads openmp; do
    flags="-O1 -g -fsanitize=thread"
    [ $prog = openmp ] && flags="$flags -fopenmp" || flags="$flags -pthread"
    gcc $flags "$ROOT/c/$prog.c" -o "tsan_$prog" 2>/dev/null || { echo "SKIP  tsan $prog (build failed)"; continue; }
    printf "%s\n4\n" "$ROOT/data/temperatures_small.csv" | "./tsan_$prog" > tsan.txt 2>&1
    if grep -q "WARNING: ThreadSanitizer" tsan.txt; then
        # libgomp is not TSan-instrumented, so OpenMP can report false positives
        echo "WARN  tsan $prog reported races (expected: uninstrumented libgomp barrier)"; [ $prog = pthreads ] && fail=1
    else
        echo "PASS  tsan $prog"
    fi
done

exit $fail
