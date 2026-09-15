#!/usr/bin/env bash
# Run the upstream Pawn compiler test suite against a built pawncc.
# Usage: tools/run-tests.sh <build_dir> [test_name ...]
#   <build_dir>  CMake build directory containing pawncc and pawndisasm
#   [test_name]  optional test names to run (default: all)
set -euo pipefail

if [[ $# -lt 1 || "${1:-}" == "--help" ]]; then
  sed -n '2,5p' "$0"
  exit 1
fi

BUILD_DIR="$1"
shift || true

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PAWNCC="${BUILD_DIR}/pawncc"
PAWNDISASM="${BUILD_DIR}/pawndisasm"

if [[ ! -x "${PAWNCC}" ]]; then
  echo "error: ${PAWNCC} not found or not executable" >&2
  echo "hint: build first with cmake, e.g. cmake -S compiler -B build && cmake --build build" >&2
  exit 1
fi
PAWNDISASM_ARG=()
if [[ -x "${PAWNDISASM}" ]]; then
  PAWNDISASM_ARG=(-d "${PAWNDISASM}")
fi

cd "${REPO_ROOT}/compiler/source/compiler/tests"
exec python3 run_tests.py -c "${PAWNCC}" "${PAWNDISASM_ARG[@]}" \
  -i ../../../include "$@"
