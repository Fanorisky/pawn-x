# Compiler Test Baseline

Recorded 2026-09-15, at commit `169f65d` (vendored compiler as imported;
no pawn-x modifications to compiler code). This is the "known good"
reference for later experiments: a change to `compiler/` must not grow
this failure list.

## Build configuration

The upstream CMake root is `compiler/source/compiler` (not `compiler/`),
and on this 64-bit Linux host the AMX sources fail with
`#error Unsupported cell size` unless built for 32-bit cells. Upstream's
own Linux builds (`.travis.yml`, `docker/docker-entrypoint.sh`) use
`-m32`, so we match that:

```bash
cmake -S compiler/source/compiler -B build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32"
cmake --build build -j"$(nproc)"
tools/run-tests.sh build
```

Result: `pawncc`, `pawndisasm`, and `pawnruns` build successfully
(32-bit ELF, gcc 14.2.0, cmake 3.31).

Note: building with `-DsNAMEMAX=63` (as upstream's docker build does)
makes `__pragma` fail as well (it expects the default 31-character
symbol-truncation warning), so the baseline uses plain `-m32`.

## Test results

Updated 2026-09-15 after the fix round for `tools/run-tests.sh`
(commit `7cfd1eb` + fix): the script now resolves relative build-dir
paths and accepts an optional `-r <pawnruns>` argument, so both
invocation forms below were run from the repo root with relative paths.

**Without the runner** — `tools/run-tests.sh build`:
**90 PASSED, 3 FAILED** out of 93.

**With the runner** — `tools/run-tests.sh -r build/pawnruns build`:
**91 PASSED, 2 FAILED** out of 93.

| Test | Type | Without `-r` | With `-r` | Cause |
|---|---|---|---|---|
| `gh_353_symbol_suggestions` | output_check | FAIL | FAIL | Expected no suggestion for `float` (line 30); compiler suggests `fstat` (a native from `file.inc`, Levenshtein distance 2 = the threshold). Genuine behavioral difference from the recorded expectation; upstream's current CI does not run this suite on Linux, so this may be a latent upstream/platform issue. |
| `__timestamp` | runtime | FAIL (runner not set) | FAIL | The runtime output contains an extra blank line between `result: 0` and `__timestamp.amx returns 0`, so it fails the string comparison. Cosmetic platform deviation in the runtime output, not a compiler defect. |
| `runtime_test_example` | runtime | FAIL (runner not set) | PASS | Only failed previously because no runner (`-r`) was passed; it passes with `pawnruns`. |

All other 90 tests pass under both invocation forms. The remaining
runtime failure (`__timestamp`) is a whitespace mismatch in the test
runner's output comparison, not the compiler;
`gh_353_symbol_suggestions` is the only genuine compiler behavior
deviation observed.
