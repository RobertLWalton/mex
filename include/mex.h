// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jul 23 12:01:34 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Program Instructions
//	Modules
//	Processes
//	Functions


// Setup
// -----

# ifndef MEX_H
# define MEX_H

// Inclusions.
//
# include <min.h>

namespace mex {

extern min::uns32 trace_indent;
extern char trace_mark;
extern min::locatable_var<min::printer> default_printer;
extern min::uns32 module_length;
extern min::uns32 stack_limit;
extern min::uns32 return_stack_limit;

}


// Program Instructions
// ------- ------------

namespace mex {

struct instr {
    min::uns8 op_code, trace_flags, unused1, unused2;
    min::uns32 immedA, immedB, immedC;
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
    PUSHV,
    NUMBER_OF_OP_CODES
};

enum
{
    NONA = 1,	// No arithmetic operands, not a JMP.
    A2 =   2,	// sp[0] and sp[-1] are the arithmetic
                // operands in that order.
    A2R =  3,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order.
    A2I =  4,	// sp[0] and immedD are the arithmetic
                // operands in that order.
    A2RI = 5,	// immedD and sp[0] are the arithmetic
                // operands in that order.
    A1 =   6,	// sp[0] is an arithmetic operand.
    J2 =   7,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order, and the
		// operation is a jump.
    J =    8,	// JMP, no arithmetic operands.
};

struct op_info
{
    min::uns8 op_code;
        // Op code as a check: e.g., mex::POP.
	// It should be true that:
	//     op_infos[mex::POP].op_code = mex::POP
    min::uns8 op_type;
        // See above.
    const char * name;
        // Name of op_code: e.g, "POP".
    const char * oper;
        // Name of op_code operator: e.g, "+" or "<=".
};

extern op_info op_infos[];

enum trace_flag
{
    TRACE_DEPTH  = 7 << 0,
    TRACE_LINES  = 1 << 3,
    TRACE_NOJUMP = 1 << 4,
    TRACE        = 1 << 7
};


// Modules
// -------

struct module_header
{
    const min::uns32 control;
    const min::uns32 length;
    const min::uns32 max_length;
    const min::phrase_position_vec position;
    const min::packed_vec_ptr<min::gen> globals;
    const min::gen interface;
    const min::packed_vec_ptr<min::gen> trace_info;
};

typedef min::packed_vec_ptr
	    <mex::instr, mex::module_header>
	    module;
typedef min::packed_vec_insptr
	    <mex::instr, mex::module_header>
	    module_ins;

MIN_REF ( min::phrase_position_vec,
          position, mex::module_ins )
MIN_REF ( min::packed_vec_ptr<min::gen>,
          globals, mex::module_ins )
MIN_REF ( min::gen, interface, mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>,
          trace_info, mex::module_ins )

inline void push_instr
        ( mex::module_ins m,
	  const mex::instr & instr )
{
    min::push(m) = instr;
    min::unprotected::acc_write_update
	( m, instr.immedD );
}


// Processes
// ---------

enum state
{
    NEVER_STARTED,
    RUNNING,
    MODULE_END,
    CALL_END,
    LIMIT_STOP,
    ERROR_STOP,
    JMP_ERROR,
    FORM_ERROR
};

const unsigned max_lexical_level = 16;

struct process_header;
typedef min::packed_vec_insptr
               <min::gen, mex::process_header>
	       process;

struct pc
{
    const mex::module module;
    min::uns32 index;
    mex::pc & operator = ( const mex::pc pc )
    {
        // Implicit = is not defined because 
        // module is const.
        //
        * (mex::module *) & this->module = pc.module;
        this->index = pc.index;
	return * this;
    }
};
template<typename S>
min::uns32 DISP ( const mex::pc S::* d )
{
    return   min::OFFSETOF ( d )
           + min::DISP ( & mex::pc::module );
}

struct ret
{
    const mex::pc saved_pc;
    min::uns32 saved_fp;
    min::uns32 nargs;
    min::uns32 nresults;
};

typedef min::packed_vec_insptr<mex::ret> return_stack;

typedef void (* trace_function )
    ( mex::process p, min::gen info );
void default_trace_function
    ( mex::process p, min::gen info );

struct process_header
{
    const min::uns32 control;
    const min::uns32 length;
    const min::uns32 max_length;
    min::printer printer;
    const mex::pc pc;
    mex::return_stack return_stack;
    min::uns32 fp[mex::max_lexical_level + 1];
    mex::trace_function trace_function;
    min::uns32 trace_depth;
    min::uns8 trace_flags;
    int excepts;
    int excepts_accumulator;
    bool optimize;
    mex::state state;
    min::uns32 counter;
    min::uns32 limit;
};

MIN_REF ( min::printer, printer, mex::process )
MIN_REF ( mex::return_stack, return_stack,
          mex::process )
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

// Functions
// ---------

bool run_process ( mex::process p );

mex::module create_module ( min::file f );

mex::process create_process
    ( min::printer printer = mex::default_printer );

mex::process init_process
    ( mex::module m, mex::process p = min::NULL_STUB );

mex::process init_process
    ( mex::pc pc, mex::process p = min::NULL_STUB );

} // end mex namespace

# endif // MEX_H
