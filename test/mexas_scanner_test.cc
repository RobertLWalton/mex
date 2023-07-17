// MEXAS Scanner Test
//
// File:	mexas_scanner_test.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Mon Jul 17 03:01:12 EDT 2023
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

    while ( mexas::next_statement() )
    {
        min::phrase_position pp =
	    { { mexas::first_line_number, 0 },
	      { mexas::last_line_number + 1, 0 } };

	mex::default_printer
	    << min::bol
	    << min::pline_numbers
	           ( mexas::input_file, pp )
	    << ":" << min::eol;

	min::print_phrase_lines
	    ( mex::default_printer,
	      mexas::input_file, pp );
        mex::default_printer << min::bol << "    "
	                     << min::bom;
	for ( min::uns32 i = 0;
	      i < mexas::statement->length; ++ i )
	    mex::default_printer 
		<< ( i != 0 ? " " : "" )
		<< mexas::statement[i];
        mex::default_printer << min::eom;
    }
    return 0;
}

