// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul 13 06:54:57 EDT 2023
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
struct variable_element
{
    const min::gen name;  // min::MISSING() if none.
    min::uns32 level, depth;
};
typedef min::packed_vec_insptr<mexas::variable_element>
    variable_stack;
extern mexas::variable_stack variable_stack;

// Function Stack
//
struct function_element
{
    const min::gen name;
    min::uns32 level, depth;
    mex::pc pc;
};
typedef min::packed_vec_insptr<mexas::function_element>
    function_stack;
extern mexas::function_stack function_stack;

// Block Stack
//
typedef min::packed_vec_insptr<mexas::op_code>
    block_stack;
extern mexas::block_stack block_stack_stack;
    // Top of stack is BEG... instruction for the last
    // block that has not yet seen its END...
    // instruction.

// Jump List
//
struct jump_element
{
    min::uns32 previous, next;
    const min::gen target_name;
    min::uns16 jmp_location;
    min::uns8 lexical_level, depth;
    min::uns16 stack_length, stack_minimum;
};
typedef min::packed_vec_insptr<mexas::jump_element>
    jump_list;
extern mexas::jump_list jump_list;
    // The jump list is a doubly linked list in the
    // jump_list vector, with jump_list[0] being a
    // dummy element that is head of the free list,
    // and jump_list[1} being a dummy element that is
    // head of the active list.


} // end mexas namespace

# endif // MEXAS_H

