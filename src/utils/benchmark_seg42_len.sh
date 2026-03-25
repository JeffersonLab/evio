#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=/home/jzarling/CODA_software/evio
CPP_EXE="$ROOT_DIR/build/bin/evio_check_seg42_len"
CPP_SRC="$ROOT_DIR/src/utils/cpp/evio_check_seg42_len.cpp"
JAVA_SRC="$ROOT_DIR/src/utils/java/evio_check_seg42_len.java"
JAVA_CP="$ROOT_DIR/java/jars/jevio-6.2.0-all.jar:$ROOT_DIR/src/utils/java"

if (($# > 0)); then
  files=("$@")
else
  shopt -s nullglob
  files=("$ROOT_DIR"/tmp/*.evio)
  shopt -u nullglob
fi

if ((${#files[@]} == 0)); then
  echo "No input files found." >&2
  exit 1
fi

bytes=0
for f in "${files[@]}"; do
  bytes=$((bytes + $(stat -c %s "$f")))
done

calc_mb_s() {
  awk -v bytes="$1" -v secs="$2" 'BEGIN {if (secs > 0) printf "%.2f", (bytes/1048576.0)/secs; else printf "0.00"}'
}

run_timed() {
  local label=$1
  shift
  local start end elapsed
  start=$(date +%s.%N)
  "$@" >/dev/null
  end=$(date +%s.%N)
  elapsed=$(awk -v s="$start" -v e="$end" 'BEGIN {printf "%.6f", e-s}')
  printf "%-6s %9ss  %8s MB/s\n" "$label" "$elapsed" "$(calc_mb_s "$bytes" "$elapsed")"
}

build_cpp() {
  if cmake --build "$ROOT_DIR/build" --target evio_check_seg42_len >/dev/null 2>&1; then
    return
  fi

  /usr/sbin/c++ \
    -I"$ROOT_DIR/src/libsrc++" \
    -I"$ROOT_DIR/build/_deps/disruptor-src" \
    -O3 -DNDEBUG -std=gnu++20 -Wall \
    "$CPP_SRC" \
    -o "$CPP_EXE" \
    -Wl,-rpath,"$ROOT_DIR/build/lib" \
    "$ROOT_DIR/build/lib/libeviocc.so" \
    -lpthread \
    /usr/lib/libboost_thread.so \
    /usr/lib/libboost_chrono.so \
    /usr/lib/libboost_filesystem.so \
    /usr/lib/libboost_atomic.so \
    /usr/lib/liblz4.so \
    -lexpat -ldl -lz -lm \
    "$ROOT_DIR/build/lib/libDisruptor.so.1.0.0"
}

echo "Preparing readers..."
build_cpp

echo "Files: ${#files[@]}"
echo "Bytes: $bytes"
echo

echo "Read benchmark:"
run_timed "C++" "$CPP_EXE" "${files[@]}"

if command -v javac >/dev/null && command -v java >/dev/null; then
  javac -cp "$ROOT_DIR/java/jars/jevio-6.2.0-all.jar" "$JAVA_SRC"
  run_timed "Java" java -cp "$JAVA_CP" evio_check_seg42_len "${files[@]}"
else
  echo "Java   skipped   java/javac not found on PATH"
fi
