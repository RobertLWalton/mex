// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Jul  1 03:51:20 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Program Instructions


// Setup
// -----

# ifndef MEX_H
# define MEX_H

// Inclusions.
//
# include <min.h>


// Program Instructions
// ------- ------------

namespace mex {

struct instr {
    min::uns8 op_code, trace_flags;
    min::uns16 immedA, immedB, immedC;
    min::gen immedD;
};

enum op_code {
    ADD, MUL,
    ADDI, MULI,
    SUB, SUBR,
    SUBI, SUBRI,
    DIV, DIVR,
    DIVI, DIVRI,
    MOD, MODR,
    MODI, MODRI,
    FLOOR, CEILING,
    TRUNC, ROUND,
    NEG, ABS,
    LOG10, LOGE,
    LOG, LOGR,
    EXP10, EXPE,
    EXP, EXPR,
    SIN, COS, TAN,
    ASIN, ACOS, ATAN,
    ATAN2, ATAN2R,
    POWER,
    PUSH, PUSHI, PUSHG, PUSHM,
    POP, POPM,
    JMP,
    JMPEQ, JMPNE,
    JMPLT, JMPLEQ,
    JMPGT, JMPGEQ,
    BEG, NOP,
    END,
    BEGL,
    ENDL, CONT,
    SET_TRACE,
    ERROR,
    BEGF, ENDF,
    CALL, CALLG,
    RET,
    PUSHL,
    POPL,
    PUSHA,
    PUSHNARGS,
    PUSHV
};

enum trace_flag
{
    TRACE_DEPTH = 7 << 0,
    TRACE_PHRASE = 1 << 3,
    TRACE_NOJUMP = 1 << 4,
    TRACE        = 1 << 7
};

struct module_header
{
    min::uns32 control;
    min::uns32 length;
    min::uns32 max_length;
    const min::phrase_position_vec position;
    const min::packed_vec_ptr<min::gen> globals;
    const min::gen interface;
    const min::packed_vec_ptr<min::gen> trace_info;
};

typedef min::packed_vec_insptr
               <mex::instr, mex::module_header>
	       module;

MIN_REF ( min::phrase_position_vec, position, mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>, globals,
                                         mex::module )
MIN_REF ( min::gen, interface, mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>, trace_info,
                                         mex::module )

struct process_header;
typedef min::packed_vec_insptr
               <min::gen, mex::process_header>
	       process;

struct pc
{
    mex::module module;
    min::uns32 index;
};

struct ret
{
    mex::pc saved_pc;
    min::uns32 saved_fp;
    min::uns32 nargs;
};

typedef void (* trace_function ) ( mex::process p );

struct process_header
{
    min::uns32 control;
    min::uns32 length;
    min::uns32 max_length;
    const mex::pc pc;
    min::uns32 sp;
    const min::packed_vec_insptr<mex::ret> return_stack;
    min::uns32 rp;
    min::uns32 fp[16];
    mex::trace_function trace_function;
    min::uns32 trace_depth;
    min::uns8 trace_flags;
    int excepts;
    int excepts_accumulator;
    bool optimize;
};

MIN_REF ( min::packed_vec_insptr<mex::ret>,
          return_stack, mex::process )
inline void set_pc ( mex::process p, mex::pc pc )
{
    * (mex::pc *) & p->pc = pc;
    if ( pc.module != min::NULL_STUB )
        min::unprotected::acc_write_update
	    ( p, pc.module );
}

} // end mex namespace

# endif // MEX_H
