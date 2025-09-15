*** a/cmp.c
--- b/cmp.c
@@
 #include "rob.h"
 #include "iq.h"
 #include "resource.h"
#include <vector>            // needed for std::vector in extern decls
 
// We only DECLARE these here to avoid multiple definition.
// They are DEFINED in sim-outorder.c.
class context;                             // forward decl (context is a C++ class in smt.h)
extern std::vector<context> contexts;      // defined in sim-outorder.c
extern int num_contexts;                   // defined in sim-outorder.c
extern int reg_counter[];                  // defined in sim-outorder.c (your per-thread counter)

@@
-void core_t::rollbackTo(context & target, counter_t & sim_num_insn, ROB_entry * targetinst, bool branch_misprediction)
void core_t::rollbackTo(context & target, counter_t & sim_num_insn, ROB_entry * targetinst, bool branch_misprediction)
 {
     ...
@@
-        if(target.ROB[ROB_index].physreg >= 0){
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_FREE);
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_ARCH);
-            reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state=REG_FREE;
        if (target.ROB[ROB_index].physreg >= 0) {
            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg, target.ROB[ROB_index].dest_format).state != REG_FREE);
            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg, target.ROB[ROB_index].dest_format).state != REG_ARCH);
            reg_file.reg_file_access(target.ROB[ROB_index].physreg, target.ROB[ROB_index].dest_format).state = REG_FREE;

            /* -- Register accounting for rollback (cmp.c) --
             * Find this thread's index by pointer-identity against the global contexts vector.
             * We cannot use target.id or target.context_id (not reliable/available).
             * Decrement once per physical register we actually freed here.
             */
            int tid = -1;
            for (int i = 0; i < num_contexts; ++i) {
                if (&contexts[i] == &target) { tid = i; break; }
            }
            if (tid >= 0 && tid < num_contexts && reg_counter[tid] > 0) {
                reg_counter[tid]--;
            }
 
             target.rename_table[target.ROB[ROB_index].archreg] = target.ROB[ROB_index].old_physreg;
             target.ROB[ROB_index].physreg = -1;
         }
@@
-        if(target.ROB[ROB_index].physreg >= 0){
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_FREE);
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_ARCH);
-            reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state=REG_FREE;
        if (target.ROB[ROB_index].physreg >= 0) {
            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg, target.ROB[ROB_index].dest_format).state != REG_FREE);
            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg, target.ROB[ROB_index].dest_format).state != REG_ARCH);
            reg_file.reg_file_access(target.ROB[ROB_index].physreg, target.ROB[ROB_index].dest_format).state = REG_FREE;

            /* -- Register accounting for rollback (cmp.c) --
             * Same as above block: decrement per-thread usage when a physreg is freed.
             */
            int tid = -1;
            for (int i = 0; i < num_contexts; ++i) {
                if (&contexts[i] == &target) { tid = i; break; }
            }
            if (tid >= 0 && tid < num_contexts && reg_counter[tid] > 0) {
                reg_counter[tid]--;
            }
 
             target.rename_table[target.ROB[ROB_index].archreg] = target.ROB[ROB_index].old_physreg;
             target.ROB[ROB_index].physreg = -1;
         }
@@
     ...
 }

This linker error is happening because contexts, cores, num_contexts, ejected_contexts, and similar global variables are being defined in both sim-outorder.c and cmp.c.

In C/C++:

Definition (int num_contexts;) actually allocates storage.

Declaration (extern int num_contexts;) only tells the compiler that the variable exists somewhere else.

You’ve got both sim-outorder.c and cmp.c defining the same variables instead of having one definition and extern declarations in the other file.

How to fix

Since you told me earlier you want only sim-outorder.c and cmp.c involved (no header rewriting), the minimal fix is:

Keep the definitions of these variables in sim-outorder.c.
For example:

// sim-outorder.c
core_t cores[MAX_CORES];
context_t contexts[MAX_CONTEXTS];
int num_contexts;
int num_cores;
int max_contexts_per_core;
int contexts_at_init_time;
int cores_at_init_time;
context_t ejected_contexts[MAX_CONTEXTS];


In cmp.c, change those lines so they are extern instead of definitions.
For example:

// cmp.c
extern core_t cores[];
extern context_t contexts[];
extern int num_contexts;
extern int num_cores;
extern int max_contexts_per_core;
extern int contexts_at_init_time;
extern int cores_at_init_time;
extern context_t ejected_contexts[];


This way:

sim-outorder.c allocates memory for the variables.

cmp.c can access them without causing multiple definition errors.

If you want, I can give you the exact patch for cmp.c so you can keep all your register rename/commit modifications without touching any other files.
Do you want me to prepare that patch?

I’m using the same safe pattern you liked (find the thread index by pointer comparison: &contexts[i] == &target) so we don’t rely on non-existent target.id/target.context_id.

Patch: update cmp.c for rollback-stage register accounting
*** a/cmp.c
--- b/cmp.c
@@
-#include "cmp.h"
+#include "cmp.h"
+#include <vector>              /* for std::vector in extern decl */
+/* Per-thread register-usage counters live in sim-outorder.c.
+ * We only *reference* them here. Do NOT define/initialize them here. */
+extern int reg_counter[];      /* size >= num_contexts, defined in sim-outorder.c */
+extern unsigned int num_contexts;
+extern std::vector<context> contexts;  /* defined in sim-outorder.c */
@@
 /* existing code ... */
@@
-        if(target.ROB[ROB_index].physreg >= 0){
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_FREE);
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_ARCH);
-            reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state=REG_FREE;
+        if (target.ROB[ROB_index].physreg >= 0) {
+            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,
+                                            target.ROB[ROB_index].dest_format).state != REG_FREE);
+            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,
+                                            target.ROB[ROB_index].dest_format).state != REG_ARCH);
+            /* Free the physreg in the RF */
+            reg_file.reg_file_access(target.ROB[ROB_index].physreg,
+                                     target.ROB[ROB_index].dest_format).state = REG_FREE;
+
+            /* --- Per-thread register counter adjustment (rollback path) --- */
+            int tid = -1;
+            for (unsigned i = 0; i < num_contexts; ++i) {
+                if (&contexts[i] == &target) { tid = (int)i; break; }
+            }
+            if (tid >= 0 && tid < (int)num_contexts && reg_counter[tid] > 0) {
+                reg_counter[tid]--;
+            }
+            /* -------------------------------------------------------------- */
 
             target.rename_table[target.ROB[ROB_index].archreg] = target.ROB[ROB_index].old_physreg;
             target.ROB[ROB_index].physreg = -1;
         }
@@
 /* ... more code ... */
@@
-        if(target.ROB[ROB_index].physreg >= 0){
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_FREE);
-            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_ARCH);
-            reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state=REG_FREE;
+        if (target.ROB[ROB_index].physreg >= 0) {
+            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,
+                                            target.ROB[ROB_index].dest_format).state != REG_FREE);
+            assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,
+                                            target.ROB[ROB_index].dest_format).state != REG_ARCH);
+            /* Free the physreg in the RF */
+            reg_file.reg_file_access(target.ROB[ROB_index].physreg,
+                                     target.ROB[ROB_index].dest_format).state = REG_FREE;
+
+            /* --- Per-thread register counter adjustment (rollback path) --- */
+            int tid = -1;
+            for (unsigned i = 0; i < num_contexts; ++i) {
+                if (&contexts[i] == &target) { tid = (int)i; break; }
+            }
+            if (tid >= 0 && tid < (int)num_contexts && reg_counter[tid] > 0) {
+                reg_counter[tid]--;
+            }
+            /* -------------------------------------------------------------- */
 
             target.rename_table[target.ROB[ROB_index].archreg] = target.ROB[ROB_index].old_physreg;
             target.ROB[ROB_index].physreg = -1;
         }

Where these hunks apply

The first hunk is at the top of cmp.c: it adds only #include <vector> and the three extern declarations so we can safely touch the per-thread counters and see the contexts vector without defining anything.

The second hunk targets the rollback function that shows up in your grep as:

cmp.c:386..393


(right after the assert calls and the line that frees the physreg). We insert the tid lookup via &contexts[i] == &target and decrement reg_counter[tid] if positive.

The third hunk is the same adjustment for the other rollback/squash path in the same file that also frees a physreg (your grep showed:

cmp.c:559..566


with the same pattern). We add the identical tid compute + decrement block there as well.

These changes keep everything local to cmp.c, use no non-existent fields like target.id, and do not redefine any globals (only extern references). They integrate cleanly with the register_rename() increment and commit() decrement you already added in sim-outorder.c.