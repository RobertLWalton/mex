// MIN System Execution Engine Compiler Support
//
// File:	mexcom.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu May 23 05:22:13 EDT 2024
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Data
//	Support Functions


// Setup
// -----

# ifndef MEXCOM_H
# define MEXCOM_H

# include <mex.h>

// Data
// ----

namespace mexcom {

extern min::locatable_var<min::file> input_file;
extern min::locatable_var<mex::module_ins>
    output_module;

extern min::uns32 error_count;
extern min::uns32 warning_count;

extern min::locatable_gen op_code_table;
extern min::locatable_gen trace_class_table;
extern min::locatable_gen trace_flag_table;
extern min::locatable_gen except_flag_table;


// Support Functions
// ------- ---------

void init_op_code_table ( void );

extern bool trace_never;
inline void push_instr
        ( mex::instr & instr,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  min::gen trace_info = min::MISSING() )
{
    if ( mexcom::trace_never
         &&
	 instr.trace_class != mex::T_ALWAYS )
        instr.trace_class = mex::T_NEVER;

    mex::module_ins m = mexcom::output_module;
    mex::push_instr ( m, instr );
    mex::push_position ( m, pp );
    mex::push_trace_info ( m, trace_info );
}

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

enum print
{
    NO_PRINT,
    PRINT,
    PRINT_WITH_SOURCE
};
extern mexcom::print print_switch;
void trace_instr ( min::uns32 location,
		   min::uns32 stack_length,
                   bool no_source = false );

} // end mexcom namespace

# endif // MEXCOM_H
