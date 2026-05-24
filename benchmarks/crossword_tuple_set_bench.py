#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Benchmark TupleSet representations on crossword dictionaries.

The generator first solves each selected crossword grid with the full embedded
SCOWL dictionary, extracts the words used by that solution, and then creates
filtered dictionary files that always retain those required words. Extra words
are sampled deterministically per length, so every filtered dictionary remains
solvable while varying TupleSet size.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import platform
import random
import re
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_GRIDS = [0, 1, 2, 5, 10]
DEFAULT_SCALES = [0.01, 0.02, 0.05, 0.08, 0.12, 0.18, 0.25, 0.35, 0.50, 0.75, 1.00]
DEFAULT_VARIANTS = {
    "dense": "build/bench-dense/bin/crossword",
    "compressed": "build/bench-compressed/bin/crossword",
}
RUNTIME_RE = re.compile(r"runtime:\s+([0-9.]+)ms")
GRID_RE = re.compile(r"const int g(\d+)\[\]\s*=\s*\{(.*?)\};", re.S)
WORD_ARRAY_RE = re.compile(r"const char\* w_(\d+)\[\]\s*=\s*\{(.*?)\};", re.S)
STRING_RE = re.compile(r'"([a-z]*)"')
INT_RE = re.compile(r"-?\d+")


@dataclass(frozen=True)
class Slot:
    length: int
    x: int
    y: int
    horizontal: bool


def repo_default() -> Path:
    return Path(__file__).resolve().parents[1]


def result_root(args: argparse.Namespace) -> Path:
    return Path(args.results_root) / args.name


def load_words(repo: Path) -> dict[int, list[str]]:
    text = (repo / "examples" / "scowl.hpp").read_text()
    words: dict[int, list[str]] = {}
    for m in WORD_ARRAY_RE.finditer(text):
        length = int(m.group(1))
        seen: set[str] = set()
        vals: list[str] = []
        for w in STRING_RE.findall(m.group(2)):
            if len(w) == length and w not in seen:
                seen.add(w)
                vals.append(w)
        words[length] = vals
    return words


def load_grids(repo: Path) -> dict[int, list[int]]:
    text = (repo / "examples" / "crossword.cpp").read_text()
    grids: dict[int, list[int]] = {}
    for m in GRID_RE.finditer(text):
        body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S)
        body = re.sub(r"//.*", "", body)
        grids[int(m.group(1))] = [int(x) for x in INT_RE.findall(body)]
    return grids


def parse_slots(data: list[int]) -> tuple[int, int, list[Slot]]:
    width, height = data[0], data[1]
    idx = 2
    black = data[idx]
    idx += 1 + 2 * black
    slots: list[Slot] = []
    while data[idx] != 0:
        length = data[idx]
        count = data[idx + 1]
        idx += 2
        for _ in range(count):
            x, y, d = data[idx], data[idx + 1], data[idx + 2]
            slots.append(Slot(length, x, y, d == 0))
            idx += 3
    return width, height, slots


def parse_solution_grid(stdout: str) -> list[str]:
    rows: list[str] = []
    for line in stdout.splitlines():
        s = line.strip()
        if s and all(("a" <= c <= "z") or c == "*" for c in s):
            rows.append(s)
    return rows


def slot_word(rows: list[str], slot: Slot) -> str:
    chars = []
    for i in range(slot.length):
        x = slot.x + i if slot.horizontal else slot.x
        y = slot.y if slot.horizontal else slot.y + i
        chars.append(rows[y][x])
    return "".join(chars)


def solve_full_grid(repo: Path, binary: Path, grid: int, timeout: float) -> list[str]:
    cmd = [
        str(binary),
        "-model", "tuple-set",
        "-mode", "solution",
        "-solutions", "1",
        "-file-sol", "stdout",
        "-file-stat", "stdout",
        str(grid),
    ]
    cp = subprocess.run(cmd, cwd=repo, text=True, capture_output=True, timeout=timeout)
    if cp.returncode != 0:
        raise RuntimeError(f"failed to solve grid {grid}: {cp.stderr or cp.stdout}")
    rows = parse_solution_grid(cp.stdout)
    if not rows:
        raise RuntimeError(f"no printable solution for grid {grid}")
    return rows


def required_words_for_grid(repo: Path, binary: Path, grid: int, timeout: float) -> dict[int, set[str]]:
    grids = load_grids(repo)
    _, _, slots = parse_slots(grids[grid])
    rows = solve_full_grid(repo, binary, grid, timeout)
    required: dict[int, set[str]] = {}
    for slot in slots:
        required.setdefault(slot.length, set()).add(slot_word(rows, slot))
    return required


def tuple_stats(words_by_length: dict[int, list[str]], lengths: set[int]) -> dict[str, Any]:
    by_length: dict[str, Any] = {}
    total_dense = 0
    for length in sorted(lengths):
        words = words_by_length.get(length, [])
        n_tuples = len(words)
        if n_tuples == 0:
            n_words = 0
            n_vals = 0
            dense_bytes = 0
        else:
            n_words = math.ceil(n_tuples / 64)
            n_vals = n_tuples
            for pos in range(length):
                n_vals += len({w[pos] for w in words})
            dense_bytes = n_words * n_vals * 8
        total_dense += dense_bytes
        by_length[str(length)] = {
            "tuples": n_tuples,
            "bit_words": n_words,
            "support_values": n_vals,
            "dense_bytes": dense_bytes,
        }
    return {"by_length": by_length, "total_dense_bytes": total_dense}


def make_filtered_words(
    full_words: dict[int, list[str]],
    required: dict[int, set[str]],
    scale: float,
    seed: int,
) -> dict[int, list[str]]:
    rng = random.Random(seed)
    out: dict[int, list[str]] = {}
    for length, req in sorted(required.items()):
        all_words = list(full_words.get(length, []))
        req_set = set(req)
        target = max(len(req_set), int(math.ceil(len(all_words) * scale)))
        extras = [w for w in all_words if w not in req_set]
        rng.shuffle(extras)
        keep = req_set | set(extras[: max(0, target - len(req_set))])
        selected = [w for w in all_words if w in keep]
        out[length] = selected
    return out


def write_dictionary(path: Path, words_by_length: dict[int, list[str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    words = [w for _, ws in sorted(words_by_length.items()) for w in ws]
    path.write_text("\n".join(words) + "\n")


def safe_scale(scale: float) -> str:
    return f"{scale:.4f}".rstrip("0").rstrip(".").replace(".", "p")


def ensure_plan(args: argparse.Namespace) -> dict[str, Any]:
    repo = Path(args.repo_root).resolve()
    root = result_root(args)
    analysis = root / "analysis"
    generated = root / "generated"
    analysis.mkdir(parents=True, exist_ok=True)
    generated.mkdir(parents=True, exist_ok=True)
    plan_path = analysis / "plan.json"
    if plan_path.exists() and not args.force_generate:
        return json.loads(plan_path.read_text())

    full_words = load_words(repo)
    grids = [int(g) for g in args.grids.split(",") if g]
    scales = [float(s) for s in args.scales.split(",") if s]
    solve_binary = repo / args.solve_binary
    parsed_grids = load_grids(repo)
    dictionaries: list[dict[str, Any]] = []
    required_summary: dict[str, Any] = {}

    for grid in grids:
        _, _, slots = parse_slots(parsed_grids[grid])
        lengths = {s.length for s in slots}
        required = required_words_for_grid(repo, solve_binary, grid, args.solve_timeout)
        required_summary[str(grid)] = {str(k): sorted(v) for k, v in required.items()}
        for scale in scales:
            words = make_filtered_words(full_words, required, scale, args.seed + grid * 1009)
            dict_rel = Path("generated") / f"grid-{grid}" / f"dict-scale-{safe_scale(scale)}.txt"
            write_dictionary(root / dict_rel, words)
            stats = tuple_stats(words, lengths)
            dictionaries.append({
                "grid": grid,
                "scale": scale,
                "dict_rel": str(dict_rel),
                "lengths": sorted(lengths),
                "required_counts": {str(k): len(v) for k, v in required.items()},
                "stats": stats,
            })

    variants = {
        name: path for name, path in (v.split("=", 1) for v in args.variants.split(",") if v)
    }
    cases: list[dict[str, Any]] = []
    for d in dictionaries:
        for variant, binary in variants.items():
            for rep in range(args.repetitions):
                cases.append({
                    **d,
                    "variant": variant,
                    "binary": binary,
                    "repetition": rep,
                })

    plan = {
        "created_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(repo),
        "grids": grids,
        "scales": scales,
        "variants": variants,
        "iterations": args.iterations,
        "samples": args.samples,
        "node_cutoff": args.node_cutoff,
        "seed": args.seed,
        "required_words": required_summary,
        "cases": cases,
    }
    plan_path.write_text(json.dumps(plan, indent=2, sort_keys=True) + "\n")
    return plan


def reported_runtime(stdout: str) -> float | None:
    matches = RUNTIME_RE.findall(stdout)
    return float(matches[-1]) if matches else None


def run_case(repo: Path, root: Path, plan: dict[str, Any], case: dict[str, Any], force: bool) -> dict[str, Any]:
    runs = root / "runs"
    runs.mkdir(parents=True, exist_ok=True)
    scale_id = safe_scale(float(case["scale"]))
    run_id = f"g{case['grid']}_s{scale_id}_{case['variant']}_r{case['repetition']}"
    json_path = runs / f"{run_id}.json"
    stdout_path = runs / f"{run_id}.stdout"
    stderr_path = runs / f"{run_id}.stderr"
    if json_path.exists() and not force:
        try:
            data = json.loads(json_path.read_text())
            if data.get("status") in {"ok", "failed", "timeout", "error"}:
                return data
        except json.JSONDecodeError:
            pass

    cmd = [
        str(repo / case["binary"]),
        "-model", "tuple-set",
        "-mode", "time",
        "-iterations", str(plan["iterations"]),
        "-samples", str(plan["samples"]),
        "-file", str(root / case["dict_rel"]),
        str(case["grid"]),
    ]
    if int(plan.get("node_cutoff", 0)) > 0:
        cmd[7:7] = ["-node", str(plan["node_cutoff"])]
    start = time.perf_counter()
    status = "ok"
    try:
        cp = subprocess.run(cmd, cwd=repo, text=True, capture_output=True, timeout=plan.get("timeout", 120.0))
        rc = cp.returncode
        stdout = cp.stdout
        stderr = cp.stderr
        if rc != 0:
            status = "failed"
    except subprocess.TimeoutExpired as e:
        rc = -1
        stdout = e.stdout if isinstance(e.stdout, str) else ""
        stderr = e.stderr if isinstance(e.stderr, str) else ""
        status = "timeout"
    elapsed = time.perf_counter() - start
    stdout_path.write_text(stdout)
    stderr_path.write_text(stderr)
    data = {
        "run_id": run_id,
        "status": status,
        "returncode": rc,
        "command": cmd,
        "elapsed_seconds": elapsed,
        "reported_ms": reported_runtime(stdout),
        "stdout": str(stdout_path.relative_to(root)),
        "stderr": str(stderr_path.relative_to(root)),
        "case": case,
    }
    json_path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    return data


def cmd_generate(args: argparse.Namespace) -> None:
    plan = ensure_plan(args)
    print(f"generated {len({c['dict_rel'] for c in plan['cases']})} dictionaries")
    print(result_root(args) / "analysis" / "plan.json")


def cmd_run(args: argparse.Namespace) -> None:
    plan = ensure_plan(args)
    plan["timeout"] = args.timeout
    root = result_root(args)
    repo = Path(args.repo_root).resolve()
    cases = plan["cases"][: args.limit if args.limit else None]
    (root / "analysis" / "plan.json").write_text(json.dumps(plan, indent=2, sort_keys=True) + "\n")
    if args.dry_run:
        for c in cases:
            print(c["grid"], c["scale"], c["variant"], c["dict_rel"])
        return
    for i, case in enumerate(cases, 1):
        data = run_case(repo, root, plan, case, args.force)
        print(f"[{i}/{len(cases)}] {data['run_id']}: {data['status']} {data.get('reported_ms')}ms", flush=True)


def median(xs: list[float]) -> float | None:
    return statistics.median(xs) if xs else None


def cmd_analyze(args: argparse.Namespace) -> None:
    root = result_root(args)
    runs = [json.loads(p.read_text()) for p in sorted((root / "runs").glob("*.json"))]
    groups: dict[tuple[int, float, str], list[dict[str, Any]]] = {}
    for r in runs:
        c = r["case"]
        groups.setdefault((c["grid"], float(c["scale"]), c["variant"]), []).append(r)
    rows = []
    for (grid, scale, variant), rs in sorted(groups.items()):
        ok = [r for r in rs if r["status"] == "ok" and r.get("reported_ms") is not None]
        sample = rs[0]["case"]
        reported = [float(r["reported_ms"]) for r in ok]
        rows.append({
            "grid": grid,
            "scale": scale,
            "variant": variant,
            "runs": len(rs),
            "ok": len(ok),
            "reported_ms_median": median(reported),
            "reported_ms_mean": statistics.mean(reported) if reported else None,
            "dense_bytes_total": sample["stats"]["total_dense_bytes"],
            "dense_bytes_max": max(v["dense_bytes"] for v in sample["stats"]["by_length"].values()),
            "stats": sample["stats"],
        })

    comparisons = []
    by_gs: dict[tuple[int, float], dict[str, Any]] = {}
    for row in rows:
        by_gs.setdefault((row["grid"], row["scale"]), {})[row["variant"]] = row
    for (grid, scale), variants in sorted(by_gs.items()):
        if "dense" in variants and "compressed" in variants:
            d = variants["dense"]["reported_ms_median"]
            c = variants["compressed"]["reported_ms_median"]
            if d and c:
                comparisons.append({
                    "grid": grid,
                    "scale": scale,
                    "dense_ms": d,
                    "compressed_ms": c,
                    "compressed_over_dense": c / d,
                    "dense_bytes_total": variants["dense"]["dense_bytes_total"],
                    "dense_bytes_max": variants["dense"]["dense_bytes_max"],
                })
    summary = {
        "created_at": datetime.now(timezone.utc).isoformat(),
        "host": platform.node(),
        "platform": platform.platform(),
        "runs": len(runs),
        "rows": rows,
        "comparisons": comparisons,
    }
    out = root / "analysis" / "summary.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(out)


def fmt_ms(v: float | None) -> str:
    return "n/a" if v is None else f"{v:.3f}"


def cmd_report(args: argparse.Namespace) -> None:
    root = result_root(args)
    summary = json.loads((root / "analysis" / "summary.json").read_text())
    report = root / "reports" / "report.md"
    report.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Crossword TupleSet Benchmark",
        "",
        f"- Runs: {summary['runs']}",
        f"- Generated: {summary['created_at']}",
        "",
        "## Dense vs Dense-Compressed",
        "",
        "| Grid | Scale | Max dense bytes | Total dense bytes | Dense ms | Compressed ms | Compressed/Dense |",
        "|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for c in summary["comparisons"]:
        lines.append(
            f"| {c['grid']} | {c['scale']:.3f} | {c['dense_bytes_max']} | "
            f"{c['dense_bytes_total']} | {fmt_ms(c['dense_ms'])} | "
            f"{fmt_ms(c['compressed_ms'])} | {c['compressed_over_dense']:.3f} |"
        )
    report.write_text("\n".join(lines) + "\n")
    print(report)


def add_common(p: argparse.ArgumentParser) -> None:
    p.add_argument("--name", required=True)
    p.add_argument("--repo-root", default=str(repo_default()))
    p.add_argument("--results-root", default="results")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)

    gen = sub.add_parser("generate")
    add_common(gen)
    gen.add_argument("--grids", default=",".join(map(str, DEFAULT_GRIDS)))
    gen.add_argument("--scales", default=",".join(map(str, DEFAULT_SCALES)))
    gen.add_argument("--variants", default=",".join(f"{k}={v}" for k, v in DEFAULT_VARIANTS.items()))
    gen.add_argument("--solve-binary", default="build/rebase-check/bin/crossword")
    gen.add_argument("--solve-timeout", type=float, default=120.0)
    gen.add_argument("--iterations", type=int, default=10)
    gen.add_argument("--samples", type=int, default=3)
    gen.add_argument("--node-cutoff", type=int, default=0)
    gen.add_argument("--repetitions", type=int, default=3)
    gen.add_argument("--seed", type=int, default=1)
    gen.add_argument("--force-generate", action="store_true")
    gen.set_defaults(func=cmd_generate)

    run = sub.add_parser("run")
    add_common(run)
    run.add_argument("--grids", default=",".join(map(str, DEFAULT_GRIDS)))
    run.add_argument("--scales", default=",".join(map(str, DEFAULT_SCALES)))
    run.add_argument("--variants", default=",".join(f"{k}={v}" for k, v in DEFAULT_VARIANTS.items()))
    run.add_argument("--solve-binary", default="build/rebase-check/bin/crossword")
    run.add_argument("--solve-timeout", type=float, default=120.0)
    run.add_argument("--iterations", type=int, default=10)
    run.add_argument("--samples", type=int, default=3)
    run.add_argument("--node-cutoff", type=int, default=0)
    run.add_argument("--repetitions", type=int, default=3)
    run.add_argument("--seed", type=int, default=1)
    run.add_argument("--force-generate", action="store_true")
    run.add_argument("--force", action="store_true")
    run.add_argument("--dry-run", action="store_true")
    run.add_argument("--limit", type=int, default=0)
    run.add_argument("--timeout", type=float, default=120.0)
    run.set_defaults(func=cmd_run)

    analyze = sub.add_parser("analyze")
    add_common(analyze)
    analyze.set_defaults(func=cmd_analyze)

    report = sub.add_parser("report")
    add_common(report)
    report.set_defaults(func=cmd_report)

    args = parser.parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
