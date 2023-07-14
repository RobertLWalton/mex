// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jul 14 05:20:46 EDT 2023
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
typedef min::packed_vec_insptr<mex::op_code>
    block_stack;
extern mexas::block_stack block_stack_stack;
    // Top of stack is BEG... instruction for the last
    // block that has not yet seen its END...
    // instruction.   BEG... instructions are pushed
    // into stack.  END... instructions check stack
    // and pop stack.

// Module Stack
//
typedef min::packed_vec_insptr<mex::module>
    module_stack;
extern mexas::module_stack module_stack_stack;
    // After module is assembled it is pushed into this
    // stack, which is therefore a list of modules
    // in the order they were assembled: most recent
    // on top.

// Jump List
//
struct jump_element
{
    const min::gen target_name;
    min::uns16 jmp_location;
    min::uns8 lexical_level, minimum_depth;
    min::uns16 stack_length, stack_minimum;
    min::uns32 next;
};
typedef min::packed_vec_insptr<mexas::jump_element>
    jump_list;
extern mexas::jump_list jump_list;
    // The jump list is a singly linked list in the
    // jump_list vector, with jump_list[0] being a
    // dummy element that is head of the free list,
    // and jump_list[1} being a dummy element that is
    // head of the active list.  When elements are
    // added they are added to the start, so the
    // list order is newest-first.

// Statement Scanner
//
// A statement is a sequence of physical lines ending
// with a non-comment, non-blank line that is not
// continued.  Blank lines at the beginning of a
// statement are deleted, but comment lines are not,
// and blank and comment lines can occur within a
// statement between a statement line and its
// continuation.
//
// A statement is scanned into a vector of lexemes.
// The elements of the vector are min::gen values.
// Numbers are MIN number general values; names are
// MIN string general values; strings are two MIN string
// general values: the first a single quote character,
// either ' or ", and the second the string inside the
// quotes.
//
// Illegal characters are replaced by # and cause error
// messages.  Only ASCII non-control characters or ASCII
// whitespace characters are legal.
//
// A partial line at the end of a file is treated as a
// full line but with a warning message.
//
// The get_line function returns true if a line was
// gotten and false if not because of end of file.
//
// The first line in the file is number 0.
//
extern min::locatable_var<min::file> input_file;
extern min::packed_vec_insptr<min::gen> line_lexemes;
extern min::uns32 first_line_number, last_line_number;
bool get_line ( void );

} // end mexas namespace

# endif // MEXAS_H

