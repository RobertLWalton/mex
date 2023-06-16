// MIN System Execution Engine Interface
//
// File:	mex.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jun 16 07:53:47 EDT 2023
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
    min::uns16 op_code, immedA;
    min::uns32 immedB;
    min::gen immedC;
}

# endif // MEX_H

