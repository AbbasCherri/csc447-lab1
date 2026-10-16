import csv, subprocess, os

data_dir = os.environ.get("DATA_DIR", "/data")
files    = ["temperatures_medium.csv", "temperatures_large.csv"]
threads  = [2, 4, 8]

def check_totals(name, results_csv):
    counts = {"Normal": 0, "Watch": 0, "Urgent": 0}
    hot = 0
    rooms = 0
    for row in csv.DictReader(open(results_csv)):
        counts[row["category"]] += 1
        hot += int(row["hot_readings"])
        rooms += 1
    for row in csv.DictReader(open(os.path.join(data_dir, "expected_summary.csv"))):
        if row["dataset"] == name:
            return (rooms == int(row["rooms"]) and
                    counts["Normal"] == int(row["normal_rooms"]) and
                    counts["Watch"] == int(row["watch_rooms"]) and
                    counts["Urgent"] == int(row["urgent_rooms"]) and
                    hot == int(row["total_hot_readings"]))
    return False

def run(prog, fname, p=None):
    stdin = os.path.join(data_dir, fname) + "\n"
    if p:
        stdin += str(p) + "\n"
    out = subprocess.run(["./" + prog], input=stdin, capture_output=True, text=True).stdout
    line = [l for l in out.splitlines() if l.startswith("RESULT,")][0].split(",")
    times = [float(x) for x in line[4:9]]
    median = float(line[9])
    actual = p
    for l in out.splitlines():
        if l.startswith("ACTUAL_THREADS,"):
            lo, hi = (int(x) for x in l.split(",")[1:3])
            if lo != hi or lo != p:
                raise SystemExit(f"{prog} requested {p} threads, team size {lo}..{hi}")
            actual = lo
    return times, median, actual

rows = []
for fname in files:
    print(fname, flush=True)
    seq_t, seq_m, _ = run("sequential", fname)
    ok_seq = check_totals(fname, "results_sequential.csv")
    os.replace("results_sequential.csv", "ref_sequential.csv")
    print(f"  sequential  median {seq_m:.6f}  totals ok: {ok_seq}", flush=True)
    rows.append([fname, "sequential", 1, 1] + seq_t + [seq_m, 1.0, ok_seq])

    for prog in ["pthreads", "openmp"]:
        for p in threads:
            t, m, actual = run(prog, fname, p)
            res_file = f"results_{prog}.csv"
            same = open(res_file).read() == open("ref_sequential.csv").read()
            ok = same and check_totals(fname, res_file)
            sp = seq_m / m
            print(f"  {prog:10s} P={p}  median {m:.6f}  speedup {sp:.3f}  correct: {ok}", flush=True)
            rows.append([fname, prog, p, actual] + t + [m, sp, ok])

with open("benchmark_results.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["dataset", "version", "threads_requested", "threads_actual",
                "t_a", "t_b", "t_c", "t_d", "t_e", "median", "speedup", "correct"])
    w.writerows(rows)
print("wrote benchmark_results.csv")
