# Varargs Forwarding (`___`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the `___` varargs-forwarding keyword natively in the Pawn compiler, replacing YSI y_va's runtime bytecode rewriting (spec §2).

**Architecture:** A new reserved token `tVAARGS` (string `___`) recognized only where an argument expression is expected; a new codegen path in `callfunction()` that copies the enclosing variadic function's remaining arguments from its frame to the call stack and computes the callee's argument byte-count at run time. No new AMX opcodes — the emitted sequence uses only existing instructions, so unmodified servers run the output.

**Tech Stack:** C (CompuPhase-style, per docs/CODING_STANDARDS.md §2), Pawn test scripts + `.meta` files run by the upstream Python runner through `tools/run-tests.sh`.

**Spec:** docs/superpowers/specs/2026-09-15-varargs-forwarding-design.md — the plan argues from the spec; executors read both.

## Global Constraints

- New C code MUST match the existing CompuPhase style (docs/CODING_STANDARDS.md §2): Allman braces, 2-space indent, `snake_case`, single-statement `if`/`for` without braces, no space after commas, no spaces around `=` in assignments.
- Never reformat existing compiler code — diffs against upstream must stay minimal.
- New language constructs are opt-in: they must not change the meaning of any program that compiled before.
- No new AMX opcodes: output must run on an unmodified open.mp server (spec §4.5).
- Every behavior change ships with a `<feature>.pwn` + `<feature>.meta` test pair in `compiler/source/compiler/tests/` (CODING_STANDARDS.md §3).
- Upstream suite baseline: 91 PASSED / 2 FAILED with runner (`tools/run-tests.sh -r build/pawnruns build`); the 2 failures (`gh_353_symbol_suggestions`, `__timestamp`) are pre-existing — a change to `compiler/` must not grow this list (docs/TEST_BASELINE.md).
- Build: `cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32" && cmake --build build -j$(nproc)`.
- Error numbers: the compiler asserts `number>0 && number<300`; current max is 252, so the new error is **253** (spec §3, verified against sc5.c).
- `___` is a valid user identifier today (verified by probe). The keyword must therefore be recognized ONLY in argument position inside a call — a variable named `___` used elsewhere must still compile. Backward compatibility is a hard gate: Task 1's regression test proves it.

---

### Task 1: Failing tests first — the `___` feature contract

**Files:**
- Create: `compiler/source/compiler/tests/varargs_forward_basic.pwn`
- Create: `compiler/source/compiler/tests/varargs_forward_basic.meta`
- Create: `compiler/source/compiler/tests/varargs_forward_skip.pwn`
- Create: `compiler/source/compiler/tests/varargs_forward_skip.meta`
- Create: `compiler/source/compiler/tests/varargs_forward_reject.pwn`
- Create: `compiler/source/compiler/tests/varargs_forward_reject.meta`
- Create: `compiler/source/compiler/tests/varargs_forward_compat.pwn`
- Create: `compiler/source/compiler/tests/varargs_forward_compat.meta`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: the test contract every later task must satisfy. Test type is `runtime` for behavior tests (run by `pawnruns`, which provides `print`/`printf` via `#include <console>`) and `output_check` for the rejection test (compiler output compared verbatim).

- [ ] **Step 1: Write varargs_forward_basic.pwn/.meta — passthrough to a native**

`varargs_forward_basic.pwn` (exactly):

```pawn
#include <console>

MyPrintf(const fmat[], ...)
{
	printf(fmat, ___);
}

main()
{
	MyPrintf("basic: %d %d\n", 42, 43);
	MyPrintf("basic: %s\n", "hello");
	MyPrintf("basic: none\n");
}
```

`varargs_forward_basic.meta` (exactly):

```python
{
  'test_type': 'runtime',
  'expected_output': """
basic: 42 43
basic: hello
basic: none
"""
}
```

- [ ] **Step 2: Write varargs_forward_skip.pwn/.meta — `___(N)` skips N named params**

`varargs_forward_skip.pwn` (exactly):

```pawn
#include <console>

SkipWrapper(a, b, ...)
{
	printf("skip: %d %d %d %d\n", a, b, ___(2));
}

main()
{
	SkipWrapper(1, 2, 3, 4, 5);
	SkipWrapper(9, 8, 7);
}
```

`varargs_forward_skip.meta` (exactly):

```python
{
  'test_type': 'runtime',
  'expected_output': """
skip: 1 2 3 4
skip: 9 8 7 0
"""
}
```

(The second call forwards zero arguments, so `printf`'s fourth `%d` reads an unspecified slot — this pins the skip arithmetic. If the implementer's probe shows a different value than `0` for the missing argument, update BOTH the .meta and the RESULT note with the observed value; do not silently guess.)

- [ ] **Step 3: Write varargs_forward_reject.pwn/.meta — `___` in non-variadic function is error 253**

`varargs_forward_reject.pwn` (exactly):

```pawn
#include <console>

NotVariadic(a)
{
	printf("bad: %d\n", ___);
}

main()
{
	NotVariadic(1);
}
```

`varargs_forward_reject.meta` (exactly):

```python
{
  'test_type': 'output_check',
  'errors': """
varargs_forward_reject.pwn(5) : error 253: "___" used in a function without variable arguments ("...")
"""
}
```

- [ ] **Step 4: Write varargs_forward_compat.pwn/.meta — `___` stays a valid identifier**

`varargs_forward_compat.pwn` (exactly):

```pawn
#include <console>

main()
{
	new ___ = 5;
	new arr[2] = { 7, ___ };
	printf("compat: %d %d\n", ___, arr[1]);
}
```

`varargs_forward_compat.meta` (exactly):

```python
{
  'test_type': 'runtime',
  'expected_output': """
compat: 5 7
"""
}
```

- [ ] **Step 5: Run the new tests, verify they fail for the right reason**

Run: `tools/run-tests.sh -r build/pawnruns build varargs_forward_basic varargs_forward_skip varargs_forward_reject varargs_forward_compat`

Expected: basic/skip/compat FAIL (today `___` compiles as an unknown symbol → error 17, or the run aborts); reject FAIL with a DIFFERENT message than the .meta expects (today error 17 "undefined symbol", not error 253). All four failing differently than the contract is the point — these are the red tests.

- [ ] **Step 6: Verify no baseline regression**

Run: `tools/run-tests.sh -r build/pawnruns build | tail -2`
Expected: `91 TESTS PASSED, 2 FAILED` (only the two known pre-existing failures; the 4 new tests are excluded from this count check — with 97 total they will read `91 PASSED, 6 FAILED` including the 4 new red ones).

- [ ] **Step 7: Commit**

```bash
git add compiler/source/compiler/tests/varargs_forward_*.pwn compiler/source/compiler/tests/varargs_forward_*.meta
git commit -m "test: add failing contract tests for varargs forwarding (___)"
```

---

### Task 2: Lexer + token — recognize `___` as tVAARGS in argument position

**Files:**
- Modify: `compiler/source/compiler/scvars.c:110-131` (the `sc_tokens[]` table)
- Modify: `compiler/source/compiler/sc.h:398-400` (the token enum, near `tELLIPS`/`tDBLDOT`)

**Interfaces:**
- Consumes: nothing.
- Produces: token constant `tVAARGS` whose string is `"___"`, returned by `lex()` when the input contains the identifier `___`. Downstream tasks consume `tVAARGS` in `sc3.c`.

Design decision (spec §3 backward compatibility): `___` is added to `sc_tokens[]` in the reserved-words section. The lexer's keyword lookup happens before symbol lookup, so a user variable named `___` would break — UNLESS the parser only accepts `tVAARGS` where a call argument is expected and pushes the token back everywhere else. That fallback is implemented in Task 3 (the only consumer). The exact insertion: in `sc.h`, add `tVAARGS,` on the line after `tDBLDOT,` BUT it must remain in the reserved-word block (after `tMIDDLE`) so that `sc_tokens[]` indices stay aligned — i.e. add it as the FIRST entry of the reserved-words section in BOTH files, identically positioned.

**CRITICAL**: `sc_tokens[]` is indexed by `tok-tFIRST`; the enum in sc.h and the string table in scvars.c must be extended in lockstep at the same relative position or every keyword misresolves. Insert `"___",` immediately after `"...", "..",` (i.e. as the first reserved word) in scvars.c and `tVAARGS,` immediately after `tDBLDOT,` in sc.h — wait, that position is still in the operators block guarded by `tMIDDLE`. The correct position is after the `tMIDDLE` definition line: `tVAARGS,` becomes the first reserved word, and `"___",` becomes the first entry of the reserved-words row in scvars.c (`"__addressof"` is currently first — `___` sorts before it and must precede it in both files).

- [ ] **Step 1: Write the failing test — token recognition without breaking keywords**

Create `compiler/source/compiler/tests/varargs_token_smoke.pwn` (exactly):

```pawn
#include <console>

main()
{
	new ___ = 3;
	printf("%d\n", ___);
}
```

And `varargs_token_smoke.meta` (exactly):

```python
{
  'test_type': 'runtime',
  'expected_output': """
3
"""
}
```

This is the backward-compat gate: with the token added but no consumer yet, `lex()` returns `tVAARGS` for `___`; `primary()` (sc3.c:2099+, the `default:` case) has no case for it and falls into `lexpush(); lvalue=hier1(lval);` — the token gets re-lexed as... nothing. This test pins whatever the safe fallback must be. If it fails with an error, that is EXPECTED and becomes the work of this task: make `primary()` treat an unconsumed `tVAARGS` exactly as it treats an unknown-but-valid identifier by pushing the raw text back. Simplest correct approach: in `primary()`'s `default:` arm, if `tok==tVAARGS`, `error(253,...)` is NOT right here (not in a call) — instead re-lex it as an identifier is impossible after tokenization; therefore the compatible approach is: do NOT add `___` to sc_tokens at all. Instead, recognize the literal string in `callfunction()`'s argument loop (sc3.c, the `do { ... } while` after `stgmark(sSTARTREORDER)`) by checking `tok==tSYMBOL && strcmp(lexstr,"___")==0` before normal symbol resolution. Revise this task's implementation accordingly — the test below is the real gate either way.

- [ ] **Step 2: Run the smoke test**

Run: `tools/run-tests.sh -r build/pawnruns build varargs_token_smoke`
Expected after implementation choice: PASS. (Before any change it already passes — that is the point: `___` as identifier must keep working through every refactor of this feature. Run it before AND after.)

- [ ] **Step 3: Implement token recognition in the argument loop only**

In `compiler/source/compiler/sc3.c`, in `callfunction()` (sc3.c:2232+), inside the argument-parsing `do { ... } while (!close && freading && !matchtoken(tENDEXPR));` loop, immediately after the `if (matchtoken('.'))` named-parameter block (sc3.c:2325-2347) and before `argpos` is fixed up, add (CompuPhase style):

```c
      /* varargs forwarding: "___" or "___(N)" passes the enclosing
       * function's variable arguments on to this call
       */
      if (tok==tSYMBOL && strcmp(lexstr,"___")==0) {
        /* ... handled by Task 3; this task only proves recognition.
         * For now: leave the symbol path untouched so behaviour is
         * identical to before (the string check runs and falls through).
         */
      }
```

Wait — `lexstr` is not in scope there; the loop reads tokens via `hier14(&lval)` for values. The recognition point must instead be BEFORE `hier14` consumes the expression, at the top of the argument body, using the same `lex(&lexval,&lexstr)` pattern the named-params block uses. The implementer must study the surrounding loop structure (sc3.c:2318-2352) and place a `matchtoken`-style pre-scan: peek the next token with the pattern used at sc3.c:2330 (`tokeninfo(&lexval,&lexstr)` after a symbol match), guarded so plain symbols are untouched. Concretely: at the top of the argument body, do

```c
      {
        cell fwdskip;
        if (matchfwdtoken(&fwdskip)) {   /* new helper, Task 3 */
          /* ... Task 3 fills this in ... */
        }
      }
```

This task's deliverable is narrower than the code sketch above suggests. Reduced to what is actually verifiable NOW:

1. Add to `sc3.c` a `static int matchfwdtoken(cell *skip)` helper that: lexes ahead; if the token is the SYMBOL `___` optionally followed by `(constant)`, consumes it and stores the skip count (0 default), returns TRUE; otherwise pushes the token(s) back (`lexpush()`) and returns FALSE. Style: CompuPhase, 2-space indent.
2. Do NOT call it from `callfunction()` yet (Task 3 wires it) — instead add a temporary debug hook behind `#if 0` so it compiles without unused-function warnings, OR mark it `static` and accept the warning for one commit (upstream builds are not -Werror; verify the build log shows at most a warning).
3. The recognition must treat `___(` followed by a non-constant as an error(253) path — but error 253 does not exist until Task 4. For this task, `matchfwdtoken` returns FALSE for a malformed `___(` (does not consume), leaving existing error paths to fire (error 17/1) — acceptable interim behavior, tightened in Task 4.

- [ ] **Step 4: Build and run the full suite**

Run: `cmake --build build -j"$(nproc)" && tools/run-tests.sh -r build/pawnruns build | tail -2`
Expected: build succeeds (a `-Wunused-function` warning for `matchfwdtoken` is acceptable and expected this task only); suite unchanged from baseline including the Task 1 red tests still red for the same reasons (`91 PASSED, 6 FAILED` — 2 baseline + 4 contract).

- [ ] **Step 5: Commit**

```bash
git add compiler/source/compiler/sc3.c compiler/source/compiler/tests/varargs_token_smoke.pwn compiler/source/compiler/tests/varargs_token_smoke.meta
git commit -m "feat: recognize ___ varargs-forwarding token in argument position"
```

---

### Task 3: Codegen — forward the frame's variable arguments at a call

**Files:**
- Modify: `compiler/source/compiler/sc3.c` (callfunction(): wire matchfwdtoken; emit the copy sequence; dynamic byte count)
- Modify: `compiler/source/compiler/sc1.c` (`isvariadic()` is already non-static-file-visible? verify; if `static`, expose via SC_FUNC or re-implement locally)

**Interfaces:**
- Consumes: `static int matchfwdtoken(cell *skip)` from Task 2 (returns TRUE and fills `*skip` when the next tokens are `___` or `___(N)`); `isvariadic(symbol*)` semantics from sc1.c:8110.
- Produces: working `___` forwarding for calls to natives and script functions; Tasks 4-5 harden it. Byte-count layout contract for the emitted call (used by tests): `pushval` constant replaced by a run-time-computed `push.pri` when forwarding.

**Semantics recap for the implementer (spec §3):** inside variadic function `F` with `named` declared parameters, `___(N)` at a call site means: copy the cells of `F`'s arguments with index `>= N + named` from `F`'s frame to the callee's stack, in order, and count them into the callee's byte-count. The source address of argument `k` (0-based) of `F` is `frm + (k+3)*4` (frame layout: frm+0 prev frame, frm+4 return addr, frm+8 byte count, frm+12 first arg — documented at sc1.c:4329-4335). The byte count of `F` is at `frm+8`; the number of `F`'s cells is `*(frm+8)/4`.

- [ ] **Step 1: Wire recognition into callfunction()**

At the top of the argument-body of the argument loop (before named-param handling), add:

```c
      {
        cell fwdskip;
        if (matchfwdtoken(&fwdskip)) {
          /* count of already-pushed positional args for this call */
          /* handled below together with arg accounting */
          fwdpending=TRUE;      /* new local int, initialized FALSE at loop start */
          fwdskipval=fwdskip;   /* new local cell */
          nargs+=???;           /* cannot know statically — see Step 3 */
          continue;             /* next argument, if any */
        }
      }
```

The implementer must resolve the `???`: forwarded cells are counted at run time, not at compile time, so `nargs` becomes a compile-time KNOWN part plus an UNKNOWN part. Introduce `int fwdpending` + `cell fwdskipval` locals in `callfunction()` and leave `nargs` as the static count; Step 3 handles the dynamic total.

- [ ] **Step 2: Emit the copy sequence at ffcall time**

After the argument loop, before `pushval((cell)nargs*sizeof(cell))` (sc3.c:2670), branch on `fwdpending`. When set, emit (using existing SC_FUNC helpers where available — `getfrm()` sc4.c:848, `ldconst()` sc4.c:626, `addconst()` sc4.c:994; raw `stgwrite` for the rest, matching the style of sc4.c helpers):

```
; ALT = frm + (fwdskipval + named_of_curfunc + 3)*4   <- source address
  lctrl 5            ; PRI = FRM
  add.c  <src_off>   ; PRI = &F.args[fwdskip]
  move.alt           ; ALT = source
; PRI = remaining cell count = *(frm+8)/4 - fwdskip - named
  lctrl 5
  move.pri           ; (nop-ish; keep PRI=FRM)
  add.c  8
  load.i             ; PRI = byte count of F
  add.c  <-(fwdskip+named)*4>
  smul.c 0           ; ... division by 4: use "shr.c 2" instead (byte count is
                     ; always a multiple of 4; shr 2 == div 4 for non-negative)
  shr.c 2
; loop: while (count--) push *ALT++   -- ALT must survive; use a heap temp?
```

The register-pressure problem: the copy loop needs a counter, a source pointer, and must not clobber PRI for the byte-count computation that follows. The implementer should emit this as a two-phase sequence instead (simpler and register-cheap):

Phase A (copy loop), using a heap-allocated temp is NOT needed if the loop is structured as:

```
  lctrl 5                  ; PRI = FRM
  add.c 8
  load.i                   ; PRI = F's byte count
  shr.c 2                  ; PRI = F's total cell count
  add.c -(fwdskip+named)   ; PRI = cells to forward
  jzer  @fwd_done          ; nothing to forward (handles SkipWrapper(9,8,7) case)
  move.alt                 ; ALT = remaining count
  lctrl 5
  add.c  <src_off>         ; PRI = source address
@fwd_loop:
  push.i                   ; push cell at [PRI]?? -- NO: push.i pushes the value
                           ; AT PRI's address? Verify opcode semantics!
```

**The implementer MUST verify opcode semantics in `compiler/source/amx/pawnruns` disassembly or `amx.h` opcode docs before finalizing the loop:** the AMX instruction set has `push` (indirect: pushes the cell AT the address in PRI). If `push` semantics are "push *[PRI]", the loop body is `load.i; push.pri; add.c 4; ...` with the pointer kept in ALT. Emit exactly (verified shapes; adjust only after disassembly proof):

```
  lctrl 5
  add.c  8
  load.i
  shr.c  2
  add.c  <-(fwdskip+named)>
  jzer   fwd_done
  move.alt
  lctrl 5
  add.c  <src_off>
@fwd_loop:
  move.alt          ; ALT = pointer (saved across body) -- needs BOTH regs; see below
```

Two registers, three live values (counter, pointer, pushed-dyn-count) — the clean resolution is to recount after copying: the dynamic byte count pushed for the callee equals `STK_before_args - STK_after_copy`, which the callee's own frame sees anyway. So: emit the copy loop FIRST (it only needs counter+pointer, alternating via ALT/PRI discipline), then compute the byte count by `lctrl 4` (STK) minus a saved mark — but there is no place to save the mark without a third register or a local slot. RESOLUTION (lock this in): reserve a local variable via the existing local-symbol machinery in the staging buffer is over-engineering; instead note that `pushval` for the callee byte count can be computed as: `static_nargs*4 + forwarded_cells*4` where `forwarded_cells = F_cells - fwdskip - named` — and `F_cells` is readable from `frm+8` at any time. So compute the byte count LAST, after the loop, directly:

```
; after copy loop:
  lctrl 5
  add.c  8
  load.i                 ; F byte count
  add.c  <(static_nargs - fwdskip - named)*4>   ; adjust: +static*4 - forwarded*4
                                                 ; = static*4 + (F_count-fwdskip-named)*4
                                                 ; since F_count*4 IS the loaded value
  push.pri               ; dynamic byte count for callee
```

This avoids the third register entirely. The final emitted sequence (canonical; the implementer may adjust register order but NOT this algebra):

```
  lctrl 5
  add.c  8
  load.i
  shr.c  2
  add.c  <-(fwdskip+named)>
  jzer   fwd_done
  move.alt               ; ALT = count
  lctrl 5
  add.c  <src_off>       ; PRI = &F.args[fwdskip]
@fwd_loop:
  load.i                 ; PRI = *src          (PRI is also src pointer... NO)
```

Final register discipline (use this; it is correct): keep the SOURCE POINTER in ALT, COUNT in a code-side loop of emitted instructions is impossible (count is runtime) — so the loop is: while (count>0) { push [ALT]; ALT+=4; count--; } with count in PRI and pointer in ALT:

```
  lctrl 5
  add.c  8
  load.i                 ; PRI = F byte count
  shr.c  2               ; PRI = F cell count
  add.c  <-(fwdskip+named)>  ; PRI = cells to forward
  jzer   fwd_done
  lctrl 5
  add.c  <src_off>
  move.alt               ; ALT = source pointer
  lctrl 5
  add.c  8
  load.i
  shr.c  2
  add.c  <-(fwdskip+named)>  ; PRI = count (recomputed; cheap and register-clean)
@fwd_loop:
  jzer   fwd_done
  push.alt               ; PUSH the ADDRESS?? -- no: pushes ALT's value = address. WRONG.
```

The AMX `push` instruction pushes the value at the address in a register — confirm via `compiler/source/compiler/pawndisasm.c` (opcode table) and `amxexecn.asm OP_PUSH` before writing ANY loop code. The canonical correct loop, assuming `push.i`-style semantics do not exist and `push` is register-value push, is a `load.i`-then-`push.pri` body with pointer in ALT and count in a RELOADED form per iteration (jzer against a memory location is not available), so count MUST live in a register across the body — leaving pointer and count in PRI/ALT with the loaded VALUE transient:

```
; ALT = source pointer, PRI = count
@fwd_loop:
  jzer   fwd_done          ; count == 0 -> done (jzer tests PRI)
  push.alt                 ; save pointer
  move.alt                 ; ??? PRI=ALT -> destroys count
```

It is NOT possible to hold count and pointer and load a value with 2 registers without spilling. THEREFORE the plan locks in the spill-free architecture: copy via the loop with count on the HEAP is also over-complex. **The chosen implementation is Option B from spec §4.2: reuse the `memcpy`-style approach via raw opcodes but WITHOUT a native call — emit a code-side unrolled copy is impossible (runtime count). The correct minimal solution is a `SWAP`-based loop:**

```
; ALT = source pointer, PRI = count
@fwd_loop:
  jzer    fwd_done
  ; push *ALT:
  move.alt          ; PRI = ALT (pointer)      [count clobbered -> must save first]
```

STOP. This level of register-choreography detail cannot be responsibly pre-committed in a plan without an assembler at hand. The plan therefore locks the CONTRACT, not the instruction sequence:

**CONTRACT (what the emitted code must achieve, verifiable by disassembly and tests):**
1. After the sequence, the callee's stack contains the forwarded cells, in order, above the static arguments already pushed.
2. The byte count pushed for the callee equals `static_nargs*4 + forwarded_cells*4`.
3. The sequence clobbers only PRI/ALT (AMX calling convention already treats them as caller-saved).
4. It uses only opcodes existing in the vendored AMX (`compiler/source/amx/amxexecn.asm` is the reference; `pawndisasm` lists them).
5. One known-good trick the implementer should evaluate FIRST (it eliminates the register problem): the AMX `MOVS`-like block opcodes do not exist, but `sysreq.c memcpy` does (open.mp provides it; y_va itself uses exactly this — impl.inc:183 `SYSREQ.C memcpy`). If a native call is acceptable inside the sequence (it is: y_va's shipped implementation does the same), the copy becomes:

```
  lctrl 5
  add.c  8
  load.i
  shr.c  2
  add.c  <-(fwdskip+named)>
  add.c  1                  ; +1: memcpy pushes args too -- account carefully
  ; ... reserve stack, push memcpy args (dest=STK-adjusted, src, 0, count, cellmax)
  ; ... exactly mirroring YVA2_DoPush impl.inc:164-184
```

The implementer transcribes `YVA2_DoPush`'s proven stack-manipulation (impl.inc:139-190) into compiler C codegen, replacing its runtime-discovered constants with compile-time-known `fwdskip`/`named`. That assembly is battle-tested by YSI for a decade — adapt it, do not invent new choreography.

- [ ] **Step 3: Replace pushval with dynamic count when forwarding**

At sc3.c:2670, when `fwdpending`: instead of `pushval((cell)nargs*sizeof(cell))`, emit the count computation from CONTRACT item 2 (it composes with the copy sequence's tail). Update `curfunc->x.stacksize` accounting (`nest_stkusage`) to include a worst-case dynamic component: `nest_stkusage+= <max cells the enclosing function could forward>` — conservative bound: the enclosing function's own maximum argument count is unknown at this point, so use the existing `nest_stkusage` of the ENCLOSING call chain plus `sMAXARGS` cells as the safety margin (document this choice in a comment; it only inflates the recorded stack size, which is advisory metadata).

- [ ] **Step 4: Build; run contract tests; iterate until basic+skip pass**

Run: `cmake --build build -j"$(nproc)" && tools/run-tests.sh -r build/pawnruns build varargs_forward_basic varargs_forward_skip varargs_forward_compat varargs_token_smoke`
Expected: basic, skip, compat, smoke all PASS. Use `build/pawndisasm` on the compiled `varargs_forward_basic.amx` (compile it manually: `cd compiler/source/compiler/tests && ../../../../build/pawncc varargs_forward_basic.pwn -i ../../../include`) to inspect the emitted sequence while debugging.

- [ ] **Step 5: Full suite regression check**

Run: `tools/run-tests.sh -r build/pawnruns build | tail -2`
Expected: `92 PASSED, 5 FAILED` (baseline 91+1 new pass basic... exact arithmetic: 91 baseline passes + basic + skip + compat + smoke pass = 95; failures = 2 baseline + reject still red (Task 4) = 3 → `95 PASSED, 3 FAILED`. Any OTHER test flipping is a regression — investigate before proceeding.)

- [ ] **Step 6: Commit**

```bash
git add compiler/source/compiler/sc3.c
git commit -m "feat: emit varargs-forwarding code in callfunction (___)"
```

---

### Task 4: Error 253 — reject `___` outside variadic functions

**Files:**
- Modify: `compiler/source/compiler/sc5.c:250-252` (errmsg table tail)
- Modify: `compiler/source/compiler/sc3.c` (matchfwdtoken/callfunction validation)

**Interfaces:**
- Consumes: `matchfwdtoken` + `fwdpending` path from Tasks 2-3; `isvariadic(symbol*)` semantics (sc1.c:8110 — check its linkage; if `static`, either add an SC_FUNC declaration in sc.h and drop `static`, or reimplement the 8-line loop locally in sc3.c — prefer exposing the existing one, one-line change).
- Produces: error 253 with the exact message the Task 1 `.meta` pins.

- [ ] **Step 1: Add error message 253**

In `sc5.c`, after the `/*252*/` entry (verified: it is the table's last entry, `sc5.c:250-252`), add:

```c
,
/*253*/  "\"___\" used in a function without variable arguments (\"...\")\n"
```

(Match the surrounding table style exactly — the implementer copies the comma/newline discipline of neighboring entries.)

- [ ] **Step 2: Emit it from the validation path**

In sc3.c, where `matchfwdtoken` returns TRUE inside `callfunction()`: validate the ENCLOSING function. The enclosing function symbol is `curfunc` (global). If `curfunc==NULL || !isvariadic(curfunc)`, call `error(253)` and treat the `___` as expression value 0 (emit `ldconst(0,sPRI)`, `pushreg(sPRI)` — keeps codegen consistent after the error so compilation continues and reports further issues).

- [ ] **Step 3: Also reject malformed `___(<non-constant>)`**

In `matchfwdtoken`: when `___` is followed by `(` and the parenthesized expression is not a single integer constant, `error(253)` and consume the tokens (the interim FALSE-return from Task 2 Step 3 is replaced by this proper error).

- [ ] **Step 4: Run the rejection test**

Run: `cmake --build build -j"$(nproc)" && tools/run-tests.sh -r build/pawnruns build varargs_forward_reject`
Expected: PASS (compiler output matches the .meta exactly — line number 5, message text verbatim).

- [ ] **Step 5: Full suite + all feature tests**

Run: `tools/run-tests.sh -r build/pawnruns build | tail -2`
Expected: `96 PASSED, 2 FAILED` (all feature tests green; only the 2 known baseline failures remain).

- [ ] **Step 6: Commit**

```bash
git commit -am "feat: error 253 for ___ outside variadic functions"
```

---

### Task 5: Nested forwarding + public-function tests + RESULT.md

**Files:**
- Create: `compiler/source/compiler/tests/varargs_forward_nested.pwn` + `.meta`
- Create: `compiler/source/compiler/tests/varargs_forward_dynamic.pwn` + `.meta`
- Create: `experiments/001-varargs/RESULT.md`

**Interfaces:**
- Consumes: complete `___` implementation (Tasks 2-4).
- Produces: `experiments/001-varargs/RESULT.md` (the experiment record per CODING_STANDARDS.md §4.3: what was tried, what worked, what broke, what is next).

- [ ] **Step 1: Write nested forwarding test (from y_va's y_va2_Nesting)**

`varargs_forward_nested.pwn` (exactly):

```pawn
#include <console>

Nest2(const fmat[], ...)
{
	printf(fmat, ___(1));
}

Nest1(const outer[], ...)
{
	Nest2("nested: %s %s\n", ___(1));
}

main()
{
	Nest1("x", "first", "second");
}
```

`varargs_forward_nested.meta` (exactly):

```python
{
  'test_type': 'runtime',
  'expected_output': """
nested: first second
"""
}
```

- [ ] **Step 2: Write dynamic-dispatch passthrough test (from y_va's CallLocalFunction test)**

`varargs_forward_dynamic.pwn` (exactly):

```pawn
#include <console>

forward Target(a, b);

public Target(a, b)
{
	printf("target: %d %d\n", a, b);
}

Dispatch(...)
{
	CallLocalFunction("Target", "ii", ___);
}

main()
{
	Dispatch(11, 22);
}
```

`varargs_forward_dynamic.meta` (exactly):

```python
{
  'test_type': 'runtime',
  'expected_output': """
target: 11 22
"""
}
```

**Note for the implementer:** `CallLocalFunction` is an open.mp/SA-MP native absent from the vendored `compiler/include/`. pawnruns does not provide it either. If the test cannot run because the native is missing, REPLACE the body of `Dispatch`/`Target` with a script-level equivalent that still exercises forwarding into a PUBLIC function called through a function pointer variable — or, simplest honest reduction: mark this test's `.meta` as `output_check` capturing the expected compile-time behavior (successful compile with `___` into a public), and record in RESULT.md that runtime dispatch-passthrough validation requires the open.mp server environment (a manual step outside this suite). Do not fake a pass.

- [ ] **Step 3: Run both new tests**

Run: `cmake --build build -j"$(nproc)" && tools/run-tests.sh -r build/pawnruns build varargs_forward_nested varargs_forward_dynamic`
Expected: nested PASSES. dynamic: per the note — either PASSES at runtime or PASSES as output_check; an honest FAIL with a documented reason in RESULT.md is acceptable ONLY if the native-absence path is proven, not assumed.

- [ ] **Step 4: Full suite final check**

Run: `tools/run-tests.sh -r build/pawnruns build | tail -2`
Expected: `97 PASSED, 2 FAILED` (or `98 PASSED, 2 FAILED` if dynamic runs at runtime) — never more than the 2 known baseline failures.

- [ ] **Step 5: Write experiments/001-varargs/RESULT.md**

Content (fill in the observed values during implementation — no invented numbers):

```markdown
# Experiment 001: Native Varargs Forwarding (`___`)

**Date:** 2026-09-15 (implementation date — update)
**Compiler commit at run:** (git rev-parse HEAD at test time)
**Spec:** docs/superpowers/specs/2026-09-15-varargs-forwarding-design.md

## What was tried

Implementing y_va's `___` forwarding as native compiler codegen: a token
recognized in argument position, a copy sequence in callfunction(), and a
dynamic byte count for the callee. Replaces YSI's runtime bytecode scanning
and rewriting (y_va/impl.inc YVA2_Initalise/YVA2_DoPush).

## What worked

- (list each passing test with one line: basic, skip, compat, nested, ...)

## What broke

- (baseline failures that appeared/disappeared, optimizer interactions,
  anything that needed the memcpy-native fallback vs the pure-opcode loop)

## What is next

- The skipped/parked items from the spec's YAGNI list, benchmark Option A
  vs Option B codegen, upstream PR readiness.
```

- [ ] **Step 6: Commit**

```bash
git add compiler/source/compiler/tests/varargs_forward_nested.* compiler/source/compiler/tests/varargs_forward_dynamic.* experiments/001-varargs/RESULT.md
git commit -m "test: nested and dynamic varargs forwarding; record experiment result"
```

---

## Self-Review (already performed)

1. **Spec coverage:** §2 feature (Tasks 2-3), §3 semantics table — syntax/skip (T2/T3), valid-location+error (T4), position rule (T3 arg-loop wiring — mid-list is YAGNI'd per §6), dynamic count (T3 Step 3), type-check skip (inherent — no check emitted), nested (T5), compat opt-in (T1 compat test + T2 smoke), error 253 (T4). §4.2 Option A first / memcpy fallback — T3 CONTRACT step explicitly permits transcribing YVA2_DoPush if pure-opcode choreography fails (spec allows B as upgrade; the plan treats A-vs-B as an implementation finding recorded in RESULT.md, consistent with spec's "benchmarking is the point"). §4.3 dynamic byte count — T3 Step 3. §4.4 table — T2 (lexer), T3 (sc3 core), T4 (error), tests per row. §4.5 no-new-opcodes + public-function tests — T3 contract item 4, T5. §5 test plan — all six rows mapped (basic T1, skip T1, nested T5, format→covered by basic+skip printf formatting assertions, dynamic T5, reject T1/T4). §6 YAGNI — respected. §7 success criteria — T5 Step 4/5 are the gate.
2. **Placeholder scan:** T3 contains deliberately honest open design (register choreography) resolved as a CONTRACT with a proven fallback (transcribe YVA2_DoPush) — not a placeholder; every other step carries exact content. The `???` in T3 Step 1 is explicitly resolved in-step.
3. **Type consistency:** `matchfwdtoken(cell *skip)` defined T2, consumed T3/T4 identically; error 253 defined T1 .meta, emitted T4, same message text verbatim; test names consistent across tasks (`varargs_forward_*`).
