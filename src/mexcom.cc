// MIN System Execution Engine Assembler
//
// File:	mexcom.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jun  2 15:37:29 EDT 2024
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup and Data
//	Support Functions


// Setup and Data
// ----- --- ----

# include <mexcom.h>

bool mexcom::trace_never = false;

min::uns32 mexcom::error_count;
min::uns32 mexcom::warning_count;

min::locatable_var<min::file> mexcom::input_file;
min::locatable_var<mex::module_ins>
    mexcom::output_module;

min::locatable_gen mexcom::op_code_table;
min::locatable_gen mexcom::trace_class_table;
min::locatable_gen mexcom::trace_flag_table;
min::locatable_gen mexcom::except_mask_table;

void mexcom::init_op_code_table ( void )
{
    if ( min::is_obj ( mexcom::op_code_table ) )
        return;

    mexcom::op_code_table = min::new_obj_gen
        ( 10 * mex::NUMBER_OF_OP_CODES,
	   4 * mex::NUMBER_OF_OP_CODES,
	   1 * mex::NUMBER_OF_OP_CODES );

    min::obj_vec_insptr vp ( mexcom::op_code_table );
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
}

static void init_trace_class_table ( void )
{
    min::uns32 n = mex::NUMBER_OF_TRACE_CLASSES
                 + 20;
    mexcom::trace_class_table = min::new_obj_gen
        ( 10 * n, 4 * n, 1 * n );

    min::obj_vec_insptr vp
        ( mexcom::trace_class_table );
    min::attr_insptr ap ( vp );

    min::locatable_gen tmp;
    mex::trace_class_info * p = mex::trace_class_infos;
    mex::trace_class_info * endp =
        p + mex::NUMBER_OF_TRACE_CLASSES;
    while  ( p < endp )
    {
        tmp = min::new_str_gen ( p->name );
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( p->trace_class );
	min::set ( ap, tmp );
	++ p;
    }
}

const unsigned NUMBER_OF_TRACE_GROUPS = 4;
static struct trace_group
  { const char * name; min::uns32 flags; }
    trace_groups[NUMBER_OF_TRACE_GROUPS] = {
    { "ALL", ( (1<<mex::NUMBER_OF_TRACE_CLASSES) - 1 )
             & ~ ( (1<<mex::T_NEVER)
	          +(1<<mex::T_ALWAYS))},
    { "NONE", 0 },
    { "FUNC", (1<<mex::T_CALLM) | (1<<mex::T_CALLG) |
              (1<<mex::T_RET) | (1<<mex::T_ENDF) },
    { "LOOP", (1<<mex::T_BEGL) | (1<<mex::T_CONT) |
              (1<<mex::T_ENDL) }
    };

static void init_trace_flag_table ( void )
{
    min::uns32 n = mex::NUMBER_OF_TRACE_CLASSES
                 + NUMBER_OF_TRACE_GROUPS
                 + 20;
    mexcom::trace_flag_table = min::new_obj_gen
        ( 10 * n, 4 * n, 1 * n );

    min::obj_vec_insptr vp ( mexcom::trace_flag_table );
    min::attr_insptr ap ( vp );

    min::locatable_gen tmp;
    mex::trace_class_info * p = mex::trace_class_infos;
    mex::trace_class_info * endp =
        p + mex::NUMBER_OF_TRACE_CLASSES;
    while  ( p < endp )
    {
        tmp = min::new_str_gen ( p->name );
	if ( p->trace_class != mex::T_NEVER
	     &&
	     p->trace_class != mex::T_ALWAYS )
	{
	    min::locate ( ap, tmp );
	    tmp = min::new_num_gen
	        ( 1 << p->trace_class );
	    min::set ( ap, tmp );
	}
	++ p;
    }

    trace_group * q = trace_groups;
    trace_group * endq = q + NUMBER_OF_TRACE_GROUPS;
    while ( q < endq )
    {
        tmp = min::new_str_gen ( q->name );
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( q->flags );
	min::set ( ap, tmp );
        ++ q;
    }
}

static void init_except_mask_table ( void )
{
    min::uns32 n = mex::NUMBER_OF_EXCEPTS + 20;
    mexcom::except_mask_table = min::new_obj_gen
        ( 10 * n, 4 * n, 1 * n );

    min::obj_vec_insptr vp
        ( mexcom::except_mask_table );
    min::attr_insptr ap ( vp );

    min::locatable_gen tmp;
    mex::except_info * p = mex::except_infos;
    mex::except_info * endp =
        p + mex::NUMBER_OF_EXCEPTS;
    while  ( p < endp )
    {
        tmp = min::new_str_gen ( p->name );
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( p->mask );
	min::set ( ap, tmp );
	++ p;
    }
}

static min::uns32 module_stack_element_stub_disp[] =
{
    0, min::DISP_END
};

static min::packed_vec<mex::module>
     module_stack_vec_type
         ( "module_stack_vec_type",
	   NULL,
	   ::module_stack_element_stub_disp );

min::locatable_var<mexcom::module_stack>
    mexcom::modules;

static void initialize ( void )
{
    mexcom::init_op_code_table();
    ::init_trace_class_table();
    ::init_trace_flag_table();
    ::init_except_mask_table();

    mexcom::modules =
	::module_stack_vec_type.new_stub ( 500 );
}
static min::initializer initializer ( ::initialize );


// Support Functions
// ------- ---------

static void print_error_or_warning
	( const char * type,
	  const min::phrase_position & pp,
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
    min::printer printer = mexcom::input_file->printer;
    printer << min::bol << min::bom
            << type << ": " << min::place_indent ( 0 );
    if ( pp )
        printer << "in "
		<< min::pline_numbers
		       ( mexcom::input_file, pp )
	        << ": ";
    printer << message1 << message2 << message3
            << message4 << message5 << message6
	    << message7 << message8 << message9;
    if ( pp ) printer << ":";
    printer << min::eom;

    if ( pp )
	min::print_phrase_lines
	    ( printer, mexcom::input_file, pp );
}

void mexcom::compile_error
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
    ::print_error_or_warning
        ( "ERROR", pp,
	      message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexcom::error_count;
}

void mexcom::compile_warn
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
    ::print_error_or_warning
        ( "WARNING", pp,
	      message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexcom::warning_count;
}
