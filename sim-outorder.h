#ifndef SIM_OUTORDER_H
#define SIM_OUTORDER_H

#include<cstdio>
#include<cstdlib>
#include<cmath>
#include<cassert>
#include<csignal>
#include<vector>
#include<string>
#include<iostream>
#include<list>
#include<set>
#include<sstream>
#include<algorithm>

#include"host.h"
#include"misc.h"
#include"machine.h"
#include"regs.h"
#include"memory.h"
#include"cache.h"
#include"loader.h"
#include"syscall.h"
#include"bpreds.h"
#include"resource.h"
#include"options.h"
#include"eval.h"
#include"stats.h"
#include"ptrace.h"
#include"dlite.h"
#include"sim.h"
#include"smt.h"
#include"cmp.h"
#include"endian.h"
#include"rob.h"
#include"iq.h"
#include"regrename.h"
#include"fetchtorename.h"
#include"inflightq.h"
#include"dram.h"
#include"eio.h"

//added for Wattch
#include "power.h"

#define COMMIT_TIMEOUT 100000
#ifndef MAX_CONTEXTS
#define MAX_CONTEXTS 4
#endif

extern int reg_counter[MAX_CONTEXTS];
//haque edit: prototype for external use - decrement the per-context register allocation counter if positive
void reg_counter_decref_if_positive(int tid);

/**************** SMT Options *******************/
//the number of contexts present in the simulator
extern int num_contexts;
//Number of contexts detected from the command line
extern int contexts_at_init_time;

//the actual contexts - (see smt.h for details)
extern std::vector<context> contexts;

//ejected contexts - retained such that we can see statistics later
extern std::vector<context> ejected_contexts;
/***********************************************/

/**************** CMP Options ******************/
//the number of cores present in the simulator
extern unsigned int num_cores;
//Number of cores detected from the command line
extern unsigned int cores_at_init_time;
//Max number of contexts allowed on a core (needed to reserve architectural registers
extern int max_contexts_per_core;

//The actual cores - (see cmp.h for details)
//✅ Must be extern, not a definition here
extern std::vector<core_t> cores;
/********************************************/

//Fetchers, ICOUNT, Round Robin and DCRA
std::vector<int> icount_fetch(unsigned int core_num);
std::vector<int> RR_fetch(unsigned int core_num);
std::vector<int> dcra_fetch(unsigned int core_num);

/* non-zero if all register operands are ready */
int operand_ready(ROB_entry *rs, int op_num);
int operand_spec_ready(ROB_entry *rs, int op_num);
int all_operands_ready(ROB_entry *rs);
int all_operands_spec_ready(ROB_entry *rs);
int one_operand_ready(ROB_entry *rs);

/*
 * input dependencies for stores in the LSQ:
 *   idep #0 - operand input (value that is store'd)
 *   idep #1 - effective address input (address of store operation)
 */
#define STORE_OP_INDEX              0
#define STORE_ADDR_INDEX            1

#define STORE_OP_READY(RS)          (operand_spec_ready(RS, STORE_OP_INDEX))
#define STORE_ADDR_READY(RS)        (operand_spec_ready(RS, STORE_ADDR_INDEX))

//default register state accessor, used by DLite
//err str, NULL for no err
const char * simoo_reg_obj(regs_t *regs,        //registers to access
    int is_write,              //access type
    enum md_reg_type rt,        //reg bank to probe
    int reg,                //register number
    eval_value_t *val);         //input, output

//default memory state accessor, used by DLite
//err str, NULL for no err
const char * simoo_mem_obj(mem_t *mem,          //memory space to access
    int is_write,              //access type
    md_addr_t addr,            //address to access
    char *p,                //input/output buffer
    int nbytes);                //size of access

//default machine state accessor, used by DLite
//err str, NULL for no err
const char * simoo_mstate_obj(FILE *stream,     //output stream
    char *cmd,               //optional command string
    regs_t *regs,              //registers to access
    mem_t *mem);              //memory space to access

enum ff_mode_t
{
    NORMAL = 0,
    NO_WARMUP = 1
};

//Fast forward handler
int ff_context(unsigned int current_context, long long insts_count, ff_mode_t mode);

#endif
