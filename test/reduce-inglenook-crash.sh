#!/bin/sh

set -u

SOURCE="/Users/zayenz/.codex/worktrees/4e83/gecode/data8.csv"
TEST_BIN="/Users/zayenz/.codex/worktrees/4e83/gecode/test/test"
WORKDIR="/tmp/inglenook-reduce"
DYLD="/Users/zayenz/.codex/worktrees/4e83/gecode"
# Test::Options pattern matching is not regex-based.
TEST_NAME="^Extensional::TupleSet::Crash::Inglenook"
MAX_GRANULARITY=64
MAX_CHUNK_TESTS=64
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUTPUT_CSV="$SCRIPT_DIR/inglenook-crash-min.csv"

usage() {
  cat <<EOF
Usage: $0 [options]
  --source PATH    Source CSV (default: $SOURCE)
  --test-bin PATH  Test binary (default: $TEST_BIN)
  --workdir PATH   Work directory (default: $WORKDIR)
  --dyld PATH      DYLD_LIBRARY_PATH value (default: $DYLD)
  --max-gran N     Max ddmin granularity (default: $MAX_GRANULARITY)
  --max-chunks N   Max chunk tests per granularity (default: $MAX_CHUNK_TESTS)
  --help           Show this message
EOF
}

log() {
  printf "%s\n" "$*" | tee -a "$LOG"
}

die() {
  log "ERROR: $*"
  exit 1
}

run_candidate() {
  candidate_csv=$1
  DYLD_LIBRARY_PATH="$DYLD" \
  GECODE_INGLENOOK_CSV="$candidate_csv" \
  "$TEST_BIN" -iter 1 -threads 1 -test "$TEST_NAME" >/dev/null 2>&1
  status=$?
  [ "$status" -eq 139 ]
}

write_candidate_from_rows() {
  rows_file=$1
  out_csv=$2
  {
    printf "From,To\n"
    cat "$rows_file"
  } > "$out_csv"
}

write_prefix_candidate() {
  prefix_rows=$1
  out_csv=$2
  {
    printf "From,To\n"
    sed -n "2,$((prefix_rows + 1))p" "$SOURCE"
  } > "$out_csv"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --source)
      [ $# -ge 2 ] || { usage >&2; exit 2; }
      SOURCE=$2
      shift 2
      ;;
    --test-bin)
      [ $# -ge 2 ] || { usage >&2; exit 2; }
      TEST_BIN=$2
      shift 2
      ;;
    --workdir)
      [ $# -ge 2 ] || { usage >&2; exit 2; }
      WORKDIR=$2
      shift 2
      ;;
    --dyld)
      [ $# -ge 2 ] || { usage >&2; exit 2; }
      DYLD=$2
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --max-gran)
      [ $# -ge 2 ] || { usage >&2; exit 2; }
      MAX_GRANULARITY=$2
      shift 2
      ;;
    --max-chunks)
      [ $# -ge 2 ] || { usage >&2; exit 2; }
      MAX_CHUNK_TESTS=$2
      shift 2
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

mkdir -p "$WORKDIR" || exit 1
LOG="$WORKDIR/reduction.log"
: > "$LOG" || exit 1

[ -f "$SOURCE" ] || die "Source CSV not found: $SOURCE"
[ -x "$TEST_BIN" ] || die "Test binary is not executable: $TEST_BIN"

total_lines=$(wc -l < "$SOURCE")
[ "$total_lines" -ge 2 ] || die "Source CSV has no data rows: $SOURCE"
total_rows=$((total_lines - 1))

log "Source: $SOURCE"
log "Rows in source: $total_rows"
log "Test binary: $TEST_BIN"
log "Output CSV: $OUTPUT_CSV"
log "ddmin max granularity: $MAX_GRANULARITY"
log "ddmin max chunk tests: $MAX_CHUNK_TESTS"

baseline_csv="$WORKDIR/baseline.csv"
cp "$SOURCE" "$baseline_csv" || exit 1
if run_candidate "$baseline_csv"; then
  log "Baseline crash confirmed."
else
  die "Baseline candidate does not crash (expected exit 139)."
fi

prefix_candidate="$WORKDIR/prefix.csv"
low=1
high=$total_rows
best=$total_rows
while [ "$low" -le "$high" ]; do
  mid=$(((low + high) / 2))
  write_prefix_candidate "$mid" "$prefix_candidate"
  if run_candidate "$prefix_candidate"; then
    best=$mid
    high=$((mid - 1))
    log "Prefix $mid rows: crashes"
  else
    low=$((mid + 1))
    log "Prefix $mid rows: no crash"
  fi
done
log "Best crashing prefix: $best rows"

current_rows_file="$WORKDIR/current.rows"
sed -n "2,$((best + 1))p" "$SOURCE" > "$current_rows_file"
current_count=$best

granularity=2
while [ "$current_count" -gt 1 ]; do
  if [ "$granularity" -gt "$current_count" ]; then
    granularity=$current_count
  fi
  chunk_size=$(((current_count + granularity - 1) / granularity))
  start=1
  reduced=0
  tested_chunks=0

  while [ "$start" -le "$current_count" ]; do
    if [ "$tested_chunks" -ge "$MAX_CHUNK_TESTS" ]; then
      log "Reached chunk test cap ($MAX_CHUNK_TESTS) at granularity $granularity"
      break
    fi

    end=$((start + chunk_size - 1))
    if [ "$end" -gt "$current_count" ]; then
      end=$current_count
    fi
    removed=$((end - start + 1))
    candidate_count=$((current_count - removed))

    if [ "$candidate_count" -ge 1 ]; then
      candidate_rows_file="$WORKDIR/candidate.rows"
      awk -v s="$start" -v e="$end" 'NR < s || NR > e { print }' \
        "$current_rows_file" > "$candidate_rows_file"
      candidate_csv="$WORKDIR/candidate.csv"
      write_candidate_from_rows "$candidate_rows_file" "$candidate_csv"

      if run_candidate "$candidate_csv"; then
        mv "$candidate_rows_file" "$current_rows_file"
        current_count=$candidate_count
        reduced=1
        granularity=2
        log "Accepted removal [$start,$end], rows now $current_count"
        break
      fi
    fi

    tested_chunks=$((tested_chunks + 1))
    start=$((end + 1))
  done

  if [ "$reduced" -eq 0 ]; then
    if [ "$granularity" -ge "$current_count" ] || \
       [ "$granularity" -ge "$MAX_GRANULARITY" ]; then
      break
    fi
    granularity=$((granularity * 2))
    if [ "$granularity" -gt "$current_count" ]; then
      granularity=$current_count
    fi
    if [ "$granularity" -gt "$MAX_GRANULARITY" ]; then
      granularity=$MAX_GRANULARITY
    fi
    log "No removable chunk at granularity, increasing to $granularity"
  fi
done

write_candidate_from_rows "$current_rows_file" "$OUTPUT_CSV"
if run_candidate "$OUTPUT_CSV"; then
  log "Final artifact still crashes."
else
  die "Final artifact does not crash anymore."
fi

log "Minimized crashing CSV rows: $current_count"
log "Wrote minimized CSV: $OUTPUT_CSV"
log "Reduction log: $LOG"
