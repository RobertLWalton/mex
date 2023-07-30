// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jul 30 16:42:15 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Data
//	Main Functions
//	Support Functions


// Setup
// -----

# ifndef MEXAS_H
# define MEXAS_H

# include <mex.h>
# include <cctype>


// Data
// ----

namespace mexas {

extern min::locatable_var<min::file> input_file;
extern min::locatable_var<mex::module_ins>
    output_module;

extern min::uns8 default_trace_flags;
extern min::uns8 next_trace_flags;
    // An instruction is given next_trace_flags which
    // is then immediately reset to default_trace_
    // flags.

// The following pushes an instruction into the module
// after first setting its trace flags.
//
inline void push_instr
        ( mex::instr & instr,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  min::gen trace_info = min::MISSING() )
{
    instr.trace_flags = mexas::next_trace_flags;
    mexas::next_trace_flags =
        mexas::default_trace_flags;

    mex::module_ins m = mexas::output_module;
    min::push(m) = instr;
    min::unprotected::acc_write_update
	( m, instr.immedD );

    min::phrase_position_vec_insptr ppins =
        (min::phrase_position_vec_insptr) m->position;
    min::push(ppins) = pp;

    min::packed_vec_insptr<min::gen> trace_info_ins =
        (min::packed_vec_insptr<min::gen>)
	m->trace_info;
    min::push(trace_info_ins) = trace_info;
}

extern min::uns8 lexical_level;
    // Current lexical_level.
extern min::uns8 depth[mex::max_lexical_level+1];
    // depth[L] is the current depth of lexical level
    // L.

extern min::uns32 lp[mex::max_lexical_level+1];
extern min::uns32 fp[mex::max_lexical_level+1];
    // lp[L] is stack pointer when lexical level begun.
    // fp[L] is frame pointer when lexical level begun.
    // fp[L] - lp[L] is number of arguments.

extern min::uns32 error_count;
extern min::uns32 warning_count;

extern min::locatable_gen star;
    // new_str_gen ( "*" );
extern min::locatable_gen single_quote;
    // new_str_gen ( "'" );
extern min::locatable_gen double_quote;
    // new_str_gen ( "\"" );

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
    const min::gen name;  // mexas::star if none.
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
inline void push_variable
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
    min::uns32 index;  // of BEGF in code vector
    function_element & operator =
	    ( const function_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
        * (min::gen *) & this->name = e.name;
	this->level = e.level;
	this->depth = e.depth;
	this->index = e.index;
	return * this;
    }
};
typedef min::packed_vec_insptr<mexas::function_element>
    function_stack;
extern min::locatable_var<mexas::function_stack>
    functions;
inline void push_function
	( mexas::function_stack s, min::gen name,
	  min::uns32 level, min::uns32 depth,
	  min::uns32 index )
{
    mexas::function_element e =
        { name, level, depth, index };
    min::push(s) = e;
    min::unprotected::acc_write_update ( s, name );
}

// Block Stack
//
struct block_element
{
    min::uns8 begin_op_code;
    min::uns8 end_op_code;
    min::uns32 stack_limit;
    min::uns32 nargs;
    min::uns32 begin_location;
};
typedef min::packed_vec_insptr<mexas::block_element>
    block_stack;
extern min::locatable_var<mexas::block_stack>
    blocks;
    // BEG... instructions push an element into this
    // stack.  Begin_op_code is the BEG... op code and
    // end_op_code is the corresponding END... op code.
    //
    // Begin_location is the location of the BEG...
    // instruction and is used allow backward jump and
    // error messages.
    //
    // Stack_limit is the stack bounary below which
    //   (1) elements cannot be popped by instructions
    //       that do not decrease depth or lexical
    //       level
    //   (2) elements cannot be hidden by elements above
    //       the limit
    //
    // Nargs is the number of stack elements holding
    // arguments to a function (BEGF) or next-variables
    // for a loop (BEGL).  Nargs is 0 for BEG.  Upon
    // encountering the BEG...  the stack pointer is at
    // stack_limit - nargs.

extern min::uns32 stack_limit;
    // This is a cache of the stack_limit of the top
    // element of the block stack.  If the block stack
    // is empty, this is 0.

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
    // When a JMP... instruction is pushed to the end of
    // the code vector, it has immedC == 0, which will
    // trigger a run-time error if the JMP... is
    // executed.  Thus unresolved JMP... instructions 
    // will trigger fatal run-time error.
    //
    const min::gen target_name;
    min::uns32 jmp_location;
    min::uns8 lexical_level, depth, maximum_depth;
    min::uns32 stack_length, stack_minimum;
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
	this->depth = e.depth;
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
inline void push_jump
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

// Main Functions
// ---- ---------

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


// Support Functions
// ------- ---------

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

// If statement[i] exists and is a name, return the name
// and increment i.  Otherwise return min::NONE().
//
inline min::gen get_name ( min::uns32 & i )
{
    if ( i < mexas::statement->length )
    {
	min::gen n = mexas::statement[i];
        min::str_ptr sp ( n );
        if ( sp && strlen ( sp ) >= 1
                && isalpha ( sp[0] ) )
	{
	    ++ i;
	    return n;
	}
    }
    return min::NONE();
}

// If statement[i] exists and is "*", return "*" and
// increment i.  Otherwise return min::NONE().
//
inline min::gen get_star ( min::uns32 & i )
{
    if ( i < mexas::statement->length
         &&
	 statement[i] == mexas::star )
    {
        ++ i;
	return mexas::star;
    }
    return min::NONE();
}

// If statement[i] exists and is a number, return the
// number and increment i.  Otherwise return
// min::NONE().
//
inline min::gen get_num ( min::uns32 & i )
{
    if ( i < mexas::statement->length
         &&
	 min::is_num ( statement[i] ) )
	return statement[i++];
    else
	return min::NONE();
}

// If statement[i] exists and is a quote, return a
// label containing the statement lexemes after the
// quote, and set i = statement->length.  Otherwise
// return min::MISSING().
//
inline min::gen get_trace_info ( min::uns32 & i )
{
    if ( i < mexas::statement->length
         &&
	 ( statement[i] == mexas::single_quote
	   ||
	   statement[i] == mexas::double_quote ) )
    {
        min::uns32 len = statement->length - i - 1;
	min::gen labbuf[len];
	for ( min::uns32 j = 0; j < len; ++ j )
	    labbuf[j] = statement[i+1+j];
	i = statement->length;
	return new_lab_gen ( labbuf, len );
    }
    else
	return min::MISSING();
}

extern min::uns8 compile_trace_flags;
    // mex::TRACE and mex::TRACE_LINES to print
    // compiled instructions as they are assembled.
void trace_instr ( min::uns32 location );
    // Print trace of instruction at mexas::ouput_
    // _module[location], as per compile_trace_flags.

unsigned jump_list_delete
	( mexas::jump_list jlist );
    // Go through jlist and delete all jump_elements
    // that have lexical level equal to mexas::
    // lexical_level.  This is to be called just BEFORE
    // mexas::lexical_level is decremented, or at the
    // end of compilation.
    //
    // If this is done, the jump_elements in jlist
    // will be sorted in lexical level order, highest
    // first.  This is assumed by this function.
    //
    // For each element deleted, call compile_error
    // indicating that the jump target was undefined
    // and referencing the jump instruction involved.
    //
    // Return the number of elements deleted.

unsigned jump_list_update
	( mexas::jump_list jlist );
    // Let L be the value of mexas::lexical_level.
    //
    // Go through jlist and for all jump_elements je of
    // lexical level equal to L, perform:
    //
    //     je.maximum_depth =
    //         min ( je.maximum_depth,
    //               mexas::depth[L] )
    //     je.stack_minimum =
    //         min ( je.stack_minimum,
    //               variables->length );
    //
    // This function should be called just AFTER 
    // mexas::depth[L] has been decremented and the
    // stack lengt, variables->length, has been
    // reduced.
    //
    // Assumes that the elements of jlist are sorted by
    // lexical level, highest first, and L is equal to
    // or higher than the lexical level of the first
    // element on jlist.
    //
    // Return the number of elements of the given
    // lexical level (counted even if they are not
    // modified).
    

unsigned jump_list_resolve
	( mexas::jump_list jlist,
	  min::gen target_name );
    // Go through jlist and resolve all jump_elements
    // that have the current lexical level, a depth
    // not greater than their maximum_depth, and a
    // target_name equal to the argument.  Resolved
    // elements are removed from jlist.  The number
    // of resolved elements is returned.

void begx ( mex::instr & instr,
            const min::phrase_position & pp =
	       min::MISSING_PHRASE_POSITION,
	   min::gen trace_info = min::MISSING() );
    // Push a block stack entry for BEG, BEGL, or BEGF
    // respectively, according to instr.op_code,
    // and execute mexas::push_instr on the arguments.
    //
    // BEGF increments mexas::lexical level to be L,
    // sets depth[L] to 0, sets lp[L] = variables->
    // length and fp[L] = lp[L] + instr.immedC.
    //
    // *** Immediately AFTER a call to this function,
    // BEGF must push instr.immedC variables stack
    // elements.
    //
    // BEG and BEGL increment depth[L] for the current
    // lexical level L.
    //
    // BEGL takes the top instr.immedB elements of the
    // variables stack and pushes them in order into the
    // variables stack giving the copy of any variable
    // with name N (not equal *) the name `next-N'.

unsigned endx ( mex::instr & instr,
                const min::phrase_position & pp =
	            min::MISSING_PHRASE_POSITION,
	        min::gen trace_info = min::MISSING() );
    // If instr.op_code matches the op_code of the top
    // block stack entry (e.g., ENDL matches BEGL), pop
    // the top block stack entry, adjusting the BEG...
    // instruction associated with that entry and the
    // END... instr argument, and execute mexas::push_
    // instr on the arguments.  Only instr.op_code and
    // instr.trace_flags are used: all other instr
    // fields are set by this function.
    //
    // If instr.op_code does not match the top of the
    // stack, and if it does match a block stack entry,
    // iteratively change instr.op_code to match the
    // top of the stack and execute the previous para-
    // graph, until a block stack entry matching the
    // original instr.op_code is popped.  Note that END
    // and ENDL can only match block stack entries with
    // the current lexical level.  Issue an error
    // message in this case.  The trace_flags of all
    // instructions pushed will be those of instr.
    //
    // Return the number of block stack entries popped.
    // Normally this will be 1.
    //
    // If instr.op_code does not match ANY block stack
    // entries, just issue an error message and return
    // 0.  No instructions are pushed.

const min::uns32 NOT_FOUND = 0xFFFFFFFF;
    // Returned by search functions when not found.

// Search the variables stack top down for an element
// with the given name, and return the index of the
// first element found.  Return NOT_FOUND if no element
// found.
//
// The elements searched are those with indices i-1,
// i-2, i-3, ..., 0 in that order.  Suitable values for
// the argument i are mexas::variables->length and
// mexas::stack_limit.
//
inline min::uns32 search ( min::gen name, min::uns32 i )
{
    while ( i != 0 )
    {
        -- i;
	if ( (&mexas::variables[i])->name == name )
	    return i;
    }
    return mexas::NOT_FOUND;
}

extern min::locatable_gen V, F;  // global search types.

min::uns32 global_search
	( mex::module & m, min::gen module_name,
			   min::gen type,
	                   min::gen name );
    // Search the modules stack for a variable (type
    // mexas::V) or function (type mexas::F)  with the
    // given name.  If module_name is a name, it must
    // match the module file name.  If it is `*', all
    // modules are searched, most recently compiled
    // first.  Returns NOT_FOUND if not found.  Returns
    // index in module globals for variables or module
    // code (of BEGF) for functions and returns module
    // in m, if found.

void make_module_interface ( void );
    // Make output_module->interface from variables
    // stack and functions stack.  The interface is
    // an object used as a hash table to map MIN
    // labels of the form [mexas::V name] for variables
    // or [mexas::F name] for functions onto min::uns32
    // index values.  If a name appears more than once
    // in a stack, the topmost (most recent) is used.

bool check_new_name
	( min::gen name, min::phrase_position pp );
    // Test whether name is a variable name in the
    // variables stack below mexas::stack_limit.  If
    // yes, print `improper hidding' error message using
    // pp and return false.  Otherwise return true.
    //
    // But if name == '*', just return true.

} // end mexas namespace

# endif // MEXAS_H
