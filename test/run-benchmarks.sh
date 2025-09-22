#!/usr/bin/env bash
set -euo pipefail

# Benchmark runner for RECAP. Times repeated executions of recap against the
# sample test dataset and reports Criterion-style statistics.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RECAP_BIN="${RECAP_BIN:-$REPO_ROOT/recap}"
TMPROOT="$(mktemp -d /tmp/recap-bench.XXXXXX)"
trap 'rm -rf "$TMPROOT"' EXIT

RUNS=100
SHOW_RUNS=0

usage() {
  cat <<USAGE
Usage: run-benchmarks.sh [options]
  -n, --runs N       Number of benchmark runs (default: 100)
      --show-runs    Print the duration for each individual run
  -h, --help         Show this help and exit
USAGE
}

while (($#)); do
  case "$1" in
    -n|--runs)
      shift
      if (($# == 0)); then
        echo "Error: --runs requires a value" >&2
        exit 1
      fi
      if ! [[ "$1" =~ ^[0-9]+$ ]] || (( $1 <= 0 )); then
        echo "Error: --runs must be a positive integer" >&2
        exit 1
      fi
      RUNS=$1
      ;;
    --show-runs)
      SHOW_RUNS=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: unknown option '$1'" >&2
      usage >&2
      exit 1
      ;;
  esac
  shift
done

build_if_needed() {
  if [ ! -x "$RECAP_BIN" ]; then
    echo "Building recap (needs pcre2, libcurl, jansson dev headers)..."
    (cd "$REPO_ROOT" && make) || { echo "Build failed"; exit 2; }
  fi
}

build_if_needed
cp -r "$REPO_ROOT/test" "$TMPROOT/test"

pushd "$TMPROOT" >/dev/null

run_once() {
  python3 - "$RECAP_BIN" <<'PY'
import subprocess
import sys
import time

recap_bin = sys.argv[1]

try:
    perf_counter_ns = time.perf_counter_ns  # Python 3.7+
except AttributeError:
    def perf_counter_ns():
        return int(time.perf_counter() * 1_000_000_000)

start = perf_counter_ns()
subprocess.run([recap_bin, "test"], check=True, stdout=subprocess.DEVNULL)
end = perf_counter_ns()
print(end - start)
PY
}

durations=()
next_pct=10
for ((i = 1; i <= RUNS; i++)); do
  ns=$(run_once)
  durations+=("$ns")
  if ((SHOW_RUNS)); then
    python3 - <<'PY' "$ns" "$i"
import sys
ns = int(sys.argv[1])
run = int(sys.argv[2])
print(f"Run {run:2d}: {ns / 1e6:8.3f} ms")
PY
  fi
  current_pct=$((i * 100 / RUNS))
  while ((current_pct >= next_pct && next_pct <= 100)); do
    printf '%3d%%\n' "$next_pct" >&2
    next_pct=$((next_pct + 10))
  done
done

# Ensure we always show 100% even when RUNS == 0 (should not happen but stay safe)
if ((next_pct <= 100)); then
  while ((next_pct <= 100)); do
    printf '%3d%%\n' "$next_pct" >&2
    next_pct=$((next_pct + 10))
  done
fi

popd >/dev/null

python3 - <<'PY' "$RUNS" "${durations[@]}"
import statistics
import sys

runs = int(sys.argv[1])
values_ns = [int(x) for x in sys.argv[2:]]
values_ms = [ns / 1e6 for ns in values_ns]
min_v = min(values_ms)
max_v = max(values_ms)
mean_v = statistics.fmean(values_ms)
std_v = statistics.pstdev(values_ms) if runs > 1 else 0.0
median_v = statistics.median(values_ms)

print(f"Benchmark recap test ({runs} runs)")
print(f"time:   [ {min_v:8.3f} ms  {mean_v:8.3f} ms  {max_v:8.3f} ms ]")
print(f"median: {median_v:8.3f} ms")
print(f"stddev: {std_v:8.3f} ms")
PY
