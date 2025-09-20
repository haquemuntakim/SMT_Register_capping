# Session Changelog — SMT_Register_capping

This file records the code edits and build actions performed in the current session (for traceability).

Date: 2025-09-17

## Files modified

1. `sim-outorder.c`
   - Added a per-context rename-stage stall check in `register_rename()`.
     - Code added (present near the call to `get_reg_set` / before `find_free_physreg`):
       ```c
       int effective_rename_limit = (rename_reg_limit >= 0) ? rename_reg_limit : cores[core_num].rename_limit;
       if (disp_context_id >= 0 && disp_context_id < num_contexts && reg_counter[disp_context_id] >= effective_rename_limit) {
           // stall rename for this thread until some registers are freed (freed at commit)
           contexts_left.erase(contexts_left.begin()+current_context);
           continue;
       }
       ```
   - Added runtime-global variable `rename_reg_limit` (already present in the file): `int rename_reg_limit = 8;` (default can be changed via runtime options)

2. `cmp.h`
   - Added a new per-core field in `class core_t`:
     ```c
     int rename_limit;
     ```
     Purpose: to allow each core to have a per-context rename limit (used when `rename_reg_limit < 0`).

3. `cmp.c`
   - Initialized `rename_limit` in the `core_t` constructor.
     - Initialization uses `DEFAULT_RENAME_LIMIT` if provided via the Makefile macro, otherwise falls back to `4`:
       ```c
       #ifdef DEFAULT_RENAME_LIMIT
           rename_limit(DEFAULT_RENAME_LIMIT),
       #else
           rename_limit(4),
       #endif
       ```


## Makefile / Build
- `Makefile` already contains a variable `RENAME_LIMIT ?= 4` and wires `-DDEFAULT_RENAME_LIMIT=$(RENAME_LIMIT)` into `OFLAGS`.
- Build steps executed (output captured):
  - `make clean && make` initially failed due to missing `core_t::rename_limit`.
  - After adding the field and constructor init, `make -j2` completed successfully (linker produced `sim-outorder` binary).

## Rationale / Notes
- The per-context rename stall check prevents rename from allocating additional physical registers for a context whose outstanding allocated registers (tracked by `reg_counter[context_id]`) is >= configured limit.
- `rename_reg_limit` is a global override (if >= 0). If it is -1, per-core `cores[core_num].rename_limit` is used.
- The conservative strategy chosen here is to stall rename (via `contexts_left.erase(...)`) rather than forcibly freeing registers (which would require complex rollback/squash behavior).

## How to set the limit to 8
- Runtime (recommended): pass `-rename:global_limit 8` to the `sim-outorder` command.
- Build-time: `make RENAME_LIMIT=8` will set `DEFAULT_RENAME_LIMIT` used to initialize `cores[].rename_limit`.

## Next steps / suggestions
- If you want to proactively reclaim registers (instead of stalling), implement `context_erase` with clear semantics (conservative: only free `REG_FREE`/`REG_ARCH` entries; aggressive: `flushcontext`/`rollbackTo` with squashes) and wire it before the stall.
- Add tests or logging hooks to verify `reg_counter[...]` behavior across commit/rollback.

---

If you want, I can also create a short `git`-style patch summary or add per-function comments in the modified files. Let me know which format you prefer.
