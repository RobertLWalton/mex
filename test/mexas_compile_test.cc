// MEXAS Compile Test
//
// File:	mexas_compile_test.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Aug  6 23:14:45 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

#include <mexas.h>

int main ( int argc, char * argv[] )
{
    min::initialize();

    min::init_ostream
        ( mex::default_printer, std::cout );
    min::init_printer
        ( mexas::input_file, mex::default_printer );
    min::init_input_stream
        ( mexas::input_file, std::cin );
    mex::default_printer << min::ascii;

    mex::default_printer
        << "Compile module and dump instructions"
	<< min::eol << min::eol;
    mexas::compile_trace_flags =
	mexas::TRACE + mexas::TRACE_LINES;
    bool result = mexas::compile ( mexas::input_file );
    mex::default_printer
        << "Compile returned "
	<< ( result ? "true" : "false" )
	<< min::eol;

    return 0;
}


