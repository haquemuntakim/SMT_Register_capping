BUILD REPORT: Fixing undefined reference to reg_counter_decref_if_positive

Date: 2025-09-14
Project: m-sim (sim-outorder)
Author: automated patch (with //haque edit markers)

Summary
-------
An undefined symbol error occurred when linking `sim-outorder`: the function reg_counter_decref_if_positive(int) was referenced by `cmp.c` but not visible at link time. I made edits to expose the function and declared it in the header. After the fixes the build succeeded and the produced binary runs.

Files changed
------------
- sim-outorder.c  //haque edit
  - Changed the helper function definition from `static inline` (internal linkage) to an external definition `void reg_counter_decref_if_positive(int tid)` so the symbol is emitted into the object file.

- sim-outorder.h  //haque edit
  - Added the prototype: `void reg_counter_decref_if_positive(int tid);` so other compilation units (e.g., cmp.c) see the declaration.

Why the error happened (technical explanation)
--------------------------------------------
1) What happened
- `cmp.c` calls reg_counter_decref_if_positive(...).
- The function existed in `sim-outorder.c`, but it was defined as a static/inline helper. That meant the compiler did not emit an externally-linkable symbol for it.
- At link time, `cmp.o` referenced the symbol, but the linker couldn't find it in any object archive, causing:
  /usr/bin/ld: cmp.o: undefined reference to `reg_counter_decref_if_positive(int)'

2) Symbol visibility primer (ASCII diagram)

  +----------------+        compile/link        +----------------+
  |    cmp.c       |  ----------------------->  |   cmp.o        |
  |  (calls f())   |    references f (extern)   |  (ref: f)      |
  +----------------+                            +----------------+
        ^                                              |
        |                                              |
        |                                              v
  +----------------+  compile  -----------------> +----------------+
  | sim-outorder.c |  defines f as static/inline | sim-outorder.o  |
  | static inline f|  (no external symbol)       | (no symbol f)   |
  +----------------+                            +----------------+
                                                    |
                                                    v
                                              link: undefined ref

3) Why the `static inline` definition didn't work
- `static` gives internal linkage: symbol is not visible outside the translation unit.
- `inline` alone does not guarantee emission of an external symbol (and compilers may not emit one), so other TUs can't link to it.
- Because only one TU had the definition, the linker had no external symbol to resolve `cmp.o`'s reference.

Attempt log (chronological)
---------------------------
Attempt 1: Run `make` (initial)
- Command: cd /home/maniacub/m-sim && make -j2
- Result: Linker error
  - cmp.c references reg_counter_decref_if_positive but no symbol emitted.
- Why it failed: function had internal linkage (static/inline) in `sim-outorder.c`.

Attempt 2: Read code and locate helper (investigation)
- Found reg_counter and reg_counter_decref_if_positive in `sim-outorder.c`.
- Found calls in `cmp.c`.
- Considered options: (A) add prototype in header + leave inline (risky), (B) make function externally visible in sim-outorder.c and declare prototype in header, (C) move function to header as inline.

Attempt 3: Change to `inline` (first edit)
- Edit: replaced `static inline` with `inline` and added prototype in header.
- Rationale: make it visible; sometimes `inline` without static causes a weak external symbol, but this is implementation-defined.
- Result: Rebuild still failed with undefined reference at link time.
- Why it failed: `inline` alone may not cause emission of an external symbol; the compiler likely treated it as inline-only and didn't emit a symbol for the TU.

Attempt 4: Change to normal external function (final edit)  //haque edit
- Edit: changed `inline` to normal `void reg_counter_decref_if_positive(int tid)` definition in `sim-outorder.c` and kept prototype in `sim-outorder.h`.
- Rationale: ensure the compiler emits an externally visible symbol; simplest and robust solution.
- Result: Rebuilt project; link succeeded and binary produced.
- Why it worked: non-inline external definition creates a symbol in `sim-outorder.o` the linker can use to resolve references from `cmp.o`.

What I changed exactly (diff-summary)
------------------------------------
- sim-outorder.c:
  - Before:
    static inline void reg_counter_decref_if_positive(int tid) { if (...) reg_counter[tid]--; }
  - After:  //haque edit
    void reg_counter_decref_if_positive(int tid) { if (...) reg_counter[tid]--; }

- sim-outorder.h:
  - Added: //haque edit
    void reg_counter_decref_if_positive(int tid);

Why choosing the non-inline solution is safe
------------------------------------------
- The function is tiny and non-performance critical; making it externally visible doesn't introduce measurable overhead.
- It keeps implementation in a single .c file (no code duplication), avoids ODR or inlining inconsistencies, and is the least intrusive fix.

Alternative approaches
----------------------
- Move the function definition into `sim-outorder.h` as `static inline` and include it from both TUs. That duplicates the implementation but allows inlining per TU. Drawback: more code duplication in object files and risk of divergent definitions.

- Use `extern inline` or `__attribute__((always_inline))` patterns; subtle, compiler- and standard-version dependent. More fragile across compilers.

Recommended follow-ups
----------------------
- Grep the code for other small static helper functions called from other TUs and ensure they are declared/defined correctly (avoid other hidden-linkage bugs).
- Address compiler warnings seen during builds (deprecated std::binary_function, printf format issues). These don't break the build now but are future maintenance issues.
- If you want `reg_counter_decref_if_positive` inlined for perf, implement it as a header `static inline` function in `sim-outorder.h` and keep the external fallback in `sim-outorder.c` guarded by a macro. I can implement that if desired.

Appendix: ASCII diagrams explaining the fix
-----------------------------------------
1) Problem: internal linkage prevents external references

  cmp.o
   |  call f()
   v
  [linker expects symbol f]

  sim-outorder.o
   |  static inline f()  // internal only
   v
  [no symbol f emitted]

  -> Linker: undefined reference to f

2) Fix: emit external symbol

  sim-outorder.o
   |  void f() { ... }  // external symbol
   v
  [symbol f emitted]

  cmp.o -> link -> resolved by sim-outorder.o

Build & test commands used
--------------------------
- Initial build: cd /home/maniacub/m-sim && make -j2
- Rebuild after edits: cd /home/maniacub/m-sim && make -j2
- Quick runtime (smoke): cd /home/maniacub/m-sim && ./sim-outorder -help
- Example sim run (from your terminal history): sudo ../m-sim/sim-outorder -rf:size 160 -fastfwd 10000 -max:inst 100000 astarNS.1.arg bzip2NS.1.arg dealIINS.1.arg gccNS.1.arg

Log of relevant terminal outputs (selected)
-------------------------------------------
- Linker error before fix:
  /usr/bin/ld: cmp.o: in function `core_t::rollbackTo(context&, long long&, ROB_entry*, bool)':
  /home/maniacub/m-sim/cmp.c:583:(.text+0x3c13): undefined reference to `reg_counter_decref_if_positive(int)'

- After final fix:
  Linking succeeded; binary `sim-outorder` produced.

Change attribution marker
-------------------------
All edits include the marker comment `//haque edit` as requested, placed next to the modified/added lines.

If you want more
----------------
- I can: generate a small patch file containing just the final changes (already applied), or revert/adjust to a header-inline approach.
- I can also clean up the top compiler warnings in a separate patch.

Status of todo list
-------------------
- All build-fix related tasks are complete. The report generation is complete and saved to: `/home/maniacub/m-sim/BUILD_REPORT.md`. I'll mark the report task completed now.

Completed by automated edits. If you'd like me to push further cleanups or a different approach (inline in header, or more extensive docs), tell me which option to take next.