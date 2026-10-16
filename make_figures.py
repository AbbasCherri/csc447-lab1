import csv, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

csv_path = "results/benchmark_results.csv"
outdir   = "results/figures"
os.makedirs(outdir, exist_ok=True)

data = {}
seq  = {}
with open(csv_path) as f:
    for r in csv.DictReader(f):
        ds = r["dataset"].replace("temperatures_", "").replace(".csv", "")
        if r["version"] == "sequential":
            seq[ds] = float(r["median"])
        else:
            data.setdefault(ds, {}).setdefault(r["version"], []).append(
                (int(r["threads_actual"]), float(r["median"])))

STYLE = {"pthreads": ("Pthreads", "o", "#1f5fa8"), "openmp": ("OpenMP", "s", "#c4561a")}

for ds, versions in data.items():
    fig, ax = plt.subplots(figsize=(5.6, 3.7), dpi=200)
    top = 1.0
    xs_used = {1}
    for ver, (label, marker, color) in STYLE.items():
        if ver not in versions:
            continue
        pts = sorted((t, seq[ds] / m) for t, m in versions[ver])
        pts = [(1, 1.0)] + pts
        xs, ys = zip(*pts)
        xs_used.update(xs)
        top = max(top, max(ys))
        ax.plot(xs, ys, marker=marker, color=color, label=label)
    ax.plot([1], [1], marker="*", color="black", markersize=13, linestyle="none",
            label="Sequential (1 thread)", zorder=5)
    ax.axhline(1, color="gray", lw=0.8, ls=":")
    ax.set_xticks(sorted(xs_used))
    ax.set_xlabel("Number of threads")
    ax.set_ylabel("Speedup")
    ax.set_ylim(0, max(2.0, top * 1.15))
    ax.set_title(f"Speedup, {ds} dataset")
    ax.grid(alpha=0.25)
    ax.legend(fontsize=8, loc="upper left")
    fig.tight_layout()
    for ext in ("png", "pdf"):
        fig.savefig(os.path.join(outdir, f"speedup_{ds}.{ext}"))
    plt.close(fig)
    print(f"wrote {outdir}/speedup_{ds}.png")
