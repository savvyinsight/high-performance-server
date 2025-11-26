#!/usr/bin/env bash
# Run a set of stress-test experiments against the server.
# Usage: ./scripts/run_experiments.sh [output_dir]

# make paths relative to repo root (script lives in scripts/)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}/.."
cd "${REPO_ROOT}"

OUTDIR=${1:-experiments}
mkdir -p "$OUTDIR"
# configurations to try (thread counts)
THREAD_COUNTS=(1 2 4 8 16)

# stress test parameters (tweak as needed)
CLIENTS=100
MSGS=100
SIZE=256

# Whether to apply sysctl tuning (requires sudo). Set to 1 to attempt.
APPLY_SYSCTL=0

if [ "$APPLY_SYSCTL" -eq 1 ]; then
  echo "Applying sysctl tuning (requires sudo)..."
  sudo sysctl -w net.core.somaxconn=10240
  sudo sysctl -w net.ipv4.tcp_tw_reuse=1
  sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"
fi

echo "Building project..."
mkdir -p build
cd build
cmake ..
make -j
cd - >/dev/null

for T in "${THREAD_COUNTS[@]}"; do
  echo "===> Running experiment: threads=$T"
  # kill existing server if any
  pkill -f high_performance_server || true
  sleep 0.2

  ./build/high_performance_server "$T" &
  SERVER_PID=$!
  echo "Server PID=$SERVER_PID"
  # wait a little for server to start
  sleep 0.5

  OUTFILE="$OUTDIR/result_threads_${T}_$(date +%Y%m%d-%H%M%S).txt"
  echo "Running stress_test.py -> $OUTFILE"
  python3 tests/stress_test.py --clients "$CLIENTS" --msgs "$MSGS" --size "$SIZE" > "$OUTFILE" 2>&1 || true

  echo "Killing server PID=$SERVER_PID"
  kill "$SERVER_PID" || true
  wait "$SERVER_PID" 2>/dev/null || true
  sleep 0.2
done

echo "Experiments finished. Results are in $OUTDIR"
