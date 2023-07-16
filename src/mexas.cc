// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Jul 15 23:16:18 EDT 2023
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

bool mexas::next_statement ( void )
{
    min::pop ( mexas::statement,
               mexas::statement->length );
    bool statement_started = false;

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
		if ( statement->length == 0 )
		    return false;
	        mexas::input_file->printer
		    << min::bol << "ERROR: "
		    << min::bom
		    << "file ended while seeking"
		       " statement continuation;"
		       " statement terminated"
		    << min::eom;
		return true;
	    }
	    else min::skip_remaining
	             ( mexas::input_file );
	}

	// TBD
    }
    return true;
}


