// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jun 30 23:10:53 EDT 2023
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
    NEX, ABS,
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
    TRACE,
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
    min::phrase_position_vec position;
    min::packed_vec_ptr<min::gen> globals;
    min::gen interface;
    min::packed_vec_ptr<min::gen> trace_info;
};
MIN_REF ( min::phrase_position, position, mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>, globals,
                                         mex::module )
MIN_REF ( min::gen, interface, mex::module )
MIN_REF ( min::packed_vec_ptr<min::gen>, trace_info,
                                         mex::module )

typedef min::packed_vec_insptr
               <mex::instr, mex::module_header>;

# endif // MEX_H
