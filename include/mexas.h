// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jul 14 04:51:27 EDT 2023
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

// Current Lexeme
//
enum lexeme_type
{
    END_OF_LINE,
    END_OF_FILE,
    NAME,
    NUMBER,
    STRING,
    OTHER
};

// Get the next lexeme.  Skip comment lines, and handle
// continuations.  Store results in mexas::lexeme_type
// and mexas::lexeme.  The latter is a MIN string or
// number, and is undefined if the lexeme_type is
// END_OF_... .
//
// Non-ASCII characters and non-whitespace ASCII control
// characters produce an error message and are replaced
// by #.  Each lexeme must be within a line, including
// STRINGs.
//
// The lexeme_line_number of the first line is 1.
//
// If called when lexeme_type is END_OF_FILE, will do
// nothing, leaving the lexeme_type at END_OF_FILE.
//
// Partial line at end of file is treated as a file
// line with error message.
//
extern mexas::lexeme_type lexeme_type;
    // Initialized to END_OF_LINE.
extern min::uns32 lexeme_line_number;
    // Initialized to 0.
extern min::locatable_gen lexeme;
void get_next_lexeme ( void );

// Data on input file.
//
extern min::locatable_var<min::file> file;
extern min::uns32 current_offset;
    // Position in file->buffer of the next character
    // to be scanned.
extern min::uns32 line_end_offset;
    // Offset just after the line ending NUL of the
    // line currently being scanned.
extern min::uns32 illegal_character_count;
    // Number of illegal characters found in line
    // so far.  Illegal characters are replaced by #.
    // Error message is printed when first illegal
    // character encountered.

} // end mexas namespace

# endif // MEXAS_H

