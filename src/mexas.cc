// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jul 14 13:17:29 EDT 2023
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

min::locatable_var<mexas::variable_stack>
    mexas::variable_stack;

static min::uns32 variable_element_gen_disp[] =
{
    min::DISP ( & mexas::variable_element::name ),
    min::DISP_END
};

static min::packed_vec<mexas::variable_element>
     variable_stack_vec_type
         ( "variable_stack_vec_type",
	   ::variable_element_gen_disp );

min::locatable_var<mexas::function_stack>
    mexas::function_stack;

static min::uns32 function_element_gen_disp[] =
{
    min::DISP ( & mexas::function_element::name ),
    min::DISP_END
};

static min::uns32 function_element_stub[] =
{
    min::DISP ( & mexas::function_element::pc ),
    min::DISP_END
};

static min::packed_vec<mexas::function_element>
     function_stack_vec_type
         ( "function_stack_vec_type",
	   ::function_element_gen_disp,
	   ::function_element_stub_disp );

min::locatable_var<mexas::block_stack>
    mexas::block_stack;

static min::packed_vec<mex::op_code>
     block_stack_vec_type
         ( "block_stack_vec_type" );

min::locatable_var<mexas::module_stack>
    mexas::module_stack;

static min::uns32 module_stack_element_stub_disp[] =
{
    0, min::DISP_END
};

static min::packed_vec<mex::module>
     module_stack_vec_type
         ( "module_stack_vec_type",
	   NULL,
	   ::module_stack_element_stub_disp );

min::locatable_var<mexas::jump_list>
    mexas::jump_list;

static min::uns32 jump_element_gen_disp[] =
{
    min::DISP ( & mexas::jump_element::name ),
    min::DISP_END
};

static min::packed_vec<mexas::jump_element>
     jump_list_vec_type
         ( "jump_list_vec_type",
	   ::jump_element_gen_disp );

min::locatable_var<min::file> mexas::input_file;
min::locatable_var<mexas::statement_lexemes>
    mexas::statement_lexemes;
    // Use min::gen_packed_vector_type.
min::uns32 mexas::first_line, mexas::last_line;


