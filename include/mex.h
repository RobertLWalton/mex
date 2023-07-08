// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Jul  8 07:04:46 EDT 2023
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
    ADD    = 1,		MUL    = 2,
    ADDI   = 3,		MULI   = 4,
    SUB    = 5,		SUBR   = 6,
    SUBI   = 7,		SUBRI  = 8,
    DIV    = 9,		DIVR   = 10,
    DIVI   = 11,	DIVRI  = 12,
    MOD    = 13,	MODR   = 14,
    MODI   = 15,	MODRI  = 16,
    FLOOR  = 17,	CEIL   = 18,
    TRUNC  = 19,	ROUND  = 20,
    NEG    = 21,	ABS    = 22,
    LOG    = 23,	EXP    = 24,
    LOG10  = 25,	EXP10  = 26,
    SIN    = 27,	ASIN   = 28,
    COS    = 29,	ACOS   = 30,
    TAN    = 31,	ATAN   = 32,
    ATAN2  = 33,	ATAN2R = 34,
    POWI   = 35,
    PUSH   = 36,	PUSHI  = 37,
    PUSHG  = 38,	PUSHM  = 39,
    POP    = 40,	POPM   = 41,
    JMP    = 42,
    JMPEQ  = 43,	JMPNE  = 44,
    JMPLT  = 45,	JMPLEQ = 46,
    JMPGT  = 47,	JMPGEQ = 48,
    BEG    = 49,	NOP    = 50,
    END    = 51,
    BEGL   = 52,
    ENDL   = 53,	CONT   = 54,
    SET_TRACE = 55,
    ERROR  = 56,
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

const unsigned max_lexical_depth = 16;

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
    min::uns32 sp;
    const min::packed_vec_insptr<mex::ret> return_stack;
    min::uns32 rp;
    min::uns32 fp[mex::max_lexical_depth + 1];
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
inline void set_pc ( mex::process p, mex::pc pc )
{
    * (mex::pc *) & p->pc = pc;
    if ( pc.module != min::NULL_STUB )
        min::unprotected::acc_write_update
	    ( p, pc.module );
}

extern min::uns32 trace_indent;
extern char trace_mark;

bool run_process ( mex::process p );

extern min::locatable_var<min::printer> default_printer;

} // end mex namespace

# endif // MEX_H
