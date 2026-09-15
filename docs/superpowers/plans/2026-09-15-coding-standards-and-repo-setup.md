# pawn-x Coding Standards & Repo Setup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the pawn-x repository (standalone, pushed to GitHub) with a complete, enforceable coding standards document covering C (compiler), Pawn (.pwn), and tooling (shell/Python), so all subsequent compiler experiments follow one set of rules.

**Architecture:** pawn-lab becomes a standalone git repo named pawn-x. Vendored code (`compiler/`, `ysi-5/`) keeps its original CompuPhase/Y-Less style — new compiler code must match it so diffs against upstream `openmultiplayer/compiler` stay clean. The standards live in `docs/CODING_STANDARDS.md`, enforced by `.editorconfig` and a small verification script. The existing test system (`compiler/source/compiler/tests/run_tests.py`, 94 `.pwn` + `.meta` tests) is the canonical regression gate for all compiler changes.

**Tech Stack:** C89/C99 (compiler source), Pawn 3.10 (experiments), Python 2/3-compatible test runner, CMake build, git + gh CLI.

**Spec:** This plan is self-contained (the deliverable IS documentation + repo scaffolding; it was agreed in-session: standalone repo, follow existing code style, cover all languages).

## Global Constraints

- Repository: standalone, single repo, name `pawn-x`, hosted at `https://github.com/Fanorisky/pawn-x` (already created, empty).
- Working directory: `/home/amba/pawn-lab` — this directory becomes the repo root.
- Vendored upstreams (record origins in CREDITS.md): `compiler/` from https://github.com/openmultiplayer/compiler, `ysi-5/` from https://github.com/Y-Less/YSI-Includes. Their internal `.git` directories are REMOVED (vendored, not submodules).
- New C code MUST match the existing CompuPhase style (verified against `compiler/source/compiler/sc5.c:488-510`): Allman braces, 2-space indent, `snake_case`, single-statement `if`/`for` without braces, no space after comma, no spaces around `=` in assignments.
- Never reformat existing compiler code — diffs against upstream must stay minimal.
- Public repo: all documentation written in English.
- Binary/bulk artifacts are never committed: `openmp/`, `*.tar.gz`, `*.amx`, build dirs.

---

### Task 1: Initialize pawn-x repository

**Files:**
- Create: `/home/amba/pawn-lab/.gitignore`
- Create: `/home/amba/pawn-lab/README.md`
- Create: `/home/amba/pawn-lab/CREDITS.md`
- Delete: `/home/amba/pawn-lab/compiler/.git` (directory)
- Delete: `/home/amba/pawn-lab/ysi-5/.git` (directory)

**Interfaces:**
- Consumes: nothing (first task).
- Produces: a git repo at `/home/amba/pawn-lab` with `main` branch pushed to `Fanorisky/pawn-x`; paths `compiler/`, `ysi-5/` committed as vendored source; paths `openmp/`, `openmp-server.tar.gz` present locally but ignored.

- [ ] **Step 1: Remove vendored .git directories**

```bash
rm -rf /home/amba/pawn-lab/compiler/.git /home/amba/pawn-lab/ysi-5/.git
```

Verify: `ls -a /home/amba/pawn-lab/compiler | grep -c '^\.git$'` outputs `0`.

- [ ] **Step 2: Write .gitignore**

Create `/home/amba/pawn-lab/.gitignore` with exactly:

```gitignore
# Server binaries and archives (downloaded, not vendored)
openmp/
*.tar.gz

# Build artifacts
build/
*.o
*.a
*.so

# Compiler output
*.amx
*.asm

# Python
__pycache__/
*.pyc

# Editors
.vscode/
.idea/
*.swp
```

- [ ] **Step 3: Write README.md**

Create `/home/amba/pawn-lab/README.md` with exactly:

```markdown
# pawn-x

Experiments and improvements to the [Pawn compiler](https://github.com/openmultiplayer/compiler)
(open.mp / SA-MP). Goal: implement natively in the compiler the language
capabilities that [YSI Includes](https://github.com/Y-Less/YSI-Includes)
currently patch from the script side (timers, inline functions, iterators,
hooks, varargs), so scripts no longer need those patches.

## Layout

| Path | Contents |
|---|---|
| `compiler/` | Vendored Pawn Community Compiler source (C) |
| `ysi-5/` | Vendored YSI 5 includes (reference for what to replace) |
| `openmp/` | open.mp server package (downloaded, gitignored) |
| `experiments/` | Experiment scripts and results |
| `docs/` | Standards, notes, experiment reports |

See `docs/CODING_STANDARDS.md` before contributing code.

## License notice

The vendored compiler is Copyright (c) ITB CompuPhase 1997-2006, modified by
the community — see `compiler/license.txt`. YSI is MPL 1.1 — see `CREDITS.md`.
```

- [ ] **Step 4: Write CREDITS.md**

Create `/home/amba/pawn-lab/CREDITS.md` with exactly:

```markdown
# Credits / Vendored Code Origins

This repository vendors third-party code. Origins and licenses:

| Path | Origin | License |
|---|---|---|
| `compiler/` | https://github.com/openmultiplayer/compiler (clone of 2026-09-15, Pawn Community Compiler, originally by CompuPhase / Zeex) | see `compiler/license.txt` |
| `ysi-5/` | https://github.com/Y-Less/YSI-Includes (clone of 2026-09-15, YSI 5) | MPL 1.1 |

Update this table when re-vendoring a newer upstream version.
```

- [ ] **Step 5: Init git, commit, push**

```bash
cd /home/amba/pawn-lab
git init -b main
git add .gitignore README.md CREDITS.md compiler/ ysi-5/
git commit -m "chore: initialize pawn-x with vendored compiler and YSI 5"
git remote add origin https://github.com/Fanorisky/pawn-x.git
git push -u origin main
```

Verify: `git status` shows clean except ignored files; `gh repo view Fanorisky/pawn-x --json name -q .name` outputs `pawn-x`.

---

### Task 2: Write the C section of CODING_STANDARDS.md

**Files:**
- Create: `/home/amba/pawn-lab/docs/CODING_STANDARDS.md`

**Interfaces:**
- Consumes: repo from Task 1.
- Produces: `docs/CODING_STANDARDS.md` with sections 1–3 (intro, C rules, C testing rules). Tasks 3–4 append further sections to this same file.

- [ ] **Step 1: Write the standards document (C part)**

Create `/home/amba/pawn-lab/docs/CODING_STANDARDS.md` with exactly:

```markdown
# pawn-x Coding Standards

These rules govern all code in this repository. They exist so that changes
stay reviewable and diffs against upstream stay minimal.

## 1. General Principles

1. **Match the neighbors.** Code that lands inside `compiler/` follows the
   original CompuPhase style described below — *even where a modern style
   guide would say otherwise*. A clean diff against
   https://github.com/openmultiplayer/compiler is worth more than taste.
2. **Never reformat existing code.** No drive-by whitespace, brace, or
   renaming changes mixed into functional commits.
3. **One responsibility per function.** If a new function exceeds ~60 lines
   or needs three levels of nested control flow, split it.
4. **English everywhere:** identifiers, comments, commit messages, docs.

## 2. C (compiler/ source)

The rules below are transcribed from the existing code
(`compiler/source/compiler/sc5.c` is the reference sample).

### 2.1 Formatting

| Rule | Convention | Example |
|---|---|---|
| Braces | Allman: opening brace on its own line | `static int f(void)\n{` |
| Indentation | 2 spaces, no tabs | — |
| Single-statement `if`/`for` | no braces | `if(b<min)\n  min=b;` |
| Spacing in calls | no space after `,` | `minimum(a,b,c)` |
| Assignment | no spaces around `=` | `dist=levenshtein_distance(name,symname);` |
| Comparisons | no spaces around operators | `if (sym->fnumber!=-1)` |
| Keyword | space after `if`, `for`, `while`, `return` when parenthesized | `if (...)`, `return x;` |
| Pointers | `*` attached to name | `symbol *sym`, `const char *s` |
| Line width | keep under 100 columns where practical | — |

New functions look like this (real style, from sc5.c):

    static int minimum(int a,int b,int c)
    {
      int min=a;
      if(b<min)
        min=b;
      if(c<min)
        min=c;
      return min;
    }

### 2.2 Naming

| Kind | Convention | Example |
|---|---|---|
| Functions | `snake_case`, verb-first | `find_closest_symbol` |
| Static (file-local) functions | `snake_case`, `static` keyword always | `static int minimum(...)` |
| Local variables | short `snake_case` | `symname`, `dist` |
| Global variables | existing file `scvars.c` pattern; avoid adding new globals | `sc_status` |
| Types/structs | existing names; new types `snake_case` + `_t` suffix acceptable | `symbol` |
| Constants/macros | `UPPERCASE` | ` LevDIS ` style tables, `#define FOLD_LEVEL 0` |

### 2.3 Comments

- `/* ... */` block headers at the top of each file (license + summary), as
  every existing `sc*.c` file has.
- Inline comments use `//` with no space after the slashes in old code; new
  comments use `// ` (with space) for readability. Both acceptable; do not
  rewrite the old ones.
- Comment *why*, not what. Assume the reader knows C.

### 2.4 Declaration rules

- Declarations at the top of a block (C89-friendly), matching existing code.
  New code may declare at point of first use *only* in a new file it owns.
- Every new file starts with the standard license header — copy it from
  `compiler/source/compiler/sc5.c` and add the pawn-x attribution line:
  `/* Modified for pawn-x (https://github.com/Fanorisky/pawn-x) */`.

### 2.5 Upstream compatibility

- New compiler features must not break existing scripts: the whole
  `compiler/source/compiler/tests/` suite (94 tests) must pass before commit.
- New language constructs are opt-in: they must not change the meaning of
  any program that compiled before, unless behind a flag or `#pragma`.

## 3. C Testing Rules

The compiler test system is `compiler/source/compiler/tests/`:
each test is a `name.pwn` script plus a `name.meta` file describing
expectations, executed by `run_tests.py` via CMake.

1. **Every behavior change ships with a test**: a new `<feature>.pwn` +
   `<feature>.meta` pair in `compiler/source/compiler/tests/`.
2. **Name tests after the feature or issue**: `varargs_native.pwn`,
   `inline_gh_123.pwn` (upstream issue numbers keep history greppable).
3. **`.meta` format** (existing, do not invent a new one):
   ```python
   {
     'test_type': 'output_check',
     'errors': """
   <exact expected compiler output>
   """
   }
   ```
4. **Test both acceptance and rejection**: one test for valid code that must
   compile, one for invalid code with the exact error line.
5. **Run the suite before every commit** (see Task 4's `tools/run-tests.sh`).
```

- [ ] **Step 2: Verify the style examples are real**

Verify the transcribed examples exist in the codebase:

```bash
grep -n "static int minimum(int a,int b,int c)" compiler/source/compiler/sc5.c
grep -n "dist=levenshtein_distance(name,symname);" compiler/source/compiler/sc5.c
```

Expected: both output one matching line each. If either fails, the standards
doc quotes code that does not exist — fix the doc to quote real code.

- [ ] **Step 3: Commit**

```bash
cd /home/amba/pawn-lab
git add docs/CODING_STANDARDS.md
git commit -m "docs: add coding standards (C section)"
```

---

### Task 3: Append the Pawn (.pwn) conventions

**Files:**
- Modify: `/home/amba/pawn-lab/docs/CODING_STANDARDS.md` (append sections 4–5)

**Interfaces:**
- Consumes: `docs/CODING_STANDARDS.md` from Task 2.
- Produces: same file extended with Pawn experiment rules and experiment-report rules.

- [ ] **Step 1: Append Pawn and experiments sections**

Append to `/home/amba/pawn-lab/docs/CODING_STANDARDS.md` exactly:

```markdown
## 4. Pawn Scripts (experiments/ and tests)

Pawn style follows the conventions visible in YSI 5 and the compiler's own
test suite (`compiler/source/compiler/tests/__addressof.pwn` is the reference).

### 4.1 Formatting

| Rule | Convention |
|---|---|
| Indentation | tabs (1 tab per level) — as in YSI and test scripts |
| Braces | Allman (own line), matching C |
| Include guard | `#if defined _INC_<name>` + `#endinput` + `#define _INC_<name>` |
| Line width | keep under 100 columns |

Reference include guard shape (from y_timers.inc):

    #if defined _INC_y_timers
      #endinput
    #endif
    #define _INC_y_timers

### 4.2 Naming

| Kind | Convention | Example |
|---|---|---|
| Functions | `Snake_Case` (Pawn community style: capitalized words) | `GetPlayerName` |
| Our experiment natives/keywords | lowercase, prefixed per feature | `varargs_...` |
| Variables | `snake_case` | `player_count` |
| Constants | `UPPERCASE` or `CONST_CASE` | `MAX_PLAYERS` |
| Public functions (timer/callback) | descriptive, prefixed | `OnVarargsExperiment` |
| Test scripts | `feature_name.pwn` matching the `.meta` | `varargs_native.pwn` |

### 4.3 Experiment scripts

- Every experiment lives in `experiments/<NNN>-<short-name>/` (e.g.
  `experiments/001-varargs/`) containing:
  - `script.pwn` — the Pawn code under experiment
  - `RESULT.md` — what was tried, what happened, verdict
- Experiments that modify the compiler pin the compiler commit hash in
  `RESULT.md` (`git rev-parse HEAD` at run time).
- Suppress warnings explicitly (`#pragma unused x`) rather than ignoring
  them — the build log must be warning-clean.

## 5. Documentation Rules

- `docs/` holds standards, design docs (`docs/superpowers/specs/`), and
  implementation plans (`docs/superpowers/plans/`).
- Every experiment report (`RESULT.md`) answers four questions: what was
  tried, what worked, what broke, what is next.
- Commit messages: Conventional Commits (`feat:`, `fix:`, `docs:`, `chore:`,
  `test:`, `experiment:`).
```

- [ ] **Step 2: Verify against reference files**

```bash
head -4 ysi-5/YSI_Coding/y_timers.inc
grep -c "#pragma unused" compiler/source/compiler/tests/__addressof.pwn
```

Expected: first command shows the include guard exactly as quoted; second
outputs a number ≥ 1 (proving the `#pragma unused` convention is real).

- [ ] **Step 3: Commit**

```bash
cd /home/amba/pawn-lab
git add docs/CODING_STANDARDS.md
git commit -m "docs: add Pawn script and documentation conventions"
```

---

### Task 4: Tooling conventions, .editorconfig, and the test runner script

**Files:**
- Modify: `/home/amba/pawn-lab/docs/CODING_STANDARDS.md` (append section 6)
- Create: `/home/amba/pawn-lab/.editorconfig`
- Create: `/home/amba/pawn-lab/tools/run-tests.sh`

**Interfaces:**
- Consumes: `docs/CODING_STANDARDS.md` (Tasks 2–3); the upstream test runner
  at `compiler/source/compiler/tests/run_tests.py` invoked as
  `python2 run_tests.py -c <pawncc> -d <pawndisasm> -i <include_dir> [test_names...]`.
- Produces: `.editorconfig` (editor-enforced indentation rules); executable
  `tools/run-tests.sh <build_dir> [test...]` that any later task calls to run
  the compiler suite.

- [ ] **Step 1: Append tooling section to the standards**

Append to `/home/amba/pawn-lab/docs/CODING_STANDARDS.md` exactly:

```markdown
## 6. Tooling (shell, Python)

### 6.1 Shell

- `#!/usr/bin/env bash`, `set -euo pipefail` at the top of every script.
- `snake_case.sh` filenames in `tools/`.
- Every script supports `--help` and exits non-zero on failure.
- No interactive prompts in scripts — they run in CI and agents.

### 6.2 Python

- The upstream `run_tests.py` is Python-2/3 compatible; keep any script we
  add in `tools/` Python 3 only, stdlib only (no pip dependencies).
- `snake_case` functions, 2-space indent (matching upstream runner style),
  `#!/usr/bin/env python3`, `argparse` for CLI.

### 6.3 Editor enforcement

`.editorconfig` at the repo root enforces: 2-space indent for C and
Python, tabs for Pawn, final newline everywhere, UTF-8. Editors that
support EditorConfig apply this automatically; the rules mirror
sections 2–4 of this document.

### 6.4 Verification

`tools/run-tests.sh <build_dir> [test...]` builds nothing itself — it
runs the upstream suite against an already-built `pawncc` in `<build_dir>`.
Run it before every commit that touches `compiler/`.
```

- [ ] **Step 2: Write .editorconfig**

Create `/home/amba/pawn-lab/.editorconfig` with exactly:

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{c,h}]
indent_style = space
indent_size = 2

[*.{pwn,inc}]
indent_style = tab
indent_size = 4

[*.py]
indent_style = space
indent_size = 2

[*.md]
trim_trailing_whitespace = false
```

- [ ] **Step 3: Write tools/run-tests.sh**

Create `/home/amba/pawn-lab/tools/run-tests.sh` with exactly:

```bash
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
```

- [ ] **Step 4: Make it executable and test failure path**

```bash
chmod +x tools/run-tests.sh
tools/run-tests.sh --help
```

Expected: prints the usage lines (sed-extracted header), exit code 1.

```bash
tools/run-tests.sh /tmp/does-not-exist || echo "FAILED-AS-EXPECTED"
```

Expected: prints the error about missing pawncc, then `FAILED-AS-EXPECTED`.

- [ ] **Step 5: Test success path (build the compiler first)**

```bash
cd /home/amba/pawn-lab
cmake -S compiler -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
tools/run-tests.sh build
```

Expected: compiler builds (this may take a few minutes); test run reports
passing tests. If some upstream tests fail on this platform, record the
baseline failure list in `docs/TEST_BASELINE.md` — that list becomes the
"known good" reference for later experiments. Do not modify compiler code
in this task.

- [ ] **Step 6: Commit and push**

```bash
cd /home/amba/pawn-lab
git add .editorconfig tools/run-tests.sh docs/CODING_STANDARDS.md docs/TEST_BASELINE.md
git commit -m "chore: add editorconfig and compiler test runner script"
git push origin main
```

(If `docs/TEST_BASELINE.md` was not needed because all tests passed, add
only the other three files.)
```

---

## Self-Review (already performed)

1. **Spec coverage:** user asked for coding rules first — Tasks 2–4 deliver
   C, Pawn, and tooling rules; Task 1 delivers the repo the rules live in
   (standalone, per earlier decision). The experiment plan (varargs etc.) is
   deliberately out of scope here and gets its own plan after this one.
2. **Placeholder scan:** no TBD/TODO; every doc and script step contains full
   file content; verification steps name exact commands and expected output.
3. **Type consistency:** `tools/run-tests.sh <build_dir> [test...]` interface
   is defined in Task 4 section 6.4 and used identically in its own steps.
   CODING_STANDARDS.md is created in Task 2 and appended in Tasks 3–4 with
   section numbers continuing (1–3, 4–5, 6) without overlap.
