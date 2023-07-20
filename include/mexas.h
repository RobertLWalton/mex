// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul 20 03:46:55 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Data
//	Functions


// Setup
// -----

# ifndef MEXAS_H
# define MEXAS_H

# include <mex.h>
# include <cctype>


// Data
// ----

namespace mexas {

extern min::uns32 lexical_level;
extern min::uns32 depth;
    // Current lexical_level and depth.

extern min::uns32 lp[mex::max_lexical_level+1];
extern min::uns32 fp[mex::max_lexical_level+1];
    // lp[L] is stack pointer when lexical level begun.
    // fp[L] is frame pointer when lexical level begun.
    // fp[L] - lp[L] is number of arguments.

extern min::uns32 error_count;
extern min::uns32 warning_count;

extern min::locatable_gen star;
    // new_str_gen ( "*" );

extern min::locatable_gen op_code_table;
    // For op_code OP < ::NUMBER_OF_OP_CODES:
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
    min::uns8 lexical_level, maximum_depth;
    min::uns16 stack_length, stack_minimum;
    min::uns32 next;
    jump_element & operator =
	    ( const jump_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
        * (min::gen *) & this->target_name =
	      e.target_name;
	this->jmp_location = e.jmp_location;
	this->lexical_level = e.lexical_level;
	this->maximum_depth = e.maximum_depth;
	this->stack_length = e.stack_length;
	this->stack_minimum = e.stack_minimum;
	this->next = e.next;
	return * this;
    }
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

// Push jump_element to head of active list.
//
inline void push
	( mexas::jump_list lst,
	  const mexas::jump_element & e )
{
    mexas::jump_element * free = ~ ( lst + 0 );
    mexas::jump_element * active = free + 1;
    min::uns32 next = free->next;
    if ( next == 0 )
    {
        next = lst->length;
	min::push(lst) = e;
    }
    else
    {
	mexas::jump_element * ep = ~ ( lst + next );
	free->next = ep->next;
        * ep = e;
    }
    (lst + next)->next = active->next;
    active->next = next;
    min::unprotected::acc_write_update
        ( lst, e.target_name );
}

// Functions
// ---------

// Print error message.  If pp != min::MISSING_PHRASE_
// POSITION, print source lines after error message.
// Increment mexas::error_count.  Printer used is
// mexas::input_file->printer.
//
void compile_error
	( const min::phrase_position & pp,
	  const char * message1,
	  const min::op & message2 = min::pnop,
	  const char * message3 = "",
	  const min::op & message4 = min::pnop,
	  const char * message5 = "",
	  const min::op & message6 = min::pnop,
	  const char * message7 = "",
	  const min::op & message8 = min::pnop,
	  const char * message9 = "" );

// Ditto but its a warning message and mexas::warning_
// count is incremented.
//
void compile_warn
	( const min::phrase_position & pp,
	  const char * message1,
	  const min::op & message2 = min::pnop,
	  const char * message3 = "",
	  const min::op & message4 = min::pnop,
	  const char * message5 = "",
	  const min::op & message6 = min::pnop,
	  const char * message7 = "",
	  const min::op & message8 = min::pnop,
	  const char * message9 = "" );

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

inline min::gen get_name ( min::uns32 i )
{
    if ( i < mexas::statement->length )
    {
	min::gen n = mexas::statement[i];
        min::str_ptr sp ( n );
        if ( sp && strlen ( sp ) >= 1
                && isalpha ( sp[0] ) )
	    return n;
    }
    return min::NONE();
}

mex::module compile
    ( min::file file, min::uns8 default_flags = 0,
                      min::uns8 compile_flags = 0 );
    // Compile file and return module.  Also push
    // module into module stack.  If there is a compile
    // error, to not produce a new module and return
    // NULL_STUB.
    //
    // Default_flags become the initial default trace
    // flags, as per the DEFAULT_TRACE instruction.
    // Compile_flags trace the compilation: not the
    // execution (TRACE_NOJMP has no effect).

} // end mexas namespace

# endif // MEXAS_H
