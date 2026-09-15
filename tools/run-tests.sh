#!/usr/bin/env bash
# Run the upstream Pawn compiler test suite against a built pawncc.
# Usage: tools/run-tests.sh [-r <runner>] <build_dir> [test_name ...]
#   <build_dir>  CMake build directory containing pawncc and pawndisasm
#   [test_name]  optional test names to run (default: all)
#   -r <runner>  optional pawnruns executable; enables the runtime tests
set -euo pipefail

BUILD_HINT='hint: build first with cmake, e.g. cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32" && cmake --build build -j$(nproc)'

RUNNER=""
if [[ "${1:-}" == "-r" ]]; then
  if [[ $# -lt 2 || "$2" == "--help" ]]; then
    sed -n '2,6p' "$0"
    exit 1
  fi
  if [[ ! -x "$2" ]]; then
    echo "error: $2 not found or not executable" >&2
    exit 1
  fi
  RUNNER="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
  shift 2
fi

if [[ $# -lt 1 || "${1:-}" == "--help" ]]; then
  sed -n '2,6p' "$0"
  exit 1
fi

if [[ ! -d "$1" ]]; then
  echo "error: $1 is not a directory" >&2
  echo "${BUILD_HINT}" >&2
  exit 1
fi
BUILD_DIR="$(cd "$1" && pwd)"
shift || true

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PAWNCC="${BUILD_DIR}/pawncc"
PAWNDISASM="${BUILD_DIR}/pawndisasm"

if [[ ! -x "${PAWNCC}" ]]; then
  echo "error: ${PAWNCC} not found or not executable" >&2
  echo "${BUILD_HINT}" >&2
  exit 1
fi
PAWNDISASM_ARG=()
if [[ -x "${PAWNDISASM}" ]]; then
  PAWNDISASM_ARG=(-d "${PAWNDISASM}")
fi
PAWNRUNS_ARG=()
if [[ -n "${RUNNER}" ]]; then
  PAWNRUNS_ARG=(-r "${RUNNER}")
fi

cd "${REPO_ROOT}/compiler/source/compiler/tests"
exec python3 run_tests.py -c "${PAWNCC}" "${PAWNDISASM_ARG[@]}" \
  "${PAWNRUNS_ARG[@]}" -i ../../../include "$@"
