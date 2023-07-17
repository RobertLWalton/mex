// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Mon Jul 17 06:58:29 EDT 2023
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

extern min::locatable_gen op_code_table;
    // For an op_code OP,
    //   op_code_table[OP] == new_str_gen ( "OP" )
    // and
    //   get ( op_code_table, new_str_gen ( "OP" ) )
    //          == new_num_gen ( OP )

// Variable Stack
//
struct variable_element
{
    const min::gen name;  // min::MISSING() if none.
    min::uns32 level, depth;
    variable_element & operator =
	    ( const variable_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
        * (min::gen *) & this->name = e.name;
	this->level = e.level;
	this->depth = e.depth;
	return * this;
    }
};
typedef min::packed_vec_insptr<mexas::variable_element>
    variable_stack;
extern min::locatable_var<mexas::variable_stack>
    variables;
inline void push
	( mexas::variable_stack s, min::gen name,
	  min::uns32 level, min::uns32 depth )
{
    mexas::variable_element e = { name, level, depth };
    min::push(s) = e;
    min::unprotected::acc_write_update ( s, name );
}

// Function Stack
//
struct function_element
{
    const min::gen name;
    min::uns32 level, depth;
    const mex::pc pc;
    function_element & operator =
	    ( const function_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
        * (min::gen *) & this->name = e.name;
	this->level = e.level;
	this->depth = e.depth;
	* (mex::pc *) & this->pc = e.pc;
	return * this;
    }
};
typedef min::packed_vec_insptr<mexas::function_element>
    function_stack;
extern min::locatable_var<mexas::function_stack>
    functions;
inline void push
	( mexas::function_stack s, min::gen name,
	  min::uns32 level, min::uns32 depth,
	  mex::pc pc )
{
    mexas::function_element e =
        { name, level, depth, pc };
    min::push(s) = e;
    min::unprotected::acc_write_update ( s, name );
    min::unprotected::acc_write_update ( s, pc.module );
}

// Block Stack
//
typedef min::packed_vec_insptr<mex::op_code>
    block_stack;
extern min::locatable_var<mexas::block_stack>
    blocks;
    // Top of stack is BEG... instruction for the last
    // block that has not yet seen its END...
    // instruction.   BEG... instructions are pushed
    // into stack.  END... instructions check stack
    // and pop stack.

// Module Stack
//
typedef min::packed_vec_insptr<mex::module>
    module_stack;
extern min::locatable_var<mexas::module_stack>
    modules;
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
extern min::locatable_var<mexas::jump_list>
    jumps;
    // The jump list is a singly linked list in the
    // jump_list vector, with jump_list[0] being a
    // dummy element that is head of the free list,
    // and jump_list[1] being a dummy element that is
    // head of the active list.  When elements are
    // added they are added to the start, so the
    // list order is newest-first.

extern min::locatable_var<min::file> input_file;
typedef min::packed_vec_insptr<min::gen>
    statement_lexemes;
extern min::locatable_var<mexas::statement_lexemes>
    statement;
    // Vector of all the lexemes in a statement.
    // A MEXAS string lexeme takes two elements that are
    // MIN strings, the first a "'" or "\"", and the
    // second the characters in between quotes.  Number
    // lexemes are MIN number general values, and name
    // lexemes are MIN string general values.
extern min::uns32 first_line_number,
                  last_line_number;
    // First and last line numbers of the statement.
    // The first line in the file is number 0.
bool next_statement ( void );
    // Get the next statement and return true.  Or
    // return false if end of file.

mex::module compile ( min::file );
    // Compile file and return module.  Also push
    // module into module stack.  If there is a compile
    // error, to not produce a new module and return
    // NULL_STUB.

} // end mexas namespace

# endif // MEXAS_H
