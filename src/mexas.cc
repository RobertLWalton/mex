// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Aug 13 17:26:01 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup and Data
//	Support Functions
//	Scanner Function
//	Compile Function


// Setup and Data
// ----- --- ----

# include <mexas.h>

# define L mexas::lexical_level
# define SP mexas::variables->length

min::uns8 mexas::compile_trace_flags = 0;
min::uns32 mexas::run_trace_flags = mex::T_ALWAYS;
bool mexas::compile_trace_never = false;

min::uns32 mexas::error_count;
min::uns32 mexas::warning_count;

min::locatable_var<min::file> mexas::input_file;
min::locatable_var<mex::module_ins>
    mexas::output_module;

min::uns8 mexas::lexical_level;
min::uns8 mexas::depth[mex::max_lexical_level+1];
min::uns32 mexas::lp[mex::max_lexical_level+1];
min::uns32 mexas::fp[mex::max_lexical_level+1];

min::locatable_gen mexas::star;
min::locatable_gen mexas::V;
min::locatable_gen mexas::F;

min::locatable_gen mexas::op_code_table;
min::locatable_gen mexas::trace_flag_table;

enum op_code
    // Extends mex::op_code to include pseudo-ops and
    // declarations.
{
    PUSHM = mex::NUMBER_OF_OP_CODES,
    PUSH,
    POP,
    CALL,
    LABEL,
    INSTRUCTION_TRACE,
    DEFAULT_TRACE,
    STACKS,
    NUMBER_OF_OP_CODES
};

static mex::op_info op_infos[] =
{
    { ::PUSHM, mex::NONA, 0, "PUSHM" },
    { ::PUSH, mex::NONA, 0, "PUSH" },
    { ::POP, mex::NONA, 0, "POP" },
    { ::CALL, mex::NONA, 0, "CALL" },
    { ::LABEL, mex::NONA, 0, "LABEL" },
    { ::INSTRUCTION_TRACE, mex::NONA,
                           0, "INSTRUCTION_TRACE" },
    { ::DEFAULT_TRACE, mex::NONA,
                            0, "DEFAULT_TRACE" },
    { ::STACKS, mex::NONA, 0, "STACKS" }
};

static void init_op_code_table ( void )
{
    mexas::op_code_table = min::new_obj_gen
        ( 10 * mex::NUMBER_OF_OP_CODES,
	   4 * mex::NUMBER_OF_OP_CODES,
	   1 * mex::NUMBER_OF_OP_CODES );

    min::obj_vec_insptr vp ( mexas::op_code_table );
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
    p = ::op_infos;
    endp = p + (   ::NUMBER_OF_OP_CODES
	         - mex::NUMBER_OF_OP_CODES );
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

const unsigned NUMBER_OF_TRACE_GROUPS = 4;
static struct trace_group
  { const char * name; min::uns32 flags; }
    trace_groups[NUMBER_OF_TRACE_GROUPS] = {
    { "ALL", (min::uns32) -1 },
    { "NONE", 0 },
    { "FUNC", (1<<mex::T_CALLM) | (1<<mex::T_CALLG) |
              (1<<mex::T_BEGF) },
    { "LOOP", (1<<mex::T_BEGL) | (1<<mex::T_CONT) |
              (1<<mex::T_ENDL) }
    };

static void init_trace_flag_table ( void )
{
    min::uns32 n = mex::NUMBER_OF_TRACE_CLASSES
                 + 20;
    mexas::trace_flag_table = min::new_obj_gen
        ( 10 * n, 4 * n, 1 * n );

    min::obj_vec_insptr vp ( mexas::trace_flag_table );
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
    min::uns32 mask =
        (1<<mex::NUMBER_OF_TRACE_CLASSES) - 1;
    mask &= ~ (1<<mex::T_NEVER);
    while ( q < endq )
    {
        tmp = min::new_str_gen ( q->name );
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( q->flags & mask );
	min::set ( ap, tmp );
        ++ q;
    }
}

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
    mexas::variables;

static min::uns32 function_element_gen_disp[] =
{
    min::DISP ( & mexas::function_element::name ),
    min::DISP_END
};

static min::packed_vec<mexas::function_element>
     function_stack_vec_type
         ( "function_stack_vec_type",
	   ::function_element_gen_disp );

min::locatable_var<mexas::function_stack>
    mexas::functions;

static min::packed_vec<mexas::block_element>
     block_stack_vec_type
         ( "block_stack_vec_type" );

min::locatable_var<mexas::block_stack>
    mexas::blocks;

min::uns32 mexas::stack_limit;

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
    mexas::modules;

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
    mexas::jumps;

min::locatable_var<mexas::statement_lexemes>
    mexas::statement;
min::uns32 mexas::first_line_number,
           mexas::last_line_number;

min::locatable_gen mexas::single_quote;
min::locatable_gen mexas::double_quote;
static min::locatable_gen backslash;

static void initialize ( void )
{
    mexas::star = min::new_str_gen ( "*" );
    mexas::V = min::new_str_gen ( "V" );
    mexas::F = min::new_str_gen ( "F" );
    mexas::single_quote = min::new_str_gen ( "'" );
    mexas::double_quote = min::new_str_gen ( "\"" );
    ::backslash = min::new_str_gen ( "\\" );

    ::init_op_code_table();
    ::init_trace_flag_table();

    mexas::variables =
	::variable_stack_vec_type.new_stub ( 1000 );
    mexas::functions =
	::function_stack_vec_type.new_stub ( 100 );
    mexas::blocks =
	::block_stack_vec_type.new_stub ( 100 );
    mexas::modules =
	::module_stack_vec_type.new_stub ( 500 );
    mexas::jumps =
	::jump_list_vec_type.new_stub ( 500 );
    mexas::statement =
	min::gen_packed_vec_type.new_stub ( 500 );
}
static min::initializer initializer ( ::initialize );


// Support Functions
// ------- ---------

static void print_error_or_warning
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
    min::printer printer = mexas::input_file->printer;
    printer << min::bom;
    if ( pp )
        printer << "in "
		<< min::pline_numbers
		       ( mexas::input_file, pp )
	        << ": ";
    printer << message1 << message2 << message3
            << message4 << message5 << message6
	    << message7 << message8 << message9;
    if ( pp ) printer << ":";
    printer << min::eom;

    if ( pp )
	min::print_phrase_lines
	    ( printer, mexas::input_file, pp );
}

void mexas::compile_error
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
    mexas::input_file->printer << min::bol << "ERROR: ";
    ::print_error_or_warning
        ( pp, message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexas::error_count;
}

void mexas::compile_warn
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
    mexas::input_file->printer
        << min::bol << "WARNING: ";
    ::print_error_or_warning
        ( pp, message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexas::warning_count;
}

bool mexas::check_new_name
	( min::gen name, min::phrase_position pp )
{
    if ( name == mexas::star ) return true;

    if (    mexas::search ( name, mexas::stack_limit )
         == mexas::NOT_FOUND )
        return true;

    mexas::compile_error
	( pp,
	  "new variable with name ",
	  min::pgen ( name ),
	  " improperly hides previous variable" );
    return false;
}

min::uns32 mexas::global_search
	( mex::module & m, min::gen module_name,
	                   min::gen type,
			   min::gen name )
{
    min::gen labbuf[2] = { type, name };
    min::locatable_gen label
        ( min::new_lab_gen ( labbuf, 2 ) );

    for ( min::uns32 i = mexas::modules->length;
          0 < i; )
    {
	-- i;
        mex::module m = mexas::modules[i];
	if ( m->name != module_name
	     &&
	     mexas::star != module_name )
	    continue;
	if ( ! min::is_obj ( m->interface ) )
	    continue;
	min::gen result =
	    min::get ( m->interface, label );
	if ( result == min::NONE() )
	    continue;
	return (min::uns32) min::int_of ( result );
    }
    return mexas::NOT_FOUND;
}

void mexas::make_module_interface ( void )
{
    min::locatable_gen interface
        ( min::new_obj_gen ( 16000, 4000 ) );

    for ( min::uns32 i = 0;
          i < mexas::variables->length; ++ i )
    {
	min::gen name = (mexas::variables + i)->name;
	if ( name == mexas::star ) continue;

	// Overrides previous setting.
	//
	min::gen labbuf[2] = { mexas::V, name };
	min::locatable_gen label
	    ( min::new_lab_gen ( labbuf, 2 ) );
	min::locatable_gen index
	    ( min::new_num_gen ( i ) );
	min::set ( interface, label, index );
    }

    for ( min::uns32 i = 0;
          i < mexas::functions->length; ++ i )
    {
	min::ptr<mexas::function_element> p =
	    mexas::functions + i;

	min::gen labbuf[2] = { mexas::F, p->name };
	min::locatable_gen label
	    ( min::new_lab_gen ( labbuf, 2 ) );
	min::locatable_gen index
	    ( min::new_num_gen ( p->index ) );
	min::set ( interface, label, index );
    }

    mex::interface_ref ( output_module ) = interface;
}

unsigned mexas::jump_list_delete
	( mexas::jump_list jlist )
{
    min::ptr<mexas::jump_element> free = jlist + 0;
    min::ptr<mexas::jump_element> previous = jlist + 1;

    mex::module_ins m = mexas::output_module;

    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexas::jump_element> next = jlist + n;
	if (   next->lexical_level
	     < mexas::lexical_level )
	    break;
	mexas::compile_error
	    ( m->position[next->jmp_location],
	      "jump target undefined: ",
	      min::pgen ( next->target_name ),
	      "; JMP... is unresolved" );
	    // Unresolved JMP... instructions have
	    // immedC == 0 which triggers a fatal
	    // run-time error.
	previous->next = next->next;
	next->next = free->next;
	free->next = n;
	++ count;
    }
    return count;
}

unsigned mexas::jump_list_update
	( mexas::jump_list jlist )
{
    min::ptr<mexas::jump_element> previous = jlist + 1;

    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexas::jump_element> next = jlist + n;
	if ( next->lexical_level < L )
	    break;
	if ( next->maximum_depth > depth[L] )
	    next->maximum_depth = depth[L];
	if ( next->stack_minimum > SP )
	    next->stack_minimum = SP;
	previous = next;
	++ count;
    }
    return count;
}

unsigned mexas::jump_list_resolve
	( mexas::jump_list jlist,
	  min::gen target_name )
{
    min::ptr<mexas::jump_element> free = jlist + 0;
    min::ptr<mexas::jump_element> previous = jlist + 1;

    mex::module_ins m = mexas::output_module;
    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexas::jump_element> next = jlist + n;
	if ( next->lexical_level < L )
	    break;
	if ( target_name == next->target_name
	     &&
	     next->maximum_depth >= mexas::depth[L] )
	{
	    min::uns32 depth_diff =
	        next->depth - mexas::depth[L];
	    min::phrase_position pp =
		m->position[next->jmp_location];
	    if ( SP > next->stack_minimum )
		mexas::compile_error
		    ( pp, "code jumped over pushes"
		          " values into the stack;"
			  " JMP unresolved" );
	    else if ( SP < next->stack_minimum )
		mexas::compile_error
		    ( pp, "code jumped over pops"
		          " values from the stack;"
			  " JMP unresolved" );
	    else
	    {
		min::ptr<mex::instr> instr =
		    m + next->jmp_location;
		instr->immedA = next->stack_length
			      - next->stack_minimum;
		instr->immedC = m->length
			      - next->jmp_location;
		instr->trace_depth = depth_diff;

		mexas::trace_instr
		    ( next->jmp_location, true );
	    }

	    previous->next = next->next;
	    next->next = free->next;
	    free->next = n;
	    ++ count;
	}
	else
	    previous = next;
    }
    return count;
}

void mexas::begx ( mex::instr & instr,
		   min::uns32 nvars, min::uns32 tvars,
	           min::gen trace_info,
                   const min::phrase_position & pp )
{
    mexas::block_element e =
        { instr.op_code, 0,
	  mexas::variables->length + nvars,
	  mexas::functions->length,
	  nvars,
	  mexas::output_module->length };

    if ( instr.op_code == mex::BEGF )
    {
        MIN_ASSERT ( mexas::lexical_level
	             <
		     mex::max_lexical_level,
		     "mex::max_lexical_level"
		     " exceeded" );
	MIN_ASSERT ( tvars == 0,
	             "BEGF has trace variables" );
	e.end_op_code = mex::ENDF;

	++ L;
	mexas::depth[L] = 0;
	mexas::lp[L] = mexas::variables->length;
	mexas::fp[L] = mexas::lp[L] + nvars;

	instr.immedA = nvars;
	instr.immedB = L;
    }
    else if ( instr.op_code == mex::BEGL )
    {
	e.end_op_code = mex::ENDL;
	if ( nvars > SP - mexas::stack_limit )
	{
	    mexas::compile_error
	        ( pp, "portion of stack in the"
		      " containing block is smaller"
		      " than the number of"
		      " next-variables" );
	    nvars = SP - mexas::stack_limit;
	        // To protect against excessively
		// large nvars values.
	}
        ++ mexas::depth[L];

	for ( min::uns32 i = 0; i < nvars; ++ i )
	{
	    min::locatable_gen name;
	    if ( nvars > SP - mexas::stack_limit )
	        name = mexas::star;
            else
	    {
	        name = (   mexas::variables
		         + ( SP - nvars ) )->name;
		if ( name != mexas::star )
		{
		    min::str_ptr sp ( name );
		    min::unsptr len =
		        min::strlen ( sp );
		    char buffer[len+10];
		    std::strcpy ( buffer, "next-" );
		    min::strcpy ( buffer + 5, sp );
		    name = min::new_str_gen ( buffer );
		}
	    }
	    mexas::push_variable
	        ( mexas::variables, name,
		  L, mexas::depth[L] );
	}
	instr.immedA = tvars;
	instr.immedB = nvars;
    }
    else if ( instr.op_code == mex::BEG )
    {
	MIN_ASSERT ( nvars == 0,
	             "BEG has non-zero nvars" );
	instr.immedA = tvars;
	e.end_op_code = mex::END;
        ++ mexas::depth[L];
    }
    else
        MIN_ABORT
	    ( "bad instr.op_code to mexas::begx" );

    min::push ( mexas::blocks ) = e;
    mexas::stack_limit = e.stack_limit;
    mexas::push_instr ( instr, pp, trace_info );
}

unsigned mexas::endx ( mex::instr & instr,
		       min::uns32 tvars,
	               min::gen trace_info,
                       const min::phrase_position & pp )
{
    if ( mexas::blocks->length == 0 )
    {
	mexas::compile_error
	    ( pp, "there is no block to end here" );
	return 0;
    }
    min::ptr<mexas::block_element> bp =
        mexas::blocks + ( mexas::blocks->length - 1 );
    if ( bp->end_op_code != instr.op_code )
    {
        // Check that ending other blocks will
	// succeed.
	//
	bool match_found = false;
	for ( int i = 2;
	      i <= (int) mexas::blocks->length; ++ i )
	{
	    bp = mexas::blocks
	       + ( mexas::blocks->length - i );
	    min::uns8 end_op_code = bp->end_op_code;
	    if ( end_op_code == instr.op_code )
	    {
	        match_found = true;
		break;
	    }
	    else if ( end_op_code == mex::ENDF )
	        break;
	}
	if ( ! match_found )
	{
	    mexas::compile_error
		( pp, "there is no block to end here" );
	    return 0;
	}
	min::uns8 end_op_code = instr.op_code;
	unsigned count = 0;
	do {
	    bp = mexas::blocks
	       + ( mexas::blocks->length - 1 );
	    instr.op_code = bp->end_op_code;
	    if ( instr.op_code != end_op_code )
	    {
	        min::obj_vec_ptr vp =
		    mexas::op_code_table;
		mexas::compile_error
		    ( pp, "inserting ",
			  min::pgen
			      ( vp[instr.op_code] ),
			  " before here" );
	    }
	    mexas::endx ( instr, pp, trace_info );
	    ++ count;
	} while ( instr.op_code != end_op_code );
	return count;
    }

    mexas::block_element e = min::pop ( mexas::blocks );
    min::pop ( mexas::functions,
                 mexas::functions->length
	       - e.function_stack );
 
    if ( instr.op_code == mex::ENDF )
    {
	min::ptr<mex::instr> ip =
	    mexas::output_module + e.begin_location;
        ip->immedC = mexas::output_module->length + 1
	           - e.begin_location;
	trace_instr ( e.begin_location, true );
	instr.immedB = L;
	mexas::jump_list_delete ( mexas::jumps );
	min::pop ( mexas::variables,
	           variables->length - e.stack_limit
		                     + e.nvars );
	-- L;
    }
    else if ( instr.op_code == mex::ENDL )
    {
	instr.immedA = mexas::variables->length
	             - e.stack_limit + tvars;
	instr.immedB = e.nvars;
        instr.immedC = mexas::output_module->length
	             - e.begin_location - 1;
	-- mexas::depth[L];
	min::pop ( mexas::variables,
	           variables->length - e.stack_limit
		                     + e.nvars );
	mexas::jump_list_update ( mexas::jumps );
    }
    else // if mex::END
    {
	instr.immedA = mexas::variables->length
	             - e.stack_limit + tvars;
	-- mexas::depth[L];
	min::pop ( mexas::variables,
	           variables->length - e.stack_limit );
	mexas::jump_list_update ( mexas::jumps );
    }

    min::uns32 len = mexas::blocks->length;
    mexas::stack_limit =
        ( len == 0 ? 0 : 
	  (mexas::blocks+(len-1))->stack_limit );
    mexas::push_instr ( instr, pp, trace_info );

    return 0;
}

void mexas::cont ( mex::instr & instr,
		   min::uns32 tvars,
	           min::gen trace_info,
                   const min::phrase_position & pp )
{
    if ( mexas::blocks->length == 0 )
    {
	mexas::compile_error
	    ( pp, "not in a BEGL ... ENDL block;"
	          " instruction ignored" );
	return;
    }
    min::ptr<mexas::block_element> bp =
        mexas::blocks + ( mexas::blocks->length - 1 );
    if ( bp->begin_op_code != mex::BEGL )
    {
	mexas::compile_error
	    ( pp, "not in a BEGL ... ENDL block;"
	          " instruction ignored" );
	return;
    }

    instr.immedA = mexas::variables->length
		 - bp->stack_limit + tvars;
    instr.immedB = bp->nvars;
    instr.immedC = mexas::output_module->length
		 - bp->begin_location - 1;

    mexas::push_instr ( instr, pp, trace_info );
}

void mexas::trace_instr
	( min::uns32 location, bool no_lines )
{
    min::uns8 trace_flags = mexas::compile_trace_flags;
    min::printer printer =
	mexas::input_file->printer;
    mex::module m = mexas::output_module;

    if ( ( trace_flags & mexas::TRACE ) == 0 )
	return;

    min::phrase_position pp = m->position[location];
    if ( ( trace_flags & mexas::TRACE_LINES )
         &&
	 ! no_lines )
	min::print_phrase_lines
	    ( printer, mexas::input_file, pp );

    printer << min::bol << "    " << min::bom;
    mex::instr instr = m[location];
    printer
	<< "[" << pp.end.line
	<< ":" << location
	<< ";" << variables->length
	<< "] "
	<< mex::op_infos[instr.op_code].name
        << " T_"
        << mex::trace_class_infos
	       [instr.trace_class].name;
    if ( instr.trace_depth != 0 )
        printer
	    << '[' << instr.trace_depth << ']';
    printer
        << " " << instr.immedA
	<< " " << instr.immedB
	<< " " << instr.immedC
	<< " " << instr.immedD
	<< "; "
	<< m->trace_info[location]
	<< min::eom;
}

void mexas::push_push_instr
        ( min::gen new_name, min::gen name,
	  const min::phrase_position & pp )
{
    mex::instr instr =
	{ 0, 0, 0, 0, 0, 0, 0, min::MISSING() };
    min::gen labbuf[3] = { name };
    min::locatable_gen trace_info;

    min::uns32 i = search ( name, SP );
    if ( i != mexas::NOT_FOUND )
    {
	min::uns32 k = L;
	while ( i < mexas::lp[k] ) -- k;
	if ( k == L )
	{
	    instr.op_code = mex::PUSHS;
	    instr.trace_class = mex::T_PUSH;
	    instr.immedA = SP - i - 1;
	}
	else if ( i >= mexas::fp[k] )
	{
	    instr.op_code = mex::PUSHL;
	    instr.trace_class = mex::T_PUSH;
	    instr.immedA = i - mexas::fp[k];
	    instr.immedB = k;
	}
	else
	{
	    instr.op_code = mex::PUSHA;
	    instr.trace_class = mex::T_PUSH;
	    instr.immedA = mexas::fp[k] - i;
	    instr.immedB = k;
	}
	labbuf[1] = new_name;
	trace_info = new_lab_gen ( labbuf, 2 );
    }
    else
    {
	mex::module gm;
	i = mexas::global_search
	    ( gm, mexas::star, mexas::V, name );
	if ( i != mexas::NOT_FOUND )
	{
	    instr.immedA = i;
	    instr.immedD = min::new_stub_gen ( gm );
	    instr.op_code = mex::PUSHG;
	    instr.trace_class = mex::T_PUSH;

	    labbuf[1] = gm->name;
	    labbuf[2] = new_name;
	    trace_info = new_lab_gen ( labbuf, 3 );
	}
	else
	{
	    mexas::compile_error
		( pp, "variable named ",
		      min::pgen ( name ),
		      " not defined; instruction"
		      " changed to PUSHI missing"
		      " value" );
	    instr.op_code = mex::PUSHI;
	    instr.trace_class = mex::T_PUSH;
	    trace_info = min::MISSING();
	}
    }
    mexas::push_instr ( instr, pp, trace_info );
}

min::uns32 mexas::get_trace_info
    ( min::ref<min::gen> trace_info,
      min::uns32 & i,
      const min::phrase_position & pp )
{
    trace_info = mexas::get_trace_info ( i );
    if ( trace_info == min::MISSING() )
        return 0;
    min::lab_ptr lp ( trace_info );
    MIN_ASSERT ( lp != min::NULL_STUB,
                 "bad trace_info" );
    min::uns32 len = min::lablen ( lp );
    for ( min::uns32 j = 1; j < len; ++ j )
    {
        min::gen n = lp[j];
	if ( mexas::is_name ( n ) )
	{
	    mexas::push_push_instr
	        ( mexas::star, n, pp );
	}
	else
	{
	    mexas::compile_error
		( pp, "not a name: ",
		      min::pgen ( n ),
		      "; PUSHI missing value output" );
	    mex::instr instr =
		{ mex::PUSHI, 0, 0, 0, 0, 0, 0,
		              min::MISSING() };
	    mexas::push_instr ( instr, pp );
	}
	mexas::trace_instr
	    ( mexas::output_module->length - 1,
	      true );
    }
    return len - 1;
}


// Scanner Function
// ------- --------

static void scan_error
        ( const char * message,
	  const char * header = "ERROR" )
{
    mexas::input_file->printer
        << min::bol << header << ": " << min::bom
	<< "line " << mexas::last_line_number + 1
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
		if ( p == endp )
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
	    {
	        min::push ( statement ) =
		    ( type == '\'' ?
		      mexas::single_quote :
		      mexas::double_quote );
		min::push ( statement ) =
		    min::new_str_gen ( work );
	    }
	    else
	    {
	        char * endptr;
		double val = std::strtod
		    ( work, & endptr );
		if ( * endptr == 0 )
		    min::push ( statement ) =
			min::new_num_gen ( val );
		else
		    min::push ( statement ) =
			min::new_str_gen ( work );
	    }
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

// Compile Function
// ------- --------

mex::module mexas::compile ( min::file file )
{
    mexas::error_count = 0;
    mexas::warning_count = 0;

    min::pop ( mexas::variables,
               mexas::variables->length );
    min::pop ( mexas::functions,
               mexas::functions->length );
    min::pop ( mexas::blocks,
               mexas::blocks->length );
    mexas::stack_limit = 0;
    min::pop ( mexas::jumps,
               mexas::jumps->length );
    mexas::jump_element e =
        { min::MISSING(), 0, 0, 0, 0, 0, 0 };
    min::push ( jumps ) = e;  // Free head.
    min::push ( jumps ) = e;  // Active head.

    L = 0;
    mexas::depth[0] = 0;
    mexas::lp[0] = 0;
    mexas::fp[0] = 0;

    mexas::input_file = file;

    mexas::output_module = (mex::module_ins)
        mex::create_module ( file );
    mex::module_ins m = mexas::output_module;

    while ( next_statement() )
    {
	mex::instr instr =
	    { 0, 0, 0, 0, 0, 0, 0, min::MISSING() };
        min::phrase_position pp =
	    { { mexas::first_line_number, 0 },
	      { mexas::last_line_number + 1, 0 } };
	min::uns32 index = 1;
	    // statement[index] is next lexeme

        min::gen v = min::get
	    ( mexas::op_code_table,
	      mexas::statement[0] );
	if ( v == min::NONE() )
	{
	    mexas::compile_error
	        ( pp, "undefined operation code"
		      " or declaration name;"
		      " statement ignored" );
	    continue;
	}
	min::uns32 op_code =
	    (min::uns32) min::int_of ( v );

        min::uns8 op_type = mex::NONA;
	if ( op_code < mex::NUMBER_OF_OP_CODES )
	{
	    op_type = mex::op_infos[op_code].op_type;
	    instr.op_code = op_code;
	    instr.trace_class =
	        mex::op_infos[op_code].trace_class;
	}

	switch ( op_type )
	{
	case mex::NONA:
	    goto NON_ARITHMETIC;
	case mex::A2:
	case mex::A2R:
	    if ( SP < mexas::stack_limit + 2 )
	        goto STACK_TOO_SHORT;
	    min::pop ( variables, 2 );
	    goto ARITHMETIC;

	case mex::A2I:
	case mex::A2RI:
	{
	    instr.immedD = mexas::get_num ( index );
	    if ( instr.immedD == min::NONE() )
	    {
		mexas::compile_error
		    ( pp, "no immediate value given;"
		          " zero assumed" );
		instr.immedD == min::new_num_gen ( 0 );
	    }
	    // Fall through
	}
	case mex::A1:
	    if ( SP < mexas::stack_limit + 1 )
	        goto STACK_TOO_SHORT;
	    if ( op_code == mex::POWI )
	    {
	        min::gen en = mexas::get_num ( index );
		if ( en == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no exponent parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::float64 ef =
		    min::direct_float_of ( en );
		if ( ef < 0
		     ||
		     ef >= (1ul << 32 )
		     ||
		     ef != (min::uns32) ef )
		{
		    mexas::compile_error
			( pp, "bad exponent parameter;"
			      " instruction ignored" );
		    continue;
		}
		instr.immedA = (min::uns32) ef;
	    }
	    min::pop ( variables );
	    goto ARITHMETIC;

	case mex::J2:
	    if ( SP < mexas::stack_limit + 2 )
	        goto STACK_TOO_SHORT;
	    min::pop ( variables, 2 );
	    // Fall through.
	case mex::J:
	    goto JUMP;
	}

	STACK_TOO_SHORT:
	{
	    mexas::compile_error
	        ( pp, "portion of stack in the current"
		      " block is too little to pop"
		      " required arguments; instruction"
		      " ignored" );
	    continue;
	}
	ARITHMETIC:
	{
	    min::gen name = mexas::get_name ( index );
	    if ( name == min::NONE() )
	    {
	        mexas::get_star ( index );
	        name = mexas::star;
	    }
	    else
		check_new_name ( name, pp );
	    mexas::push_variable
	        ( mexas::variables, name,
	          L, mexas::depth[L] );
	    mexas::push_instr ( instr, pp, name );
	    goto TRACE;
	}
	JUMP:
	{
	    min::gen target = mexas::get_name ( index );
	    if ( target == min::NONE() )
	    {
		mexas::compile_error
		    ( pp, "jmp... does not have a "
		          " jmp-target that is a name"
			  " instruction will fail" );
		    // instr fails because immedC = 0
		target = min::MISSING();
		    // For trace_info
	    }
	    else
	    {
		mexas::jump_element je =
		    { target,
		      m->length,
		      L,
		      depth[L],
		      depth[L],
		      SP,
		      SP };
		mexas::push_jump ( mexas::jumps, je );
	    }
	    mexas::push_instr ( instr, pp, target );
	    goto TRACE;
	}
	NON_ARITHMETIC:
	{
	    switch ( op_code )
	    {
	    case ::PUSH:
	    case ::PUSHM:
	    case mex::PUSHG:
	    {
	        min::gen mod_name;
		if ( op_code == mex::PUSHG )
		{
		    min::gen mod_name =
			mexas::get_name ( index );
		    if ( mod_name == min::NONE() )
		        mod_name = mexas::get_star
			    ( index );
		    if ( mod_name == min::NONE() )
		    {
			mexas::compile_error
			    ( pp, "no module name:"
				  " instruction"
				  " ignored" );
			continue;
		    }
		}
		min::gen name =
		    mexas::get_name ( index );
		if ( name == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no variable name:"
			      " instruction ignored" );
		    continue;
		}
		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		switch ( op_code )
		{
		case ::PUSH:
		{
		    mexas::push_push_instr
		        ( new_name, name, pp );
		    break;
		}
		case ::PUSHM:
		{
		    min::uns32 limit =
			( L == 0 ? mexas::stack_limit :
				   mexas::lp[1] );
		    min::uns32 j =
		        search ( name, limit );
		    if ( j == mexas::NOT_FOUND )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " not globally"
				  " defined;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.op_code = mex::PUSHL;
		    instr.trace_class = mex::T_PUSH;
		    instr.immedA = j;
		    instr.immedB = 0;

		    min::gen labbuf[2] =
		        { new_name, name };
		    min::locatable_gen trace_info
			( min::new_lab_gen
			      ( labbuf, 2 ) );
		    mexas::push_instr
			( instr, pp, trace_info );
		    break;
		}
		case mex::PUSHG:
		{
		    mex::module gm;
		    min::uns32 index =
		        mexas::global_search
			    ( gm, mod_name,
			      mexas::V, name );
		    if ( index == mexas::NOT_FOUND )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " in module named ",
				  min::pgen
				      ( mod_name ),
				  " not defined;"
				  " instruction"
				  " ignored" );
			continue;
		    }

		    instr.immedA = index;
		    instr.immedD =
		        min::new_stub_gen ( gm );

		    min::gen labbuf[3] =
		        { new_name, gm->name, name };
		    min::locatable_gen trace_info
			( min::new_lab_gen
			      ( labbuf, 3 ) );
		    mexas::push_instr
			( instr, pp, trace_info );

		    break;
		}
		}

		mexas::push_variable
		    ( mexas::variables, new_name,
		      L, mexas::depth[L] );
		goto TRACE;
	    }
	    case mex::PUSHI:
	    {
	        min::gen D = mexas::get_num ( index );
		if ( D == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no number to push:"
			      " instruction ignored" );
		    continue;
		}
		instr.immedD = D;

		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		mexas::push_instr
		    ( instr, pp, new_name );
		mexas::push_variable
		    ( mexas::variables, new_name,
		      L, mexas::depth[L] );
		goto TRACE;
	    }
	    case ::POP:
	    {
		min::gen name =
		    mexas::get_name ( index );
		if ( name == min::NONE() )
		    name = mexas::get_star ( index );
		if ( name == min::NONE() )
		    name = mexas::star;
		if ( name != mexas::star )
		{
		    min::uns32 j = search ( name, SP );
		    if ( j == mexas::NOT_FOUND )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " not defined within"
				  " module; instruction"
				  " ignored" );
			continue;
		    }
		    if ( j < mexas::fp[L] )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " is of lower than"
				  " current lexical"
				  " level, or is"
				  " argument to current"
				  " lexical level, and"
				  " as such cannot"
				  " legally be written;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.immedA = SP - j - 1;
		}
		else
		    instr.immedA = 0;
		instr.op_code = mex::POPS;
		instr.trace_class = mex::T_POP;

		min::gen old_name =
		    ( mexas::variables
		      +
		      ( mexas::variables->length - 1 ) )
		    ->name;
		min::gen labbuf[2] = { old_name, name };
		min::locatable_gen trace_info
		    ( min::new_lab_gen ( labbuf, 2 ) );

		mexas::push_instr
		    ( instr, pp, trace_info );
		min::pop ( mexas::variables );
		goto TRACE;
	    }
	    case ::LABEL:
	    {
		if ( mexas::compile_trace_flags
		     &
		     mexas::TRACE_LINES )
		    min::print_phrase_lines
			( mexas::input_file->printer,
			  mexas::input_file, pp );

		min::gen target =
		    mexas::get_name ( index );
		if ( target == min::NONE() )
		{
		    mexas::compile_error
			( pp, "label does not have a "
			      " jmp-target that is a"
			      " name; declaraton"
			      " ignored" );
		    continue;
		}
		mexas::jump_list_resolve
		    ( mexas::jumps, target );
		continue;
	    }
	    case ::STACKS:
	    {
		min::printer printer =
		    mexas::input_file->printer;
		if (   compile_trace_flags
		     & mexas::TRACE_LINES )
		{
		    min::phrase_position spp = pp;
		    -- spp.end.line;
		    if (   spp.end.line
		         > spp.begin.line )
			min::print_phrase_lines
			    ( printer,
			      mexas::input_file,
			      spp );
		}

		printer << min::bol << "    STACKS: "
		        << min::bom;

		printer << "VARIABLES: " << min::bom;
		min::uns32 level = L;
		min::uns32 depth = mexas::depth[L];
		for ( min::uns32 i = variables->length;
		      0 < i; )
		{
		    -- i;
		    mexas::variable_element v =
		        variables[i];
		    while ( v.level < level )
		    {
		        printer << "| ";
			-- level;
			depth = mexas::depth[level];
		    }
		    while ( v.depth < depth )
		    {
		        printer << "; ";
			-- depth;
		    }
		    printer << v.name;
		    if ( i > 0 ) printer << " ";
		}
		printer << min::eom;

		if ( functions->length == 0 )
		{
		    printer << min::eom;
		    continue;
		}

		printer << min::indent << "FUNCTIONS: "
		        << min::bom;
		level = L;
		depth = mexas::depth[L];
		for ( min::uns32 i = functions->length;
		      0 < i; )
		{
		    -- i;
		    mexas::function_element f =
		        functions[i];
		    while ( f.level < level )
		    {
		        printer << "| ";
			-- level;
			depth = mexas::depth[level];
		    }
		    while ( f.depth < depth )
		    {
		        printer << "; ";
			-- depth;
		    }
		    printer << f.name;
		    if ( i > 0 ) printer << " ";
		}
		printer << min::eom;

		printer << min::eom;
		continue;
	    }
	    case mex::BEG:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::begx
		    ( instr, 0, tvars, trace_info, pp );
		goto TRACE;
	    }
	    case mex::END:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::endx
		    ( instr, tvars, trace_info, pp );
		goto TRACE;
	    }
	    case mex::BEGL:
	    {
	        min::gen nn = mexas::get_num ( index );
		if ( nn == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no nnext parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::float64 nf =
		    min::direct_float_of ( nn );
		if ( nf < 0
		     ||
		     nf >= (1ul << 32 )
		     ||
		     nf != (min::uns32) nf )
		{
		    mexas::compile_error
			( pp, "bad nnext parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 nnext = (min::uns32) nf;

		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::begx
		    ( instr, nnext, tvars, trace_info,
		             pp );
		goto TRACE;
	    }
	    case mex::ENDL:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::endx
		    ( instr, tvars, trace_info, pp );
		goto TRACE;
	    }
	    case mex::CONT:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::cont
		    ( instr, tvars, trace_info, pp );
		goto TRACE;
	    }
	    case mex::BEGF:
	    {
		min::gen function_name =
		    mexas::get_name ( index );
		if ( function_name == min::NONE() )
		{
		    mexas::compile_error
			( pp, "BEGF has no function"
			      " name; instruction"
			      " ignored" );
		    continue;
		}
		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nargs = index - first;
		min::gen message =
		    mexas::get_str ( index );
		if ( message == min::NONE() )
		    message = function_name;

		min::gen labbuf[nargs + 1];
		labbuf[0] = message;
		for ( min::uns32 i = 0; i < nargs;
		                        ++ i )
		    labbuf[i+1] = statement[first+i];
		min::locatable_gen trace_info
		    ( min::new_lab_gen
		          ( labbuf, nargs+1 ) );

		mexas::push_function
		    ( mexas::functions,
		      function_name,
		      L, mexas::depth[L],
		      m->length );

		mexas::begx
		    ( instr, nargs, 0,
		      trace_info, pp );
		for ( min::uns32 i = 0; i < nargs;
		                        ++ i )
		    mexas::push_variable
			( mexas::variables,
			  statement[first+i],
			  L, mexas::depth[L] );
		goto TRACE;
	    }
	    case mex::ENDF:
	    {
		if ( mexas::compile_trace_flags
		     &
		     mexas::TRACE_LINES )
		    min::print_phrase_lines
			( mexas::input_file->printer,
			  mexas::input_file, pp );
		mexas::endx
		    ( instr, 0, min::MISSING(), pp );
		trace_instr ( m->length - 1, true );
		continue;
	    }
	    case mex::RET:
	    {
	        if ( L == 0 )
		{
		    mexas::compile_error
			( pp, "RET not in a function;"
			      " instruction ignored" );
		    continue;
		}
		min::gen nn = mexas::get_num ( index );
		if ( nn == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no nresults parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::float64 nf =
		    min::direct_float_of ( nn );
		if ( nf < 0
		     ||
		     nf >= (1ul << 32 )
		     ||
		     nf != (min::uns32) nf )
		{
		    mexas::compile_error
			( pp, "bad nresults parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 nresults = (min::uns32) nf;
		instr.immedB = L;
		instr.immedC = nresults;
		mexas::push_instr ( instr, pp );
		min::pop ( mexas::variables, nresults );
		break;
	    }
	    case ::CALL:
	    case mex::CALLM:
	    case mex::CALLG:
	    {
		mex::module cm = m;
		min::uns32 begf_location =
		    mexas::NOT_FOUND;
		min::gen function_name;

		if ( op_code == mex::CALLG )
		{
		    min::gen mod_name =
		        mexas::get_name ( index );
		    if ( mod_name == min::NONE() )
		        mod_name =
			    mexas::get_star ( index );
		    if ( mod_name == min::NONE() )
		    {
			mexas::compile_error
			    ( pp, "no module name for"
			          " CALLG;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    function_name =
		        mexas::get_name ( index );
		    if ( function_name == min::NONE() )
		    {
			mexas::compile_error
			    ( pp, "no function name for"
			          " CALLG;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    begf_location = mexas::global_search
		        ( cm, mod_name, mexas::F,
			      function_name );
		    if (    begf_location
		         == mexas::NOT_FOUND )
		    {
			mexas::compile_error
			    ( pp, "function ",
			          min::pgen
				      ( function_name ),
				  " in module ",
			          min::pgen
				      ( mod_name ),
			          " not found;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.immedD =
		        min::new_stub_gen ( cm );
		}
		else
		{
		    function_name =
		        mexas::get_name ( index );
		    if ( function_name == min::NONE() )
		    {
			mexas::compile_error
			    ( pp, "no function name for"
			          " CALL/CALLM;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    min::uns32 i =
		        mexas::functions->length;
		    while ( 0 < i )
		    {
		        -- i;
			min::ptr
			    <mexas::function_element>
			    fptr =
			    (mexas::functions + i );
			if (    fptr->name
			     == function_name )
			{
			    begf_location =
			        fptr->index;
			    break;
			}
		    }
		    if (    begf_location
			 != mexas::NOT_FOUND )
		    {
		        instr.op_code = mex::CALLM;
			instr.trace_class =
			    mex::T_CALLM;
		    }
		    else if ( op_code == ::CALL )
		    {
		        instr.op_code = mex::CALLG;
			instr.trace_class =
			    mex::T_CALLG;
			begf_location =
			    mexas::global_search
				( cm, mexas::star,
				      mexas::F,
				      function_name );
			instr.immedD =
			    min::new_stub_gen ( cm );
		    }
		    if (    begf_location
		         == mexas::NOT_FOUND )
		    {
			mexas::compile_error
			    ( pp, "function ",
			          min::pgen
				      ( function_name ),
			          " not found;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		}

		if ( begf_location >= cm->length )
		{
		    mexas::compile_error
			( pp, "bad function_element;"
			      " begf_location >="
			      " cm->length;"
			      " instruction ignored" );
		    continue;
		}
		min::ptr<mex::instr> ip =
		    cm + begf_location;
		if ( ip->op_code != mex::BEGF )
		{
		    mexas::compile_error
			( pp, "bad function_element;"
			      " does not point at"
			      " BEGF;"
			      " instruction ignored" );
		    continue;
		}

	        min::gen na = mexas::get_num ( index );
		if ( na == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no nargs parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::float64 nf =
		    min::direct_float_of ( na );
		if ( nf < 0
		     ||
		     nf >= (1ul << 32 )
		     ||
		     nf != (min::uns32) nf )
		{
		    mexas::compile_error
			( pp, "bad nargs parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 nargs = (min::uns32) nf;

		if ( ip->immedA > nargs )
		{
		    mexas::compile_error
			( pp, "BEGF expects ",
			      min::puns
			         ( ip->immedA,
				   "%u" ),
			      " arguments but CALL..."
			      " provides only ",
			      min::puns
			         ( nargs, "%u" ),
			      ";"
			      " instruction ignored" );
		    continue;
		}

		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nresults = index - first;
		min::gen message =
		    mexas::get_str ( index );
		if ( message == min::NONE() )
		    message = function_name;

		min::gen labbuf[nresults + 1];
		labbuf[0] = message;
		for ( min::uns32 i = 0; i < nresults;
		                        ++ i )
		    labbuf[i+1] = statement[first+i];
		min::locatable_gen trace_info
		    ( min::new_lab_gen
		          ( labbuf, nresults+1 ) );
		
		instr.immedA = nargs;
		instr.immedB = nresults;
		instr.immedC = begf_location;
		mexas::push_instr
		    ( instr, pp, trace_info );

		min::pop ( mexas::variables, nargs );
		for ( min::uns32 i = 0; i < nresults;
		                        ++ i )
		    mexas::push_variable
			( mexas::variables,
			  statement[first+i],
			  L, mexas::depth[L] );
		break;
	    }
	    }
	}
	TRACE:
	{
	    trace_instr ( m->length - 1 );
	    continue;
	}

    }

    mexas::jump_list_delete ( mexas::jumps );

    if ( mexas::error_count > 0 )
        return min::NULL_STUB;

    if (    mexas::input_file->file_name
         == min::MISSING() )
        mex::name_ref(m) = min::MISSING();
    else
    {
        min::str_ptr sp
	    ( mexas::input_file->file_name );
	const char * p =
	    min::unprotected::str_of ( sp );
	min::uns32 len = std::strlen ( p );
	if ( len > 4
	     &&
	     std::strcmp ( ".mex", p + len - 4 ) == 0 )
	    mex::name_ref(m) =
	        min::new_str_gen ( p, len - 1 );
	else
	    mex::name_ref(m) =
	        mexas::input_file->file_name;
    }

    mexas::make_module_interface();

    min::locatable_var<mex::process> process
        ( mex::init_process ( m ) );
    process->trace_flags = mexas::run_trace_flags;
    process->limit = mex::run_counter_limit;
    mex::run_process ( process );

    min::push ( mexas::modules ) = m;

    return m;
}
