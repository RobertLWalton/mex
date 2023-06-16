// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jun 16 17:25:13 EDT 2023
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
    min::uns16 op_code, regA, regB, regC;
    min::gen immed;
};

enum op_code {
    ADD, ADDI,
    SUB, SUB, SUBI, SUBRI,
    MUL, MULI,
    DIV, DIV, DIVI, DIVRI,
    REM, REM, REMI, REMRI,
    EXPI,
    JMP,
    JMPEQ, JMPNE, JMPLT, JMPLEQ, JMPGT, JMPGEQ,
    NEG, ABS,
    ASSIGN, ASSIGNI,
    BEGIN, END, ITERATE, BREAK,
    NOP
};

# endif // MEX_H
