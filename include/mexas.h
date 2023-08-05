// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Aug  5 06:09:33 EDT 2023
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

// The following pushes an instruction into the module
// code vector.
//
inline void push_instr
        ( mex::instr & instr,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  min::gen trace_info = min::MISSING() )
{
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
void push_push_instr
        ( min::gen new_name, min::gen name,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION );
    // Just like mexas:push_inst, but constructs the
    // instruction to be pushed to the code vector
    // using a PUSH... op code and using the `name'
    // to specify the stack or module global location
    // to be pushed.  Also constructs the appropriate
    // trace_info.
    //
    // First the variables stack is search for the name,
    // from top to bottom.  If not found, the module
    // globals are searched, most recent module first,
    // using the interface of each module, and stopping
    // at the first location found.
    //
    // If `name' is not found, a compile_error message
    // is generated and the instruction generated is a
    // PUSHI of min::MISSING().
    //
    // The trace_info constructed is [< new_name name >]
    // unless the location is in a module, in which case
    // it is [< new_name module_name name >].  New_name
    // may be mexas::star but name may not be.

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
    min::uns32 function_stack;
    min::uns32 nvars;
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
    //   (1) variable elements cannot be popped by
    //       instructions that do not decrease depth or
    //       lexical level
    //   (2) variable elements cannot be hidden by
    //       elements above the limit
    //
    // Nvars is the number of stack elements holding
    // arguments to a function (BEGF) or next-variables
    // for a loop (BEGL).  Nvars is 0 for BEG.  Upon
    // encountering the BEG...  the stack pointer is at
    // stack_limit - nvars.

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
    ( min::file file, min::uns8 compile_flags = 0 );
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
 
// Return true if the lexeme is a name (MIN string with
// initial letter), and false otherwise.
//
inline bool is_name ( min::gen lexeme )
{
    min::str_ptr sp ( lexeme );
    return ( sp && strlen ( sp ) >= 1
	        && isalpha ( sp[0] ) );
}

// If statement[i] exists and is a name, return the name
// and increment i.  Otherwise return min::NONE().
//
inline min::gen get_name ( min::uns32 & i )
{
    if ( i < mexas::statement->length )
    {
	min::gen n = mexas::statement[i];
	if ( mexas::is_name ( n ) )
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

// If statement[i] exists and is a quote, and
// statement[i+1] exists and is a MIN string,
// return statement[i+1] and increment i twice.
// Otherwise return min::NONE().
//
inline min::gen get_str ( min::uns32 & i )
{
    if ( i + 1 < mexas::statement->length
         &&
	 ( statement[i] == mexas::single_quote
	   ||
	   statement[i] == mexas::double_quote )
	 &&
	 min::is_str ( statement[i+1] ) )
    {
        ++ i;
	return statement[i++];
    }
    else
	return min::NONE();
}

// If statement[i] exists and is a quote, return a MIN
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

min::uns32 get_trace_info
	( min::ref<min::gen> trace_info,
	  min::uns32 & i,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION );
    // Ditto but also call push_push_instr (star,n,pp)
    // for each each variable named n in the trace_info.
    // The variable names are after the initial quoted
    // lexeme, and the value of the first variable named
    // is pushed first into the stack.  If a lexeme that
    // should be a variable name is not, a compile_error
    // message is output, PUSHI min::MISSING() is pushed
    // into the code vector, and the bad name is still
    // included in the trace_info MIN label.
    //
    // mexas::trace_instr is called for each PUSH...
    // instruction output.
    //
    // The variables stack is not changed.  The first
    // thing the possibly traced instruction should do
    // at run-time, after optionally being traced, is
    // pop the values pushed by the PUSH... instructions
    // output by this function.
    //
    // The number of variables (the number of PUSH...
    // instructions ouput) is returned by this function.
    // The trace_info MIN label is returned in the
    // trace_info argument if there is a quoted lexeme
    // at the initial i position; otherwise trace_info
    // is set to min::MISSING(), no PUSH... instructions
    // are output, and 0 is returned.

enum compile_trace_flags
{
    TRACE       = 1 << 0,
    TRACE_LINES = 1 << 1
};
extern min::uns8 compile_trace_flags;
    // mexas::TRACE and mexas::TRACE_LINES to print
    // compiled instructions as they are assembled.
void trace_instr ( min::uns32 location,
                   bool no_lines = false );
    // Print trace of instruction at mexas::ouput_
    // _module[location], as per compile_trace_flags,
    // except ignore TRACE_LINES if no_lines is true.

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
    // mexas::depth[L] has been decremented and
    // variables->length has been reduced.
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
	    min::uns32 nvars, min::uns32 tvars,
	    min::gen trace_info = min::MISSING(),
            const min::phrase_position & pp =
	       min::MISSING_PHRASE_POSITION );
    // Push a block stack entry for BEG, BEGL, or BEGF
    // respectively, according to instr.op_code,
    // and execute mexas::push_instr on the arguments.
    // This function sets all the necessary instr
    // members except op_code and trace_class.  These
    // other members must be initialized to 0 or
    // min::MISSING().
    //
    // Nvars equals nnext for BEGL, the number of
    // variables listed as arguments for BEGF, and must
    // be 0 for BEG.
    //
    // Tvars is the number of trace variables in the
    // run-time stack.  Trace variables should NOT be
    // recorded in the assembler's variables stack.
    //
    // BEG and BEGL increment depth[L] for the current
    // lexical level L.  BEGF increments L.
    //
    // BEGL takes the top nvars elements of the
    // variables stack and pushes them in order into the
    // variables stack giving the copy of any variable
    // with name N (not equal *) the name `next-N'.
    //
    // BEGF increments mexas::lexical level to be L,
    // sets depth[L] to 0, sets lp[L] = variables->
    // length and fp[L] = lp[L] + instr.immedC.
    //
    // **** In addition to the BEGF op_code, the initial
    // instr value must specify the nargs parameter in
    // instr.immedA.  Immediately AFTER a call to this
    // function, BEGF must push instr.immedA variables
    // stack elements.

unsigned endx ( mex::instr & instr,
	        min::uns32 tvars,
	        min::gen trace_info = min::MISSING(),
                const min::phrase_position & pp =
	            min::MISSING_PHRASE_POSITION );
    // If instr.op_code matches the op_code of the top
    // block stack entry (e.g., ENDL matches BEGL), pop
    // the top block stack entry, adjusting the BEG...
    // instruction associated with that entry and the
    // END... instr argument, and execute mexas::push_
    // instr on the arguments.
    //
    // This function sets all the necessary instr
    // members except op_code and trace_class.  These
    // other members must be initialized to 0 or
    // min::MISSING().
    //
    // Tvars is the number of trace variables in the
    // run-time stack.  Trace variables should NOT be
    // recorded in the assembler's variables stack.
    //
    // END and ENDL decrement depth[L] for the current
    // lexical level L.  ENDF decrements L.
    //
    // If instr.op_code does not match the top of the
    // stack, and if it does match a block stack entry,
    // iteratively change instr.op_code to match the
    // top of the stack and execute the previous para-
    // graph, until a block stack entry matching the
    // original instr.op_code is popped.  Note that END
    // and ENDL can only match block stack entries with
    // the current lexical level.  Issue an error
    // message in this case.  The trace_class of all
    // instructions pushed will be those of T_PUSH.
    //
    // Return the number of block stack entries popped.
    // Normally this will be 1.
    //
    // If instr.op_code does not match ANY block stack
    // entries, just issue an error message and return
    // 0.  No instructions are pushed.

void cont ( mex::instr & instr,
	    min::uns32 tvars,
	    min::gen trace_info = min::MISSING(),
            const min::phrase_position & pp =
	        min::MISSING_PHRASE_POSITION );
    // Similar to endx but only handles the CONT
    // instruction.  Does NOT pop the block stack or
    // decrease depth.  Does require the top of the
    // block stack to be a BEGL entry.

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
