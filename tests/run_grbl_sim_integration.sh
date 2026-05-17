#!/usr/bin/env bash
set -euo pipefail

SIM_BIN="${GRBL_SIM_BIN:-/data/projects/Simulator/build/grblHAL_sim}"
PORT="${GRBL_SIM_PORT:-2323}"

if [[ ! -x "$SIM_BIN" ]]; then
  echo "SKIP: grblHAL_sim not found at $SIM_BIN"
  exit 0
fi

cleanup() {
  if [[ -n "${SIM_PID:-}" ]]; then
    kill "$SIM_PID" >/dev/null 2>&1 || true
    wait "$SIM_PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

"$SIM_BIN" -p "$PORT" -n >/dev/null 2>&1 &
SIM_PID=$!
sleep 0.8

"$@" "$PORT"
