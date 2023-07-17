// MEXAS Scanner Test
//
// File:	mexas_scanner_test.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jul 16 22:15:52 EDT 2023
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

    while ( mexas::next_statement() )
    {
        mex::default_printer << mexas::statement[0] << min::eol;
    }
    return 0;
}

