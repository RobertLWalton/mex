// MIN System Execution Engine Compiler Support
//
// File:	mexcom.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Mon May 26 10:59:19 PM EDT 2025
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

extern min::locatable_var<min::printer> printer;
extern min::locatable_var<min::file> input_file;
extern min::locatable_var<mex::module_ins>
    output_module;

extern min::uns32 error_count;
extern min::uns32 warning_count;

extern min::locatable_gen op_code_table;
extern min::locatable_gen trace_class_table;
extern min::locatable_gen trace_flag_table;
extern min::locatable_gen except_mask_table;

typedef min::packed_vec_insptr<mex::module>
    module_stack;
extern min::locatable_var<mexcom::module_stack>
    modules;


// Support Functions
// ------- ---------

void init_op_code_table ( void );

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

} // end mexcom namespace

# endif // MEXCOM_H
