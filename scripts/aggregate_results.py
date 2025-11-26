#!/usr/bin/env python3
"""
Aggregate experiment output files into CSV and JSON for easy comparison.

Usage:
  scripts/aggregate_results.py --dir experiments --out-csv experiments/summary.csv --out-json experiments/summary.json

It parses files produced by `scripts/run_experiments.sh` (stdout/stderr of `tests/stress_test.py`) and extracts:
  - clients, msgs_per_client, msg_size
  - total_messages, errors, duration (s), ops/s
  - latency percentiles (mean, p50, p95, p99, max) in ms

If a file does not contain parseable metrics it is skipped with a warning.
"""
import argparse
import glob
import json
import os
import re
import csv
from typing import List

parser = argparse.ArgumentParser(description='Aggregate experiment result files')
parser.add_argument('--dir', required=True, help='Directory containing experiment result files')
parser.add_argument('--out-csv', required=False, default=None, help='CSV output path')
parser.add_argument('--out-json', required=False, default=None, help='JSON output path')
args = parser.parse_args()

def parse_file(path: str):
    with open(path, 'r', errors='ignore') as f:
        txt = f.read()

    # Stress header
    m1 = re.search(r"Stress test finished:\s*clients=(\d+)\s+msgs/client=(\d+)\s+msg_size=(\d+)", txt)
    if not m1:
        return None
    clients = int(m1.group(1))
    msgs = int(m1.group(2))
    msg_size = int(m1.group(3))

    # Totals line: accept flexible separators
    m2 = re.search(r"Total messages:\s*(\d+)\s+errors:\s*(\d+)\s+duration=([\d\.]+)s\s+ops/s=([\d\.]+)", txt)
    if not m2:
        # try alternate pattern with commas
        m2 = re.search(r"Total messages:\s*(\d+),\s*errors:\s*(\d+),\s*duration=([\d\.]+)s,\s*ops/s=([\d\.]+)", txt)
    if not m2:
        return None
    total_messages = int(m2.group(1))
    errors = int(m2.group(2))
    duration_s = float(m2.group(3))
    ops_per_s = float(m2.group(4))

    # Latency line
    m3 = re.search(r"Latency ms:\s*mean=([\d\.]+)\s+p50=([\d\.]+)\s+p95=([\d\.]+)\s+p99=([\d\.]+)\s+max=([\d\.]+)", txt)
    mean_ms = p50 = p95 = p99 = max_ms = None
    if m3:
        mean_ms = float(m3.group(1))
        p50 = float(m3.group(2))
        p95 = float(m3.group(3))
        p99 = float(m3.group(4))
        max_ms = float(m3.group(5))

    return {
        'file': os.path.basename(path),
        'clients': clients,
        'msgs_per_client': msgs,
        'msg_size': msg_size,
        'total_messages': total_messages,
        'errors': errors,
        'duration_s': duration_s,
        'ops_per_s': ops_per_s,
        'mean_ms': mean_ms,
        'p50_ms': p50,
        'p95_ms': p95,
        'p99_ms': p99,
        'max_ms': max_ms,
    }

def main():
    d = args.dir
    if not os.path.isdir(d):
        print(f"ERROR: directory does not exist: {d}")
        return

    files = sorted(glob.glob(os.path.join(d, '*')))
    rows: List[dict] = []
    skipped = []
    for p in files:
        if os.path.isdir(p):
            continue
        res = parse_file(p)
        if res is None:
            skipped.append(os.path.basename(p))
        else:
            rows.append(res)

    if not rows:
        print(f"No parseable result files found in {d}. Skipped: {len(skipped)} files")
        return

    # default outputs
    out_csv = args.out_csv or os.path.join(d, 'summary.csv')
    out_json = args.out_json or os.path.join(d, 'summary.json')

    # write CSV
    keys = ['file','clients','msgs_per_client','msg_size','total_messages','errors','duration_s','ops_per_s','mean_ms','p50_ms','p95_ms','p99_ms','max_ms']
    with open(out_csv, 'w', newline='') as cf:
        writer = csv.DictWriter(cf, fieldnames=keys)
        writer.writeheader()
        for r in rows:
            writer.writerow({k: r.get(k) for k in keys})

    with open(out_json, 'w') as jf:
        json.dump(rows, jf, indent=2)

    print(f"Wrote {len(rows)} results to {out_csv} and {out_json}")
    if skipped:
        print(f"Skipped {len(skipped)} files (unparseable): {skipped[:10]}")

if __name__ == '__main__':
    main()
