// MIN System Execution Engine Assembler
//
// File:	mexas.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Dec 21 07:03:45 PM EST 2024
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
# include <mexcom.h>
# include <mexstack.h>
# include <cctype>


// Data
// ----

namespace mexas {

void push_push_instr
        ( min::gen new_name, min::gen name,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  bool no_source = false,
	  min::int32 offset = 0 );
    // Just like mexstack:push_inst, but constructs the
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
    //
    // Offset is added to the immedA value of any PUSHS,
    // and is the number of trace values pushed before
    // the current PUSH, as these are not recorded in
    // the variables stack.

extern min::locatable_gen star;
    // new_str_gen ( "*" );
extern min::locatable_gen single_quote;
    // new_str_gen ( "'" );
extern min::locatable_gen double_quote;
    // new_str_gen ( "\"" );
extern min::locatable_gen left_bracket;
    // new_str_gen ( "[" );
extern min::locatable_gen right_bracket;
    // new_str_gen ( "]" );

// Variable Stack
//
struct variable_element
{
    const min::gen name;  // mexas::star if none.
    min::uns32 level_and_depth;
        // Level is in high order 16 bits, depth is in
	// low order 16 bits.
    variable_element
	    ( const variable_element & e ) :
	name ( e.name ),
	level_and_depth ( e.level_and_depth ) {}
    variable_element
	    ( min::gen name,
	      min::uns32 level_and_depth ) :
	name ( name ),
	level_and_depth ( level_and_depth ) {}
    variable_element & operator =
	    ( const variable_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
        * (min::gen *) & this->name = e.name;
	this->level_and_depth = e.level_and_depth;
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
    mexas::variable_element e =
        { name, ( level << 16 ) + depth };
    min::push(s) = e;
    min::unprotected::acc_write_update ( s, name );
    ++ mexstack::var_stack_length;
}

// Function Stack
//
struct function_element
{
    const min::gen name;
    min::uns32 level_and_depth;
        // Level is in high order 16 bits, depth is in
	// low order 16 bits.
    min::uns32 index;  // of BEGF in code vector
    function_element
	    ( const function_element & f ) :
	name ( f.name ),
	level_and_depth ( f.level_and_depth ),
        index ( f.index ) {}
    function_element
	    ( min::gen name, min::uns32 level_and_depth,
                             min::uns32 index ) :
	name ( name ),
	level_and_depth ( level_and_depth ),
        index ( index ) {}
    function_element & operator =
	    ( const function_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
        * (min::gen *) & this->name = e.name;
	this->level_and_depth = e.level_and_depth;
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
        { name, ( level << 16 ) + depth, index };
    min::push(s) = e;
    min::unprotected::acc_write_update ( s, name );
    ++ mexstack::func_stack_length;
}


// Main Functions
// ---- ---------

int main ( int argc, char * argv[] );

mex::module compile ( min::file file );
    // Compile file and return module.  Also push
    // module into module stack.  If there is a compile
    // error, to not produce a new module and return
    // NULL_STUB.


// Support Functions
// ------- ---------

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
    if ( min::is_id_str ( lexeme ) )
    {
	min::str_ptr sp ( lexeme );
	return ( isalpha ( sp[0] ) );
    }
    else return false;
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

// Executes get_str and if that returns non-NONE,
// returns its value.  Otherwise checks for '['
// and if found, skips, collects get_str values
// until ']' found, and then puts these values in
// a MIN label and returns that.  Returns NONE
// if neither of these are found (including the
// case of missing ']').
// 
// If MIN label returned, it will need gc
// protection.
//
inline min::gen get_label ( min::uns32 & i )
{
    min::uns32 original_i = i;

    min::gen value = mexas::get_str ( i );
    if ( value != min::NONE() ) return value;

    if ( i + 2 < mexas::statement->length
         &&
	 statement[i] == mexas::left_bracket )
    {
        ++ i;
	min::gen labv [ mexas::statement->length];
	min::uns32 j = 0;
	while ( true )
	{
	    value = mexas::get_str ( i );
	    if ( value == min::NONE() ) break;
	    labv[j++] = value;
	}
	if ( i < mexas::statement->length
	     &&
	     statement[i] == mexas::right_bracket )
	{
	    ++ i;
	    return new_lab_gen ( labv, j );
	}

	i = original_i;
	return min::NONE();
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

bool check_parameter
    ( min::uns32 & param, min::gen n,
      const min::phrase_position & pp,
      const char * pname, bool is_level = false );
    // Convert n to an uns32 param suitable for use as
    // an immedX parameter.  If it does convert, store
    // it in param and return true.  Otherwise output
    // an error message and return false.  Pname is
    // the name of the parameter (e.g. nresults).
    //
    // If is_level is true, convert n to a lexical level
    // L'.  If n > 0, L' = n, but if n < 0, L' = L + n.
    // If L' is not in the range [1,L], output an error
    // message and return false.
    //
    // If an error message is output, it ends with
    // `instruction ignored'.  So if false is returned,
    // the instruction should be ignored.

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

min::uns32 local_search
	( min::gen name, min::phrase_position pp,
	  bool argument_ok = false );
    // Search variables stack for a variable of the
    // given name, and if there is no error, return
    // its index.  If there is an error, output an
    // error message and return NOT_FOUND.  pp is
    // only use by error messages.
    //
    // The errors are:
    //     (1) variable not found in stack
    //     (2) variable of less than current lexical
    //         level
    //     (3) argument_ok == false and variable is
    //         an argument of the current function

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
