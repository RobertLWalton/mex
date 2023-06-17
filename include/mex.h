// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Jun 17 03:18:04 EDT 2023
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
    min::uns16 op_code;
    min::uns16 immedA, immedB, immedC;
    min::gen immedD;
};

enum op_code {
    ADD, ADDI,
    SUB, SUBR, SUBI, SUBRI,
    MUL, MULI,
    DIV, DIVR, DIVI, DIVRI,
    REM, REMR, REMI, REMRI,
    EXPI,
    JMP,
    JMPEQ, JMPEQI, JMPNE, JMPNEI,
    JMPLT, JMPLTI, JMPLEQ, JMPLEQI,
    JMPGT, JMPGTI, JMPGEQ, JMPGEQI,
    NEG, ABS,
    PUSH, PUSHI, PUSHG,
    BEG, END,
    BEGLOOP, ENDLOOP, BREAK,
    CALL, CALLG,
    RET,
    NOP
};

# endif // MEX_H
