// MIN System Execution Engine Assembler Main Program
//
// File:	mexas_main.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Sep  1 05:22:43 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

# include <mexas.h>

int main ( int argc, char * argv[] )
{
    min::initialize();

    min::init_ostream
        ( mex::default_printer, std::cout );
    mex::default_printer << min::ascii;

    min::printer printer = mex::default_printer;

    min::locatable_gen tmp1, tmp2;

    int i = 1;
    while ( i < argc )
    {
        const char * arg = argv[i++];
	if ( strcmp ( "-pa", arg ) == 0 )
	    mexas::assemble_print_switch =
	        mexas::PRINT;
	else if ( strcmp ( "-pasource", arg ) == 0 )
	    mexas::assemble_print_switch =
	        mexas::PRINT_WITH_SOURCE;
	else if ( strcmp ( "-paoff", arg ) == 0 )
	    mexas::assemble_print_switch =
	        mexas::NO_PRINT;

	else if ( strcmp ( "-tcnormal", arg ) == 0 )
	    mexas::assemble_trace_never = false;
	else if ( strcmp ( "-tcnever", arg ) == 0 )
	    mexas::assemble_trace_never = true;

	else if ( strcmp ( "-oon", arg ) == 0 )
	    mexas::run_optimize = false;
	else if ( strcmp ( "-ooff", arg ) == 0 )
	    mexas::run_optimize = true;

	else if ( strncmp ( "-tc:", arg, 4 ) == 0 )
	{
	    min::uns32 flags = (1 << mex::T_ALWAYS );
	    const char * p = arg + 4;
	    while ( * p )
	    {
	        const char * q = p;
		while ( * q && * q != ',' ) ++ q;
		if ( q > p )
		{
		    tmp1 = min::new_str_gen
		        ( p, q - p );
		    tmp2 = min::get
		        ( mexas::trace_flag_table,
			  tmp1 );
		    if ( tmp2 != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( tmp2 );
			flags |= (min::uns32) f;
		    }
		    else
		        printer
			    << min::bol
			    << "trace class/group "
			    << min::pgen ( tmp1 )
			    << " unrecognized;"
			       " ignored"
			    << min::eol;
		}
		if ( * q ) ++ q;
		p = q;
	    }
	    mexas::run_trace_flags = flags;
	}

	else if ( strncmp ( "-ex:", arg, 4 ) == 0 )
	{
	    int excepts = 0;
	    const char * p = arg + 4;
	    while ( * p )
	    {
	        const char * q = p;
		while ( * q && * q != ',' ) ++ q;
		if ( q > p )
		{
		    tmp1 = min::new_str_gen
		        ( p, q - p );
		    tmp2 = min::get
		        ( mexas::except_flag_table,
			  tmp1 );
		    if ( tmp2 != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( tmp2 );
			excepts |= (int) f;
		    }
		    else
		        printer
			    << min::bol
			    << "except name "
			    << min::pgen ( tmp1 )
			    << " unrecognized;"
			       " ignored"
			    << min::eol;
		}
		if ( * q ) ++ q;
		p = q;
	    }
	    mexas::run_excepts = excepts;
	}
	else if ( strcmp ( "-", arg ) == 0 )
	{
	    min::locatable_var<min::file> file;
	    min::init_printer
	        ( file, mex::default_printer );
	    min::init_input_stream ( file, std::cin );
	    bool result = mexas::compile ( file );
	    if ( ! result )
	    {
	        printer << min::bol
		        << "exiting due to compile"
		           " errors"
			<< min::eol;
		return 1;
	    }
	    else
	        printer << min::bol
		        << "standard input successfully"
		           " compiled"
			<< min::eol;
	}
	else if ( arg[0] == '-' )
	{
	    printer << min::bol
	            << "unrecognized option: " << arg
		    << min::eol;
	    return 1;
	}
	else
	{
	    min::locatable_gen file_name
	        ( min::new_str_gen ( arg ) );
	    min::locatable_var<min::file> file;
	    min::init_printer
	        ( file, mex::default_printer );
	    min::init_input_named_file
	        ( file, file_name );
	    bool result = mexas::compile ( file );
	    if ( ! result )
	    {
	        printer << min::bol
		        << "exiting due to compile"
		           " errors"
			<< min::eol;
		return 1;
	    }
	    else
	        printer << min::bol
			<< arg
		        << " successfully compiled"
			<< min::eol;
	}
    }
    return 0;
}
