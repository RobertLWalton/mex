// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Mon Jul 10 03:31:43 EDT 2023
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

// The following must correlate with the opcode_info
// table in mex.cc.
//
enum op_code {
    _NONE_,
    ADD, ADDI,
    MUL, MULI,
    SUB, SUBR, SUBI, SUBRI,
    DIV, DIVR, DIVI, DIVRI,
    MOD, MODR, MODI, MODRI,
    FLOOR, CEIL, TRUNC, ROUND,
    NEG, ABS,
    LOG, LOG10, EXP, EXP10,
    SIN, ASIN,
    COS, ACOS,
    TAN, ATAN,
    ATAN2, ATAN2R,
    POWI,
    PUSHS, PUSHL, PUSHI, PUSHG,
    POPS,
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
    CALLM, CALLG,
    RET,
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

MIN_REF ( min::phrase_position_vec, position,
                                    mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>, globals,
                                         mex::module )
MIN_REF ( min::gen, interface, mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>, trace_info,
                                         mex::module )

enum finish_state
{
    MODULE_END	= 1,
    CALL_END	= 2,
    LIMIT_STOP	= 3,
    ERROR_STOP	= 4,
    JMP_ERROR	= 5,
    FORM_ERROR	= 6
};

const unsigned max_lexical_level = 16;

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
    const mex::pc saved_pc;
    min::uns32 saved_fp;
    min::uns32 nargs;
    min::uns32 nresults;
};

typedef void (* trace_function )
    ( mex::process p, min::gen info );

struct process_header
{
    min::uns32 control;
    min::uns32 length;
    min::uns32 max_length;
    min::printer printer;
    const mex::pc pc;
    min::packed_vec_insptr<mex::ret> return_stack;
    min::uns32 fp[mex::max_lexical_level + 1];
    mex::trace_function trace_function;
    min::uns32 trace_depth;
    min::uns8 trace_flags;
    int excepts;
    int excepts_accumulator;
    bool optimize;
    mex::finish_state finish_state;
    min::uns32 counter;
    min::uns32 limit;
};

MIN_REF ( min::printer, printer, mex::process )
MIN_REF ( min::packed_vec_insptr<mex::ret>,
          return_stack, mex::process )
inline min::gen * process_push
    ( mex::process p, min::gen * sp, min::gen v )
{
    * sp ++ = v;
    min::unprotected::acc_write_update ( p, v );
    return sp;
}
inline void set_pc ( mex::process p, mex::pc pc )
{
    * (mex::pc *) & p->pc = pc;
    if ( pc.module != min::NULL_STUB )
        min::unprotected::acc_write_update
	    ( p, pc.module );
}
inline void set_saved_pc
    ( mex::process p, mex::ret * ret, mex::pc pc )
{
    * (mex::pc *) & ret->saved_pc = pc;
    min::unprotected::acc_write_update
	( p->return_stack, pc.module );
}

extern min::uns32 trace_indent;
extern char trace_mark;

bool run_process ( mex::process p );

extern min::locatable_var<min::printer> default_printer;

} // end mex namespace

# endif // MEX_H
