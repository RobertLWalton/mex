// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri May 24 04:25:07 EDT 2024
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
# include <cmath>
# include <cfenv>

# define L mexstack::lexical_level
# define SP mexstack::var_stack_length

# define MUP min::unprotected

mexcom::print mexcom::print_switch = mexcom::NO_PRINT;
bool mexcom::trace_never = false;

min::uns32 mexcom::error_count;
min::uns32 mexcom::warning_count;

min::locatable_var<min::file> mexcom::input_file;
min::locatable_var<mex::module_ins>
    mexcom::output_module;

min::uns32 mexstack::var_stack_length = 0;
min::uns32 mexstack::func_stack_length = 0;
min::uns32 mexstack::func_var_stack_length = 0;
void mexstack::pop_stacks ( void )
{
    MIN_REQUIRE (    mexas::variables->length
                  >= mexstack::var_stack_length );
    min::pop ( mexas::variables,
    	         mexas::variables->length
	       - mexstack::var_stack_length );

    MIN_REQUIRE
        ( mexstack::func_var_stack_length == 0 );

    MIN_REQUIRE (    mexas::functions->length
                  >= mexstack::func_stack_length );
    min::pop ( mexas::functions,
    	         mexas::functions->length
	       - mexstack::func_stack_length );
}

min::uns8 mexstack::lexical_level;
min::uns8 mexstack::depth[mex::max_lexical_level+1];
min::uns32 mexstack::lp[mex::max_lexical_level+1];
min::uns32 mexstack::fp[mex::max_lexical_level+1];

min::locatable_gen mexas::star;
min::locatable_gen mexas::V;
min::locatable_gen mexas::F;

min::locatable_gen mexcom::op_code_table;
min::locatable_gen mexcom::trace_class_table;
min::locatable_gen mexcom::trace_flag_table;
min::locatable_gen mexcom::except_flag_table;

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

enum op_code
    // Extends mex::op_code to include pseudo-ops and
    // declarations.
{
    PUSHM = mex::NUMBER_OF_OP_CODES,
    PUSH,
    POP,
    CALL,
    LABEL,
    TEST_INSTRUCTION,
    STACKS,
    NUMBER_OF_OP_CODES
};

static struct extended_op
  { const char * name; ::op_code op_code; }
    extended_ops[] = {
    { "PUSHM", ::PUSHM },
    { "PUSH", ::PUSH },
    { "POP", ::POP },
    { "CALL", ::CALL },
    { "LABEL", ::LABEL },
    { "TEST_INSTRUCTION", ::TEST_INSTRUCTION },
    { "STACKS", ::STACKS }
    };

static void init_op_code_table ( void )
{
    mexcom::init_op_code_table();

    min::obj_vec_insptr vp ( mexcom::op_code_table );
    min::attr_insptr ap ( vp );

    extended_op * p = ::extended_ops;
    extended_op * endp =
        p + (   ::NUMBER_OF_OP_CODES
	      - mex::NUMBER_OF_OP_CODES );
    min::locatable_gen tmp;
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

static void init_except_flag_table ( void )
{
    min::uns32 n = mex::NUMBER_OF_EXCEPTS + 20;
    mexcom::except_flag_table = min::new_obj_gen
        ( 10 * n, 4 * n, 1 * n );

    min::obj_vec_insptr vp
        ( mexcom::except_flag_table );
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
static min::locatable_gen on;
static min::locatable_gen off;
static min::locatable_gen loop;

static void initialize ( void )
{
    mexas::star = min::new_str_gen ( "*" );
    mexas::V = min::new_str_gen ( "V" );
    mexas::F = min::new_str_gen ( "F" );
    mexas::single_quote = min::new_str_gen ( "'" );
    mexas::double_quote = min::new_str_gen ( "\"" );
    ::backslash = min::new_str_gen ( "\\" );
    ::on = min::new_str_gen ( "ON" );
    ::off = min::new_str_gen ( "OFF" );
    ::loop = min::new_str_gen ( "LOOP" );

    ::init_op_code_table();
    ::init_trace_class_table();
    ::init_trace_flag_table();
    ::init_except_flag_table();

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
    ::print_error_or_warning
        ( "ERROR", pp,
	      message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexcom::error_count;
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
    ::print_error_or_warning
        ( "WARNING", pp,
	      message1, message2, message3,
	      message4, message5, message6,
	      message7, message8, message9 );
    ++ mexcom::warning_count;
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
        m = mexas::modules[i];
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

// Find a module given an module_name.  Searches most
// recently compiled first.  Returns the module, or
// NULL_STUB if none found.
//
static mex::module module_search
	( min::gen module_name )
{
    for ( min::uns32 i = mexas::modules->length;
          0 < i; )
    {
	-- i;
        if ( mexas::modules[i]->name == module_name )
	    return mexas::modules[i];
    }
    return min::NULL_STUB;
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

    mex::interface_ref ( mexcom::output_module ) =
        interface;
}

unsigned mexas::jump_list_delete
	( mexas::jump_list jlist )
{
    min::ptr<mexas::jump_element> free = jlist + 0;
    min::ptr<mexas::jump_element> previous = jlist + 1;

    mex::module_ins m = mexcom::output_module;

    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexas::jump_element> next = jlist + n;
	if (   next->lexical_level
	     < mexstack::lexical_level )
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
	if ( next->maximum_depth > mexstack::depth[L] )
	    next->maximum_depth = mexstack::depth[L];
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

    mex::module_ins m = mexcom::output_module;
    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexas::jump_element> next = jlist + n;
	if ( next->lexical_level < L )
	    break;
	if ( target_name == next->target_name
	     &&
	     next->maximum_depth >= mexstack::depth[L] )
	{
	    min::uns32 depth_diff =
	        next->depth - mexstack::depth[L];
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

		mexcom::trace_instr
		    ( next->jmp_location,
		      mexstack::var_stack_length,
		      true );
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
	  mexstack::var_stack_length + nvars,
	  mexas::functions->length,
	  nvars,
	  mexcom::output_module->length };

    if ( instr.op_code == mex::BEGF )
    {
        MIN_ASSERT ( mexstack::lexical_level
	             <
		     mex::max_lexical_level,
		     "mex::max_lexical_level"
		     " exceeded" );
	MIN_ASSERT ( tvars == 0,
	             "BEGF has trace variables" );
	e.end_op_code = mex::ENDF;

	++ L;
	mexstack::depth[L] = 0;
	mexstack::lp[L] = mexstack::var_stack_length;
	mexstack::fp[L] = mexstack::lp[L] + nvars;

	instr.immedA = nvars;
	instr.immedB = L;
    }
    else if ( instr.op_code == mex::BEGL )
    {
	e.end_op_code = mex::ENDL;
	e.stack_limit= mexstack::var_stack_length;

        ++ mexstack::depth[L];
	instr.immedA = tvars;
	instr.immedB = nvars;
    }
    else if ( instr.op_code == mex::BEG )
    {
	MIN_ASSERT ( nvars == 0,
	             "BEG has non-zero nvars" );
	instr.immedA = tvars;
	e.end_op_code = mex::END;
        ++ mexstack::depth[L];
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
		    mexcom::op_code_table;
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
    mexstack::func_stack_length = e.function_stack;
 
    if ( instr.op_code == mex::ENDF )
    {
	min::ptr<mex::instr> ip =
	    mexcom::output_module + e.begin_location;
        ip->immedC = mexcom::output_module->length + 1
	           - e.begin_location;
	mexcom::trace_instr
	    ( e.begin_location,
	      mexstack::var_stack_length,
	      true );
	instr.immedB = L;
	mexas::jump_list_delete ( mexas::jumps );
	mexstack::var_stack_length =
		e.stack_limit - e.nvars;
	-- L;
    }
    else if ( instr.op_code == mex::ENDL )
    {
	instr.immedA = mexstack::var_stack_length
	             - e.stack_limit + tvars;
	instr.immedB = e.nvars;
        instr.immedC = mexcom::output_module->length
	             - e.begin_location - 1;
	-- mexstack::depth[L];
	mexstack::var_stack_length =
		e.stack_limit - e.nvars;
	mexas::jump_list_update ( mexas::jumps );
    }
    else // if mex::END
    {
	instr.immedA = mexstack::var_stack_length
	             - e.stack_limit + tvars;
	-- mexstack::depth[L];
	mexstack::var_stack_length =
		e.stack_limit;
	mexas::jump_list_update ( mexas::jumps );
    }
    mexstack::pop_stacks();

    min::uns32 len = mexas::blocks->length;
    mexas::stack_limit =
        ( len == 0 ? 0 : 
	  (mexas::blocks+(len-1))->stack_limit );
    mexas::push_instr ( instr, pp, trace_info );

    MIN_REQUIRE ( mexas::variables->length
                  == mexstack::var_stack_length );
    MIN_REQUIRE ( mexas::functions->length
                  == mexstack::func_stack_length );

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

    instr.immedA = mexstack::var_stack_length
		 - bp->stack_limit + tvars;
    instr.immedB = bp->nvars;
    instr.immedC = mexcom::output_module->length
		 - bp->begin_location - 1;

    mexas::push_instr ( instr, pp, trace_info );
}

void mexcom::trace_instr
	( min::uns32 location,
	  min::uns32 stack_length,
	  bool no_source )
{
    mexcom::print print = mexcom::print_switch;
    min::printer printer =
	mexcom::input_file->printer;
    mex::module m = mexcom::output_module;

    if ( print == mexcom::NO_PRINT )
	return;

    min::phrase_position pp = m->position[location];
    if ( print == mexcom::PRINT_WITH_SOURCE
         &&
	 ! no_source )
	min::print_phrase_lines
	    ( printer, mexcom::input_file, pp );

    mex::instr instr = m[location];
    printer
        << min::bol << min::bom <<"    "
	<< "[" << pp.end.line
	<< ":" << location
	<< ";" << stack_length
	<< "] "
	<< min::place_indent ( 0 )
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

void mexstack::push_push_instr
        ( min::gen new_name, min::gen name,
	  min::uns32 index,
	  const min::phrase_position & pp,
	  min::uns32 offset )
{
    mex::instr instr =
	{ 0, 0, 0, 0, 0, 0, 0, min::MISSING() };
    min::gen labbuf[2] = { name, new_name };
    min::locatable_gen trace_info =
    new_lab_gen ( labbuf, 2 );

    min::uns32 k = L;
    while ( index < mexstack::lp[k] ) -- k;
    if ( k == L )
    {
	instr.op_code = mex::PUSHS;
	instr.trace_class = mex::T_PUSH;
	instr.immedA = SP - index - 1 + offset;
    }
    else if ( index >= mexstack::fp[k] )
    {
	instr.op_code = mex::PUSHL;
	instr.trace_class = mex::T_PUSH;
	instr.immedA = index - mexstack::fp[k];
	instr.immedB = k;
    }
    else
    {
	instr.op_code = mex::PUSHA;
	instr.trace_class = mex::T_PUSH;
	instr.immedA = mexstack::fp[k] - index;
	instr.immedB = k;
    }
    mexas::push_instr ( instr, pp, trace_info );
}

void mexas::push_push_instr
        ( min::gen new_name, min::gen name,
	  const min::phrase_position & pp,
	  min::uns32 offset )
{
    min::uns32 i = search ( name, SP );
    if ( i != mexas::NOT_FOUND )
    {
    	mexstack::push_push_instr
	    ( new_name, name, i, pp, offset );
	return;
    }

    mex::instr instr =
	{ 0, 0, 0, 0, 0, 0, 0, min::MISSING() };
    min::gen labbuf[3] = { name };
    min::locatable_gen trace_info;

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
    min::lab_ptr labp ( trace_info );
    MIN_ASSERT ( labp != min::NULL_STUB,
                 "bad trace_info" );
    min::uns32 len = min::lablen ( labp );
    for ( min::uns32 j = 1; j < len; ++ j )
    {
        min::gen n = labp[j];
	if ( mexas::is_name ( n ) )
	{
	    mexas::push_push_instr
	        ( mexas::star, n, pp, j - 1 );
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
	mexcom::trace_instr
	    ( mexcom::output_module->length - 1,
	      mexstack::var_stack_length,
	      true );
    }
    return len - 1;
}

bool mexas::check_parameter
	( min::uns32 & param, min::gen n,
	  const min::phrase_position & pp,
	  const char * pname, bool is_level )
{
    min::float64 nf = MUP::direct_float_of ( n );

    if ( std::isnan ( nf )
	 ||
	 nf != (min::int64) nf )
    {
	mexas::compile_error
	    ( pp, "bad ", min::pnop, pname, min::pnop,
	          " parameter; statement ignored" );
	return false;
    }

    if ( is_level )
    {
        if ( - L < nf && nf <= 0 )
	{
	    param = L + nf;
	    return true;
	}
	else if ( 1 <= nf && nf <= L )
	{
	    param = nf;
	    return true;
	}
    }
    else if ( 0 <= nf && nf < (1ul << 32) )
    {
	param = nf;
	return true;
    }

    mexas::compile_error
	( pp, pname, min::pnop,
	      " parameter out of range;"
	      " statement ignored" );
    return false;
}


// Scanner Function
// ------- --------

static void scan_error
        ( const char * message,
	  const char * header = "ERROR" )
{
    mexcom::input_file->printer
        << min::bol << min::bom << header << ": "
	<< min::place_indent ( 0 )
	<< "line " << mexas::last_line_number + 1
	<< ": " << message << min::eom;
    print_line ( mexcom::input_file->printer,
                 mexcom::input_file,
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
        mexcom::input_file->buffer;

    bool statement_started = false;
        // A statement is started by any non-blank line.
    bool continuation_mark_found = false;
        // I.e., ::backslash.

    while ( true )
    {
        // Process next line.

	if ( ! statement_started )
	    mexas::first_line_number =
	        mexcom::input_file->next_line_number;
	mexas::last_line_number =
	    mexcom::input_file->next_line_number;
	min::uns32 begin_offset =
	    min::next_line ( mexcom::input_file );
	min::uns32 end_offset =
	    mexcom::input_file->next_offset - 1;
	    // Do not include NUL character.
	if ( begin_offset == min::NO_LINE )
	{
	    begin_offset = min::remaining_offset
		( mexcom::input_file );
	    end_offset = begin_offset
	               + min::remaining_length
			     ( mexcom::input_file );
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
	             ( mexcom::input_file );
	}

	statement_started = true;

	char work[end_offset - begin_offset + 10];

	const char * p = ~ min::begin_ptr_of ( buffer );
	const char * endp = p + end_offset;
	p += begin_offset;
	    // buffer + XX is illegal in case of partial
	    // line at end of file and XX == end_offset.
	bool illegal_character_found = false;
	bool lexeme_found = false;

#	define SAVE \
	    begin_offset = end_offset - ( endp - p );
#	define RESTORE \
	    p = ~ min::begin_ptr_of ( buffer ); \
	    endp = p + end_offset; \
	    p += begin_offset;

	while ( true )
	{
	    // Loop through lexemes in a line.

	    // Skip whitespace.
	    //
	    while ( p < endp && std::isspace ( * p ) )
	        ++ p;

	    if ( p >= endp ) goto END_LINE;

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
	   
	    if ( type == 0
		 &&
		 work[0] == '/'
		 &&
		 work[1] == '/' )
	    {
	        // Comment
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
    mexcom::error_count = 0;
    mexcom::warning_count = 0;

    min::pop ( mexas::variables,
               mexas::variables->length );
    mexstack::var_stack_length = 0;
    min::pop ( mexas::functions,
               mexas::functions->length );
    mexstack::func_stack_length = 0;
    min::pop ( mexas::blocks,
               mexas::blocks->length );
    mexas::stack_limit = 0;
    min::pop ( mexas::jumps,
               mexas::jumps->length );
    mexas::jump_element e =
        { min::MISSING(), 0, 0, 0, 0, 0, 0, 0 };
    min::push ( jumps ) = e;  // Free head.
    min::push ( jumps ) = e;  // Active head.

    L = 0;
    mexstack::depth[0] = 0;
    mexstack::lp[0] = 0;
    mexstack::fp[0] = 0;

    mexcom::input_file = file;

    mexcom::output_module = (mex::module_ins)
        mex::create_module ( file );
    mex::module_ins m = mexcom::output_module;

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
	    ( mexcom::op_code_table,
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
	    mexstack::var_stack_length -= 2;
	    goto ARITHMETIC;

	case mex::A2I:
	case mex::A2RI:
	    instr.immedD = mexas::get_num ( index );
	    if ( instr.immedD == min::NONE() )
	    {
		mexas::compile_error
		    ( pp, "no immediate value given;"
		          " zero assumed" );
		instr.immedD == min::new_num_gen ( 0 );
	    }
	    /* FALLTHRU */
	case mex::A1:
	    if ( SP < mexas::stack_limit + 1 )
	        goto STACK_TOO_SHORT;
	    if ( op_code == mex::PUSHV )
	        goto NON_ARITHMETIC;
		    // PUSHV executes as A1 and
		    // compiles mostly as NONA.
	    else if ( op_code == mex::POWI )
	    {
	        min::gen en = mexas::get_num ( index );
		if ( en == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no exponent parameter;"
			      " instruction ignored" );
		    continue;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedA, en,
			      pp, "exponent" ) )
		    continue;
	    }
	    min::pop ( variables );
	    mexstack::var_stack_length -= 1;
	    goto ARITHMETIC;

	case mex::J2:
	    if ( SP < mexas::stack_limit + 2 )
	        goto STACK_TOO_SHORT;
	    min::pop ( variables, 2 );
	    mexstack::var_stack_length -= 2;
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
	          L, mexstack::depth[L] );
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
		      mexstack::depth[L],
		      mexstack::depth[L],
		      SP,
		      SP,
		      0 };
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
		    mod_name =
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
				   mexstack::lp[1] );
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
		      L, mexstack::depth[L] );
		break;
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
		      L, mexstack::depth[L] );
		break;
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
		    mexas::variable_element * ve =
		        ~ ( mexas::variables + j );
		    if ( ve->level < L )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " is of lower than"
				  " current lexical"
				  " level, and"
				  " as such cannot"
				  " legally be written;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    if ( j < mexstack::fp[L] )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " is an argument to"
				  " the current"
				  " function, and"
				  " as such cannot"
				  " legally be written;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    if (    ve->depth
		         == mexstack::depth[L] )
		    {
			mexas::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " has the same depth"
				  " as the POP"
				  " instruction, and"
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
		      ( mexstack::var_stack_length
		        - 1 ) )
		    ->name;
		min::gen labbuf[2] = { old_name, name };
		min::locatable_gen trace_info
		    ( min::new_lab_gen ( labbuf, 2 ) );

		mexas::push_instr
		    ( instr, pp, trace_info );
		min::pop ( mexas::variables );
	        mexstack::var_stack_length -= 1;
		break;
	    }
	    case ::LABEL:
	    {

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

		if ( mexcom::print_switch
		     ==
		     mexcom::PRINT_WITH_SOURCE )
		    min::print_phrase_lines
			( mexcom::input_file->printer,
			  mexcom::input_file, pp );

		mexas::jump_list_resolve
		    ( mexas::jumps, target );
		continue;
	    }
	    case ::STACKS:
	    {
		if ( mexcom::print_switch
		     ==
		     mexcom::NO_PRINT )
		    continue;

		min::printer printer =
		    mexcom::input_file->printer;

		if ( mexcom::print_switch
		     ==
		     mexcom::PRINT_WITH_SOURCE )
		{
		    min::phrase_position spp = pp;
		    -- spp.end.line;
		    if (   spp.end.line
		         > spp.begin.line )
			min::print_phrase_lines
			    ( printer,
			      mexcom::input_file,
			      spp );
		}

		printer << min::bom
		        << "    STACKS: "
		        << min::place_indent ( 0 );

		printer << min::save_indent
                        << "VARIABLES: "
		        << min::place_indent ( 0 );
		min::uns32 level = L;
		min::uns32 depth = mexstack::depth[L];
		for ( min::uns32 i =
		          mexstack::var_stack_length;
		      0 < i; )
		{
		    -- i;
		    mexas::variable_element v =
		        variables[i];
		    while ( v.level < level )
		    {
		        printer << "| ";
			-- level;
			depth = mexstack::depth[level];
		    }
		    while ( v.depth < depth )
		    {
		        printer << "; ";
			-- depth;
		    }
		    printer << v.name;
		    if ( i > 0 ) printer << " ";
		}
		printer << min::restore_indent;

		if ( functions->length == 0 )
		{
		    printer << min::eom;
		    continue;
		}

		printer << min::indent
                        << min::save_indent
		        << "FUNCTIONS: "
		        << min::place_indent ( 0 );
		level = L;
		depth = mexstack::depth[L];
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
			depth = mexstack::depth[level];
		    }
		    while ( f.depth < depth )
		    {
		        printer << "; ";
			-- depth;
		    }
		    printer << f.name;
		    if ( i > 0 ) printer << " ";
		}
		printer << min::restore_indent;

		printer << min::eom;
		continue;
	    }
	    case ::TEST_INSTRUCTION:
	    {
	        min::gen op_code =
		    mexas::get_name ( index );
		if ( op_code == min::NONE() )
		{
		    mexas::compile_error
			( pp, "no op_code;"
			      " statement ignored" );
		    continue;
		}

		op_code = min::get
		    ( mexcom::op_code_table, op_code );
		if ( op_code == min::NONE()
		     ||
		        min::direct_float_of ( op_code )
		     >= mex::NUMBER_OF_OP_CODES )
		{
		    mexas::compile_error
			( pp, "undefined operation"
			      " code; statement"
			      " ignored" );
		    continue;
		}
		instr.op_code =
		    (min::uns32)
		    min::int_of ( op_code );

	        min::gen trace_class =
		    mexas::get_name ( index );
		if ( trace_class == min::NONE() )
		{
		    mexas::push_instr ( instr, pp );
		    break;
		}
		trace_class = min::get
		    ( mexcom::trace_class_table,
		      trace_class );
		if ( trace_class != min::NONE() )
		{
		    min::float64 f =
			min::direct_float_of
			    ( trace_class );
		    instr.trace_class = (min::uns8) f;
		}
		else
		{
		    mexas::compile_error
			( pp, "unrecognized trace"
			      " class; "
			      " statement ignored" );
		    continue;
		}
	        min::gen trace_depth =
		    mexas::get_num ( index );
		if ( trace_depth == min::NONE() )
		{
		    mexas::push_instr ( instr, pp );
		    break;
		}
		min::uns32 depth;
		if ( !  mexas::check_parameter
		            ( depth, trace_depth,
			      pp, "trace_depth" ) )
		    continue;
		if ( depth >= 265 )
		{
		    mexas::compile_error
			( pp, "trace_depth too large;"
			      " statement ignored" );
		    continue;
		}
		instr.trace_depth = (min::uns8) depth;

	        min::gen immedA =
		    mexas::get_num ( index );
		if ( immedA == min::NONE() )
		{
		    mexas::push_instr ( instr, pp );
		    break;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedA, immedA,
			      pp, "immedA" ) )
		    continue;

	        min::gen immedB =
		    mexas::get_num ( index );
		if ( immedB == min::NONE() )
		{
		    mexas::push_instr ( instr, pp );
		    break;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedB, immedB,
			      pp, "immedB" ) )
		    continue;

	        min::gen immedC =
		    mexas::get_num ( index );
		if ( immedC == min::NONE() )
		{
		    mexas::push_instr ( instr, pp );
		    break;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedC, immedC,
			      pp, "immedC" ) )
		    continue;

	        instr.immedD = mexas::get_num ( index );
		if ( instr.immedD == min::NONE() )
		{
		    instr.immedD =
		        mexas::get_name ( index );
		    if ( instr.immedD == min::NONE() )
		    {
			mexas::push_instr ( instr, pp );
			break;
		    }
		    mex::module m = ::module_search
		        ( instr.immedD );
		    if ( m == min::NULL_STUB )
		    {
			mexas::compile_error
			    ( pp, "",
			          min::pgen
				      ( instr.immedD ),
			          " does not name a"
				  " module;"
				  " statement ignored"
			    );
			continue;
		    }
		    instr.immedD =
		        min::new_stub_gen ( m );
		}

		mexas::push_instr ( instr, pp );
		break;
	    }
	    case mex::BEG:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::begx
		    ( instr, 0, tvars, trace_info, pp );
		break;
	    }
	    case mex::END:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexas::endx
		    ( instr, tvars, trace_info, pp );
		break;
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
		min::uns32 nnext;
		if ( !  mexas::check_parameter
		            ( nnext, nn,
			      pp, "nnext" ) )
		    continue;

		if ( nnext > SP - mexas::stack_limit )
		{
		    mexas::compile_error
			( pp, "portion of stack in the"
			      " containing block is"
			      " smaller than the number"
			      " of next-variables" );
		    nnext = SP - mexas::stack_limit;
			// To protect against
			// excessively large nvars
			// values.
		}

		min::gen message =
		    mexas::get_str ( index );
		if ( message == min::NONE() )
		    message = ::loop;

		for ( min::uns32 i = 1;
		      i <= nnext; ++ i )
		{
		    mexas::variable_element * ve =
		        ~ (   mexas::variables
		            + ( SP - i ) );
		    if ( ve->level != L
		         ||
			 ve->depth != mexstack::depth[L]
			 ||
			 ve->name == mexas::star )
		    {
		        nnext = i - 1;
			mexas::compile_error
			    ( pp, "BEGL: not enough"
			          " named variables"
				  " of current lexical"
				  " level and depth"
				  " in the stack;"
				  " nnext reduced to",
				  min::puns
				      ( nnext, "%u" ) );
			break;
		    }
		}

		min::gen labbuf[nnext+1];
		labbuf[0] = message;
		for ( min::uns32 i = 1;
		      i <= nnext; ++ i )
		    labbuf[nnext+1-i] =
			(   mexas::variables
			  + ( SP - i ) )->name;

		min::locatable_gen trace_info
		    ( min::new_lab_gen
		          ( labbuf, nnext + 1 ) );

		for ( min::uns32 i = 0;
		      i < nnext; ++ i )
		{
		    MIN_ASSERT
			(    nnext
			  <= SP - mexas::stack_limit,
			  "BEGL: mexas::stack_limit"
			  " violation"
			);
		    min::locatable_gen name
			( (   mexas::variables
			    + ( SP - nnext ) )->name );
		    MIN_ASSERT
			( name != mexas::star,
			  "BEGL: variable name is *" );

		    min::str_ptr sp ( name );
		    min::unsptr len =
		        min::strlen ( sp );
		    char buffer[len+10];
		    std::strcpy ( buffer, "next-" );
		    min::strcpy ( buffer + 5, sp );
		    name = min::new_str_gen ( buffer );

		    mexas::push_variable
			( mexas::variables, name,
			  L, mexstack::depth[L] );
		}
		mexas::begx
		    ( instr, nnext, 0, trace_info, pp );
		break;
	    }
	    case mex::ENDL:
	    {
		mexas::endx
		    ( instr, 0, min::MISSING(), pp );
		break;
	    }
	    case mex::CONT:
	    {
		mexas::cont
		    ( instr, 0, min::MISSING(), pp );
		break;
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
		      L, mexstack::depth[L],
		      m->length );

		mexas::begx
		    ( instr, nargs, 0,
		      trace_info, pp );
		for ( min::uns32 i = 0; i < nargs;
		                        ++ i )
		    mexas::push_variable
			( mexas::variables,
			  statement[first+i],
			  L, mexstack::depth[L] );
		break;
	    }
	    case mex::ENDF:
	    {
		if ( mexcom::print_switch
		     ==
		     mexcom::PRINT_WITH_SOURCE )
		    min::print_phrase_lines
			( mexcom::input_file->printer,
			  mexcom::input_file, pp );
		mexas::endx
		    ( instr, 0, min::MISSING(), pp );
		mexcom::trace_instr
		    ( m->length - 1,
	              mexstack::var_stack_length,
		      true );
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
		min::uns32 nresults;
		if ( !  mexas::check_parameter
		            ( nresults, nn,
			      pp, "nresults" ) )
		    continue;
		instr.immedB = L;
		instr.immedC = nresults;
		mexas::push_instr ( instr, pp );
		min::pop ( mexas::variables, nresults );
	        mexstack::var_stack_length -= nresults;
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
		min::uns32 nargs;
		if ( !  mexas::check_parameter
		            ( nargs, na,
			      pp, "nargs" ) )
		    continue;

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
			min::NONE()
			||
			mexas::get_star ( index )
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
	        mexstack::var_stack_length -= nargs;
		for ( min::uns32 i = 0; i < nresults;
		                        ++ i )
		    mexas::push_variable
			( mexas::variables,
			  statement[first+i],
			  L, mexstack::depth[L] );
		break;
	    }
	    case mex::SET_TRACE:
	    {
		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nflags = index - first;

		min::uns32 flags = 0;
		for ( min::uns32 i = 0; i < nflags;
		                        ++ i )
		{
		    min::gen fname = statement[first+i];
		    min::gen fbit = min::get
		        ( mexcom::trace_flag_table,
			  fname );
		    if ( fbit != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( fbit );
			flags |= (min::uns32) f;
		    }
		    else
			mexas::compile_error
			    ( pp, "Unrecognized trace"
			          " flag: ",
				  min::pgen ( fname ),
				  "; ignored" );
		}
		instr.immedA = flags;

		mexas::push_instr ( instr, pp );
		break;
	    }
	    case mex::SET_EXCEPTS:
	    {
		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nflags = index - first;

		min::uns32 flags = 0;
		for ( min::uns32 i = 0; i < nflags;
		                        ++ i )
		{
		    min::gen fname = statement[first+i];
		    min::gen fbit = min::get
		        ( mexcom::except_flag_table,
			  fname );
		    if ( fbit != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( fbit );
			flags |= (min::uns32) f;
		    }
		    else
			mexas::compile_error
			    ( pp, "Unrecognized except"
			          " flag: ",
				  min::pgen ( fname ),
				  "; ignored" );
		}
		instr.immedA = flags;

		mexas::push_instr ( instr, pp );
		break;
	    }
	    case mex::SET_OPTIMIZE:
	    {
	        min::gen param =
		    mexas::get_name ( index );
		if ( param == ::on )
		    instr.immedA = 1;
		else if ( param == ::off )
		    instr.immedA = 0;
		else
		    mexas::compile_error
		        ( pp, "SET_OPTIMIZE requires"
			      " `ON' or `OFF'"
			      " parameter; `OFF'"
			      " assumed" );

		mexas::push_instr ( instr, pp );
		break;
	    }
	    case mex::NOP:
	    case mex::TRACE:
	    case mex::WARN:
	    case mex::ERROR:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		instr.immedA = tvars;
		mexas::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::PUSHNARGS:
	    case mex::PUSHV:
	    {
		min::uns32 level;
	        min::gen nl = mexas::get_num ( index );
		if ( nl == min::NONE() )
		    level = L;
		else if ( !  mexas::check_parameter
		                 ( level, nl,
			           pp, "level", true ) )
		    continue;
		instr.immedB = level;

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
		      L, mexstack::depth[L] );
		break;
	    }

	    default:
		mexas::push_instr ( instr, pp );
	    }
	}
	TRACE:
	{
	    bool no_source = false;
	    if ( index < mexas::statement->length )
	    {
		min::printer printer =
		    mexcom::input_file->printer;
		min::phrase_position pp =
		    m->position[m->length - 1];
		printer << min::bom << "ERROR: "
		        << min::place_indent ( 0 )
			<< "extra stuff at end of"
			   " instruction: ";
		while (   index
		        < mexas::statement->length )
		{
		    min::gen item =
			mexas::statement[index++];
		    printer << " ";
		    if ( ( item == mexas::single_quote
		           ||
			   item == mexas::double_quote )
			 &&
			   index
			 < mexas::statement->length )
		        printer
			    << min::pgen_never_quote
			           ( item )
			    << min::pgen_never_quote
			           ( mexas::statement
				         [index++] )
			    << min::pgen_never_quote
			           ( item );
		    else
		        printer << min::pgen ( item );
		}
		printer << min::indent
		        << min::pline_numbers
			   ( mexcom::input_file, pp )
			<< ":" << min::eom;

		min::print_phrase_lines
		    ( printer, mexcom::input_file, pp );

		++ mexcom::error_count;

		no_source = true;
	    }
	    mexcom::trace_instr
		( m->length - 1,
		  mexstack::var_stack_length,
		  no_source );
	    continue;
	}

    }

    mexas::jump_list_delete ( mexas::jumps );

    if ( mexcom::error_count > 0 )
        return min::NULL_STUB;

    if (    mexcom::input_file->file_name
         == min::MISSING() )
        mex::name_ref(m) = min::MISSING();
    else
    {
        min::str_ptr sp
	    ( mexcom::input_file->file_name );
	const char * p =
	    min::unprotected::str_of ( sp );
	min::uns32 len = std::strlen ( p );
	if ( len > 4
	     &&
	     std::strcmp ( ".mex", p + len - 4 ) == 0 )
	    mex::name_ref(m) =
	        min::new_str_gen ( p, len - 4 );
	else
	    mex::name_ref(m) =
	        mexcom::input_file->file_name;

    }

    mexas::make_module_interface();

    min::locatable_var<mex::process> process
        ( mex::init_process ( m ) );
    mex::run_process ( process );
    if ( process->state != mex::MODULE_END )
    {
        process->printer
	    << min::bom << "ERROR: "
	    << min::place_indent ( 0 )
	    << "module initialization process did not"
	       " terminate normally at module end: "
	    << mex::state_infos[process->state]
	    	    .description;
        if ( process->state == mex::EXCEPTS_ERROR )
	{
	    process->printer << "; exceptions are: ";
	    mex::print_excepts
	        ( process->printer,
		  process->excepts_accumulator,
		  process->excepts_mask );
	}
	process->printer << min::eom;
        return min::NULL_STUB;
    }

    min::packed_vec_insptr<min::gen> g =
        (min::packed_vec_insptr<min::gen>)
	min::gen_packed_vec_type.new_stub
	    ( process->length );
    mex::globals_ref ( m ) = g;
    for ( min::uns32 i = 0; i < process->length; ++ i )
        min::push(g) = process[i];

    min::push ( mexas::modules ) = m;

    return m;
}
