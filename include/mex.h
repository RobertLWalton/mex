// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Aug 25 09:29:25 PM EDT 2024
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


// Program Instructions
// ------- ------------

struct instr {
    min::uns8 op_code, trace_class, trace_depth,
              unused;
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
    JMPF, JMPT,
    BEG, NOP,
    END,
    BEGL,
    ENDL, CONT,
    BEGF, ENDF,
    CALLM, CALLG,
    RET,
    PUSHA,
    PUSHNARGS,
    PUSHV,
    SET_TRACE,
    TRACE, WARN, ERROR,
    SET_EXCEPTS,
    TRACE_EXCEPTS,
    SET_OPTIMIZE,
    NUMBER_OF_OP_CODES
};

enum trace_class
{
    T_NEVER,
    T_ALWAYS,
    T_AOP,
    T_PUSH,
    T_POP,
    T_JMP,
    T_JMPS,
    T_JMPF,
	// T_JMPS for successful JMPs, T_JMPF for failed
	// JMPs.  Trace class of conditional jumps,
	// JMP..., is T_JMPS.  +1 is added at run-time
	// if the jump fails.
    T_BEG,
    T_END,
    T_BEGL,
    T_ENDL,
    T_CONT,
    T_BEGF,
    T_ENDF,
    T_CALLM,
    T_CALLG,
    T_RET,
    T_NOP,
    T_SET_EXCEPTS,
    T_SET_OPTIMIZE,
    NUMBER_OF_TRACE_CLASSES
};

enum op_type
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
    J1 =   8,	// sp[0] is an operand, and the
    		// operation is a jump.
    J =    9,	// JMP, no arithmetic operands.
};

struct op_info
{
    min::uns8 op_code;
        // Op code as a check: e.g., mex::POP.
	// It should be true that:
	//     op_infos[mex::POP].op_code = mex::POP
    min::uns8 op_type;
        // See above.
    min::uns8 trace_class;
        // Trace class of op_code.
    const char * name;
        // Name of op_code: e.g, "POP".
    const char * oper;
        // Name of op_code operator: e.g, "+" or "<=".
};
extern op_info op_infos[];

struct trace_class_info
{
    min::uns8 trace_class;
        // Trace class as a check: e.g., mex::T_POP.
	// It should be true that:
	//     trace_class_infos[mex::POP].trace_class
	//         = mex::T_POP
    const char * name;
        // Name of trace_class: e.g, "T_POP".
};
extern trace_class_info trace_class_infos[];

struct except_info
{
    int mask;
    const char * name;
};
const int NUMBER_OF_EXCEPTS = 5;
extern except_info except_infos[NUMBER_OF_EXCEPTS];

void print_excepts
    ( min::printer printer, int excepts,
                            int highlight = 0 );
    // Print a single space separated list of the names
    // of the except bits on in excepts.  Procede a
    // name with * if its bit is also on in highlight.



// Modules
// -------

extern min::uns32 module_length;

struct module_header
{
    const min::uns32 control;
    const min::uns32 length;
    const min::uns32 max_length;
    const min::gen name;  // From input file->file_name.
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

MIN_REF ( min::gen, name, mex::module )
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

inline void push_position
        ( mex::module m,
	  const min::phrase_position & pp )
{
    min::phrase_position_vec_insptr ppins =
        (min::phrase_position_vec_insptr) m->position;
    min::push(ppins) = pp;
}

inline void push_trace_info
        ( mex::module m, min::gen trace_info )
{
    min::packed_vec_insptr<min::gen> trace_info_ins =
        (min::packed_vec_insptr<min::gen>)
	m->trace_info;
    min::push(trace_info_ins) = trace_info;
}


// Processes
// ---------

extern min::locatable_var<min::printer> default_printer;

extern min::uns32 trace_indent;
extern char trace_mark;

extern min::uns32 run_stack_limit;
extern min::uns32 run_return_stack_limit;
extern min::uns32 run_counter_limit;
extern min::uns32 run_trace_flags;
extern int run_excepts_mask;
extern bool run_optimize;

enum state
{
    NEVER_STARTED,
    RUNNING,
    MODULE_END,
    CALL_END,
    COUNTER_LIMIT_STOP,
    STACK_LIMIT_STOP,
    RETURN_STACK_LIMIT_STOP,
    ERROR_STOP,
    JMP_ERROR,
    FORMAT_ERROR,
    EXCEPTS_ERROR,
    NUMBER_OF_STATES
};

struct state_info
{
    mex::state state;
    const char * name;
    const char * description;
};
extern state_info state_infos[];
    

const unsigned max_lexical_level = 16;

struct process_header;
typedef min::packed_vec_insptr
               <min::gen, mex::process_header>
	       process;

struct pc
{
    const mex::module module;
    min::uns32 index;
    pc ( const mex::pc & pc ) :
	module ( pc.module ), index ( pc.index ) {}
    pc ( const mex::module module, min::uns32 index ) :
	module ( module ), index ( index ) {}
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
    min::uns32 saved_level;
    min::uns32 saved_fp;
    min::uns32 saved_nargs;
    min::uns32 nresults;
};

typedef min::packed_vec_insptr<mex::ret> return_stack;

struct process_header
{
    const min::uns32 control;
    const min::uns32 length;
    const min::uns32 max_length;
    min::printer printer;
    const mex::pc pc;
    mex::return_stack return_stack;
    min::uns32 level;
    min::uns32 fp[mex::max_lexical_level + 1];
    min::uns32 nargs[mex::max_lexical_level + 1];
    min::uns32 trace_depth;
    min::uns32 trace_flags;
    int excepts_mask;
    int excepts_accumulator;
    bool optimize;
    min::uns32 counter;
    min::uns32 optimized_counter;
    min::uns32 counter_limit;
    mex::state state;
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
