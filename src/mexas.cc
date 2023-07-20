// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul 20 02:48:23 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup and Data
//	Support Functions
//	Scanner Function
//	Compile Function


// Setup and Data
// ----- --- ----

# include <mexas.h>

min::uns32 mexas::error_count;
min::uns32 mexas::warning_count;

min::uns32 mexas::lexical_level;
min::uns32 mexas::depth;
min::uns32 mexas::lp[mex::max_lexical_level+1];
min::uns32 mexas::fp[mex::max_lexical_level+1];

min::locatable_gen mexas::star;

min::locatable_gen mexas::op_code_table;

enum op_code
    // Extends mex::op_code to include pseudo-ops and
    // declarations.
{
    PUSHM = mex::NUMBER_OF_OP_CODES,
    PUSH,
    POP,
    CALL,
    LABEL,
    INSTRUCTION_TRACE,
    DEFAULT_TRACE,
    NUMBER_OF_OP_CODES
};

static mex::op_info op_infos[] =
{
    { ::PUSHM, mex::NONA, "PUSHM" },
    { ::PUSH, mex::NONA, "PUSH" },
    { ::POP, mex::NONA, "POP" },
    { ::CALL, mex::NONA, "CALL" },
    { ::LABEL, mex::NONA, "LABEL" },
    { ::INSTRUCTION_TRACE, mex::NONA,
                           "INSTRUCTION_TRACE" },
    { ::DEFAULT_TRACE, mex::NONA,
                            "DEFAULT_TRACE" }
};

static void init_op_code_table ( void )
{
    mexas::op_code_table = min::new_obj_gen
        ( 10 * mex::NUMBER_OF_OP_CODES,
	   4 * mex::NUMBER_OF_OP_CODES,
	   1 * mex::NUMBER_OF_OP_CODES );

    min::obj_vec_insptr vp ( mexas::op_code_table );
    min::attr_insptr ap ( vp );

    min::locatable_gen tmp;
    mex::op_info * p = mex::op_infos;
    mex::op_info * endp = p + mex::NUMBER_OF_OP_CODES;
    while  ( p < endp )
    {
        tmp = min::new_str_gen ( p->name );
        min::attr_push(vp) = tmp;
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( p->op_code );
	min::set ( ap, tmp );
	++ p;
    }
    p = ::op_infos;
    endp = p + (   ::NUMBER_OF_OP_CODES
	         - mex::NUMBER_OF_OP_CODES );
    while  ( p < endp )
    {
        tmp = min::new_str_gen ( p->name );
        min::attr_push(vp) = tmp;
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( p->op_code );
	min::set ( ap, tmp );
	++ p;
    }
}

static min::uns32 variable_element_gen_disp[] =
{
    min::DISP ( & mexas::variable_element::name ),
    min::DISP_END
};

static min::packed_vec<mexas::variable_element>
     variable_stack_vec_type
         ( "variable_stack_vec_type",
	   ::variable_element_gen_disp );

min::locatable_var<mexas::variable_stack>
    mexas::variables;

static min::uns32 function_element_gen_disp[] =
{
    min::DISP ( & mexas::function_element::name ),
    min::DISP_END
};

static min::uns32 function_element_stub_disp[] =
{
    mex::DISP ( & mexas::function_element::pc ),
    min::DISP_END
};

static min::packed_vec<mexas::function_element>
     function_stack_vec_type
         ( "function_stack_vec_type",
	   ::function_element_gen_disp,
	   ::function_element_stub_disp );

min::locatable_var<mexas::function_stack>
    mexas::functions;

static min::packed_vec<mex::op_code>
     block_stack_vec_type
         ( "block_stack_vec_type" );

min::locatable_var<mexas::block_stack>
    mexas::blocks;

static min::uns32 module_stack_element_stub_disp[] =
{
    0, min::DISP_END
};

static min::packed_vec<mex::module>
     module_stack_vec_type
         ( "module_stack_vec_type",
	   NULL,
	   ::module_stack_element_stub_disp );

min::locatable_var<mexas::module_stack>
    mexas::modules;

static min::uns32 jump_element_gen_disp[] =
{
    min::DISP ( & mexas::jump_element::target_name ),
    min::DISP_END
};

static min::packed_vec<mexas::jump_element>
     jump_list_vec_type
         ( "jump_list_vec_type",
	   ::jump_element_gen_disp );

min::locatable_var<mexas::jump_list>
    mexas::jumps;

min::locatable_var<min::file> mexas::input_file;
min::locatable_var<mexas::statement_lexemes>
    mexas::statement;
min::uns32 mexas::first_line_number,
           mexas::last_line_number;

static min::locatable_gen single_quote;
static min::locatable_gen double_quote;
static min::locatable_gen backslash;

static void initialize ( void )
{
    mexas::star = min::new_str_gen ( "*" );
    ::single_quote = min::new_str_gen ( "'" );
    ::double_quote = min::new_str_gen ( "\"" );
    ::backslash = min::new_str_gen ( "\\" );

    ::init_op_code_table();

    mexas::variables =
	::variable_stack_vec_type.new_stub ( 1000 );
    mexas::functions =
	::function_stack_vec_type.new_stub ( 100 );
    mexas::blocks =
	::block_stack_vec_type.new_stub ( 100 );
    mexas::modules =
	::module_stack_vec_type.new_stub ( 500 );
    mexas::jumps =
	::jump_list_vec_type.new_stub ( 500 );
    mexas::statement =
	min::gen_packed_vec_type.new_stub ( 500 );
}
static min::initializer initializer ( ::initialize );


// Support Functions
// ------- ---------

static void print_error_or_warning
	( const min::phrase_position & pp,
	  const char * message1,
	  const min::op & message2,
	  const char * message3,
	  const min::op & message4,
	  const char * message5,
	  const min::op & message6,
	  const char * message7,
	  const min::op & message8,
	  const char * message9 )
{
    min::printer printer = mexas::input_file->printer;
    printer << min::bom;
    if ( pp )
        printer << "in "
		<< min::pline_numbers
		       ( mexas::input_file, pp )
	        << ": ";
    printer << message1 << message2 << message3
            << message4 << message5 << message6
	    << message7 << message8 << message9;
    if ( pp ) printer << ":";
    printer << min::eom;

    if ( pp )
	min::print_phrase_lines
	    ( printer, mexas::input_file, pp );
}

void mexas::compile_error
	( const min::phrase_position & pp,
	  const char * message1,
	  const min::op & message2,
	  const char * message3,
	  const min::op & message4,
	  const char * message5,
	  const min::op & message6,
	  const char * message7,
	  const min::op & message8,
	  const char * message9 )
{
    mexas::input_file->printer << min::bol << "ERROR: ";
    ::print_error_or_warning
        ( pp, message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexas::error_count;
}

void mexas::compile_warn
	( const min::phrase_position & pp,
	  const char * message1,
	  const min::op & message2,
	  const char * message3,
	  const min::op & message4,
	  const char * message5,
	  const min::op & message6,
	  const char * message7,
	  const min::op & message8,
	  const char * message9 )
{
    mexas::input_file->printer
        << min::bol << "WARNING: ";
    ::print_error_or_warning
        ( pp, message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexas::warning_count;
}



// Scanner Function
// ------- --------

static void scan_error
        ( const char * message,
	  const char * header = "ERROR" )
{
    mexas::input_file->printer
        << min::bol << header << ": " << min::bom
	<< "line " << mexas::last_line_number + 1
	<< ": " << message << min::eom;
    print_line ( mexas::input_file->printer,
                 mexas::input_file,
		 mexas::last_line_number );
}

static void scan_warning
        ( const char * message )
{
    ::scan_error ( message, "WARNING" );
}

bool mexas::next_statement ( void )
{
    min::pop ( mexas::statement,
               mexas::statement->length );
    min::packed_vec_ptr<char> buffer =
        mexas::input_file->buffer;

    bool statement_started = false;
        // A statement is started by any non-blank line.
    bool continuation_mark_found = false;
        // I.e., ::backslash.

    while ( true )
    {
        // Process next line.

	if ( ! statement_started )
	    mexas::first_line_number =
	        mexas::input_file->next_line_number;
	mexas::last_line_number =
	    mexas::input_file->next_line_number;
	min::uns32 begin_offset =
	    min::next_line ( mexas::input_file );
	min::uns32 end_offset =
	    mexas::input_file->next_offset - 1;
	    // Do not include NUL character.
	if ( begin_offset == min::NO_LINE )
	{
	    begin_offset = min::remaining_offset
		( mexas::input_file );
	    end_offset = begin_offset
	               + min::remaining_length
			     ( mexas::input_file );
	    if ( begin_offset == end_offset )	
	    {
	        // EOF
		//
		if ( continuation_mark_found )
		    ::scan_warning
			( "file ended while seeking"
			  " statement continuation;"
			  " statement terminated" );
		return statement->length != 0;
	    }
	    else min::skip_remaining
	             ( mexas::input_file );
	}

	char work[end_offset - begin_offset + 10];

	const char * p = ~ ( buffer + begin_offset );
	const char * endp = ~ ( buffer + end_offset );
	bool illegal_character_found = false;
	bool lexeme_found = false;

#	define SAVE \
	    begin_offset = end_offset - ( endp - p );
#	define RESTORE \
	    p = ~ ( buffer + begin_offset ); \
	    endp = ~ ( buffer + end_offset );

	while ( true )
	{
	    // Loop through lexemes in a line.

	    // Skip whitespace.
	    //
	    while ( p < endp && std::isspace ( * p ) )
	        ++ p;

	    if ( p >= endp ) goto END_LINE;
	    statement_started = true;

	    // Scan lexeme into work.
	    //
	    char * q = work;
	    char type = * p;
	    if ( type == '\'' || type == '"' )
	    {
	        ++ p;
		while ( p < endp && * p != type )
		{
		    char c = * p ++;
		    if ( ( c < ' ' || c >= 0177 )
		         &&
			 ! isspace ( c ) )
		    {
		        c = '#';
			illegal_character_found = true;
		    }
		    * q ++ = c;
		}
		if ( p == endp )
		{
		    SAVE;
		    ::scan_warning
		        ( "string does not have"
			  " string-ending quote;"
			  " quote added" );
		    RESTORE;
		}
		else ++ p;
	    }
	    else
	    {
	        type = 0;
		while ( p < endp && ! isspace ( * p ) )
		{
		    char c = * p ++;
		    if ( ( c < ' ' || c >= 0177 )
		         &&
			 ! isspace ( c ) )
		    {
		        c = '#';
			illegal_character_found = true;
		    }
		    * q ++ = c;
		}
	    }
	    * q = 0;
	   
	    if ( ! lexeme_found
	         &&
		 type == 0
		 &&
		 ::strcmp ( work, "//" ) == 0 )
	    {
	        // Comment Line
		//
		while ( p < endp )
		{
		    char c = * p ++;
		    if ( ( c < ' ' || c >= 0177 )
		         &&
			 ! isspace ( c ) )
			illegal_character_found = true;
		}
		goto END_LINE;
	    }

	    lexeme_found = true;

	    // Put lexeme in statement.
	    //
	    SAVE;
	    if ( type != 0 )
	    {
	        min::push ( statement ) =
		    ( type == '\'' ? ::single_quote :
		                     ::double_quote );
		min::push ( statement ) =
		    min::new_str_gen ( work );
	    }
	    else
	    {
	        char * endptr;
		double val = std::strtod
		    ( work, & endptr );
		if ( * endptr == 0 )
		    min::push ( statement ) =
			min::new_num_gen ( val );
		else
		    min::push ( statement ) =
			min::new_str_gen ( work );
	    }
	    RESTORE;
	}

	END_LINE:

	if ( illegal_character_found )
	    ::scan_error
		( "illegal character found in line;"
		  " changed to `#'" );

	if ( lexeme_found )
	{
	    if (    statement[statement->length - 1]
	         == ::backslash )
	    {
		continuation_mark_found = true;
		min::pop ( statement );
	    }
	    else
	        break; // End of statement.
	}
    }

    return true;
}

// Compile Function
// ------- --------

mex::module mexas::compile
	( min::file file, min::uns8 default_flags,
	                  min::uns8 compile_flags )
{
    mexas::error_count = 0;
    mexas::warning_count = 0;

    min::pop ( mexas::variables,
               mexas::variables->length );
    min::pop ( mexas::functions,
               mexas::functions->length );
    min::pop ( mexas::blocks,
               mexas::blocks->length );
    min::pop ( mexas::jumps,
               mexas::jumps->length );
    mexas::jump_element e =
        { min::MISSING(), 0, 0, 0, 0, 0, 0 };
    min::push ( jumps ) = e;  // Free head.
    min::push ( jumps ) = e;  // Active head.

    mexas::lexical_level = 0;
    mexas::depth = 0;
    mexas::lp[0] = 0;
    mexas::fp[0] = 0;

    mexas::input_file = file;

    mex::module_ins m = (mex::module_ins)
        mex::create_module ( file );

    while ( next_statement() )
    {
        min::phrase_position pp =
	    { { mexas::first_line_number, 0 },
	      { mexas::last_line_number + 1, 0 } };

        min::gen v = min::get
	    ( mexas::op_code_table,
	      mexas::statement[0] );
	if ( v == min::NONE() )
	{
	    mexas::compile_error
	        ( pp, "undefined operation code"
		      " or declaration name;"
		      " statement ignored" );
	    continue;
	}
	min::uns32 op_code =
	    (min::uns32) min::int_of ( v );
        min::uns8 op_type = mex::NONA;
	mex::instr instr =
	    { 0, default_flags,
	      0, 0, 0, min::MISSING() };
	min::uns32 fp = mexas::fp[mexas::lexical_level];
	if ( op_code < mex::NUMBER_OF_OP_CODES )
	{
	    op_type = mex::op_infos[op_code].op_type;
	    instr.op_code = op_code;
	}
	switch ( op_type )
	{
	case mex::A2:
	    if ( variables->length < fp + 2 )
	        goto STACK_TOO_SHORT;
	    min::pop ( variables, 2 );
	    goto ARITHMETIC;
	}

	STACK_TOO_SHORT:
	{
	    mexas::compile_error
	        ( pp, "stack too empty to pop required"
		      " arguments" );
	    continue;
	}
	ARITHMETIC:
	{
	    min::gen name = mexas::get_name ( 1 );
	    if ( name == min::NONE() )
	        name = mexas::star;
	    mexas::push ( mexas::variables, name,
	                  mexas::lexical_level,
	                  mexas::depth );
	    mex::push_instr ( m, instr );
	}
    }

    return min::NULL_STUB;  // TBD
}
