import csv, json, os

csv_path = "results/benchmark_results.csv"
sysinfo  = "results/system_info.txt"
outdir   = "results/report_results"
os.makedirs(outdir, exist_ok=True)

NAMES = {"sequential": "Sequential", "pthreads": "Pthreads", "openmp": "OpenMP"}
ORDER = {"sequential": 0, "pthreads": 1, "openmp": 2}

rows = []
with open(csv_path) as f:
    for r in csv.DictReader(f):
        times = [float(r[k]) for k in ("t_a", "t_b", "t_c", "t_d", "t_e")]
        rows.append({
            "dataset":   r["dataset"].replace("temperatures_", "").replace(".csv", ""),
            "version":   r["version"],
            "requested": int(r["threads_requested"]),
            "threads":   int(r["threads_actual"]),
            "times":     sorted(times),
            "median":    float(r["median"]),
            "correct":   r["correct"].strip().lower() == "true",
        })

datasets = []
for r in rows:
    if r["dataset"] not in datasets:
        datasets.append(r["dataset"])
rows.sort(key=lambda r: (datasets.index(r["dataset"]), ORDER[r["version"]], r["threads"]))

problems = []
for r in rows:
    if abs(r["times"][2] - r["median"]) > 1e-9:
        problems.append(f'{r["dataset"]} {r["version"]} P={r["threads"]}: median column '
                        f'({r["median"]}) is not the 3rd sorted time ({r["times"][2]})')
    if r["threads"] != r["requested"]:
        problems.append(f'{r["dataset"]} {r["version"]}: asked for {r["requested"]} threads '
                        f'but the team size was {r["threads"]}')
    if not r["correct"]:
        problems.append(f'{r["dataset"]} {r["version"]} P={r["threads"]}: marked NOT correct')

seq = {}
for r in rows:
    if r["version"] == "sequential":
        seq[r["dataset"]] = r["median"]
for r in rows:
    r["speedup"]    = seq[r["dataset"]] / r["median"]
    r["efficiency"] = r["speedup"] / r["threads"]

with open(os.path.join(outdir, "results_table.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["dataset", "version", "threads", "time1_s", "time2_s", "time3_s",
                "time4_s", "time5_s", "median_s", "speedup"])
    for r in rows:
        w.writerow([r["dataset"], NAMES[r["version"]], r["threads"]] +
                   [f"{t:.6f}" for t in r["times"]] + [f'{r["median"]:.6f}', f'{r["speedup"]:.3f}'])

with open(os.path.join(outdir, "results_table.md"), "w") as f:
    f.write("| Dataset | Version | Threads | Time 1 (s) | Time 2 (s) | Time 3 (s) | "
            "Time 4 (s) | Time 5 (s) | Median (s) | Speedup |\n")
    f.write("|---|---|---|---|---|---|---|---|---|---|\n")
    for r in rows:
        f.write(f'| {r["dataset"]} | {NAMES[r["version"]]} | {r["threads"]} | ' +
                " | ".join(f"{t:.6f}" for t in r["times"]) +
                f' | {r["median"]:.6f} | {r["speedup"]:.3f} |\n')

summary = {"datasets": {}, "problems": problems}
for ds in datasets:
    rs = [r for r in rows if r["dataset"] == ds]
    par = [r for r in rs if r["version"] != "sequential"]
    fastest = min(rs, key=lambda r: r["median"])
    best_par = max(par, key=lambda r: r["speedup"])
    worst_par = min(par, key=lambda r: r["speedup"])
    spreads = {(r["version"], r["threads"]): (r["times"][-1] - r["times"][0]) / r["median"] for r in rs}
    avg_spread = sum(spreads.values()) / len(spreads)
    summary["datasets"][ds] = {
        "sequential_median_s": seq[ds],
        "fastest_overall": {"version": NAMES[fastest["version"]], "threads": fastest["threads"],
                            "median_s": fastest["median"], "speedup": fastest["speedup"]},
        "best_parallel":  {"version": NAMES[best_par["version"]], "threads": best_par["threads"],
                           "median_s": best_par["median"], "speedup": best_par["speedup"]},
        "worst_parallel": {"version": NAMES[worst_par["version"]], "threads": worst_par["threads"],
                           "median_s": worst_par["median"], "speedup": worst_par["speedup"]},
        "avg_run_to_run_spread_pct": 100 * avg_spread,
        "by_version": {},
    }
    for v in ("pthreads", "openmp"):
        vr = sorted([r for r in par if r["version"] == v], key=lambda r: r["threads"])
        summary["datasets"][ds]["by_version"][NAMES[v]] = [
            {"threads": r["threads"], "median_s": r["median"], "speedup": r["speedup"],
             "efficiency": r["efficiency"]} for r in vr]

if len(datasets) >= 2:
    a, b = datasets[0], datasets[1]
    summary["scaling"] = {"from": a, "to": b, "sequential_time_ratio": seq[b] / seq[a]}

for ds in datasets:
    d = summary["datasets"][ds]
    d["any_clear_speedup"] = d["best_parallel"]["speedup"] > 1 + d["avg_run_to_run_spread_pct"] / 100

json.dump(summary, open(os.path.join(outdir, "summary.json"), "w"), indent=2)

L = []
L.append("RESULTS SUMMARY")
L.append("=" * 60)
if os.path.exists(sysinfo):
    L.append("\nMachine (from " + sysinfo + "):")
    L.extend("  " + line.rstrip() for line in open(sysinfo) if line.strip())
L.append("\nData checks:")
L.extend(["  OK: every case marked correct, team sizes match, medians are the 3rd sorted time"]
         if not problems else ["  PROBLEM: " + p for p in problems])

for ds in datasets:
    d = summary["datasets"][ds]
    L.append(f"\n--- {ds} dataset ---")
    L.append(f"Sequential median: {d['sequential_median_s']:.6f} s")
    fo, bp, wp = d["fastest_overall"], d["best_parallel"], d["worst_parallel"]
    L.append(f"Fastest configuration: {fo['version']}"
             + (f" with {fo['threads']} threads" if fo["version"] != "Sequential" else "")
             + f" at {fo['median_s']:.6f} s")
    L.append(f"Best parallel case:  {bp['version']}, {bp['threads']} threads, speedup {bp['speedup']:.3f}")
    L.append(f"Worst parallel case: {wp['version']}, {wp['threads']} threads, speedup {wp['speedup']:.3f}")
    L.append(f"Average run-to-run spread (max-min over median): {d['avg_run_to_run_spread_pct']:.1f}%")
    L.append("Speedup beyond the noise level? " + ("YES" if d["any_clear_speedup"] else "NO - best speedup is within run-to-run spread"))
    L.append("Per version (threads: median s / speedup / efficiency):")
    for v, lst in d["by_version"].items():
        L.append(f"  {v:9s} " + "   ".join(
            f"{x['threads']}: {x['median_s']:.6f} / {x['speedup']:.3f} / {x['efficiency']:.2f}" for x in lst))

if "scaling" in summary:
    s = summary["scaling"]
    L.append(f"\n--- More data ---")
    L.append(f"Sequential time went from {seq[s['from']]:.6f} s ({s['from']}) to "
             f"{seq[s['to']]:.6f} s ({s['to']}): a factor of {s['sequential_time_ratio']:.2f}")
    for v in ("Pthreads", "OpenMP"):
        for t in (2, 4, 8):
            sp = [next((x["speedup"] for x in summary["datasets"][ds]["by_version"][v] if x["threads"] == t), None)
                  for ds in (s["from"], s["to"])]
            if None not in sp:
                L.append(f"  {v} {t} threads: speedup {sp[0]:.3f} -> {sp[1]:.3f}")

text = "\n".join(L) + "\n"
open(os.path.join(outdir, "summary.txt"), "w").write(text)
print(text)
print(f"files written to {outdir}/")
