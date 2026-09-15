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
