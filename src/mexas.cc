// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jul 16 06:37:26 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Statement Scanner


// Setup
// -----

# include <mexas.h>

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
    mexas::variables
    ( ::variable_stack_vec_type.new_stub ( 1000 ) );

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
    mexas::functions
    ( ::function_stack_vec_type.new_stub ( 100 ) );

static min::packed_vec<mex::op_code>
     block_stack_vec_type
         ( "block_stack_vec_type" );

min::locatable_var<mexas::block_stack>
    mexas::blocks
    ( ::block_stack_vec_type.new_stub ( 100 ) );

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
    mexas::modules
    ( ::module_stack_vec_type.new_stub ( 500 ) );

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
    mexas::jumps
    ( ::jump_list_vec_type.new_stub ( 500 ) );

min::locatable_var<min::file> mexas::input_file;
min::locatable_var<mexas::statement_lexemes>
    mexas::statement
    ( min::gen_packed_vec_type.new_stub ( 500 ) );
min::uns32 mexas::first_line_number,
           mexas::last_line_number;

static min::locatable_gen single_quote;
static min::locatable_gen double_quote;
static min::locatable_gen backslash;

static void initialize ( void )
{
    ::single_quote = min::new_str_gen ( "'" );
    ::double_quote = min::new_str_gen ( "\"" );
    ::backslash = min::new_str_gen ( "\\" );
}
static min::initializer initializer ( ::initialize );


// Statement Scanner
// --------- -------

static void scan_error
        ( const char * message,
	  const char * header = "ERROR" )
{
    mexas::input_file->printer
        << min::bol << header << ": " << min::bom
	<< "line " << mexas::last_line_number
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
		if ( p = endp )
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
	        min::push ( statement ) =
		    ( type == '\'' ? ::single_quote :
		                     ::double_quote );
	    min::push ( statement ) =
	        min::new_str_gen ( work );
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


