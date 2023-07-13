// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul 13 03:49:13 EDT 2023
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

# ifndef MEXAS_H
# define MEXAS_H

// Inclusions.
//
# include <mex.h>

namespace mexas {


// Variable Stack
//
struct variable
{
    const min::gen name;  // min::MISSING() if none.
    min::uns32 level, depth;
};
typedef min::packed_vec_insptr<mexas::variable>
    variable_stack;
extern mexas::variable_stack variable_stack;

// Function Stack
//
struct function
{
    const min::gen name;
    min::uns32 level, depth;
    mex::pc pc;
};
typedef min::packed_vec_insptr<mexas::function>
    function_stack;
extern mexas::function_stack function_stack;

} // end mexas namespace

# endif // MEXAS_H

