// MIN System Execution Engine Assembler
//
// File:	mexstack.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Dec 22 03:27:30 AM EST 2024
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

# include <mexstack.h>

bool mexstack::trace_never = false;
mexstack::print mexstack::print_switch =
    mexstack::NO_PRINT;

# define L mexstack::lexical_level
# define SP mexstack::run_stack_length

min::uns32 mexstack::run_stack_length = 0;

min::uns8 mexstack::lexical_level;
min::uns8 mexstack::depth[mex::max_lexical_level+1];
min::uns32 mexstack::lp[mex::max_lexical_level+1];
min::uns32 mexstack::fp[mex::max_lexical_level+1];

static min::packed_vec<mexstack::block_element>
     block_stack_vec_type
         ( "block_stack_vec_type" );

min::locatable_var<mexstack::block_stack>
    mexstack::blocks;

min::uns32 mexstack::run_stack_limit;

static min::uns32 jump_element_gen_disp[] =
{
    min::DISP ( & mexstack::jump_element::target ),
    min::DISP_END
};

static min::packed_vec<mexstack::jump_element>
     jump_list_vec_type
         ( "jump_list_vec_type",
	   ::jump_element_gen_disp );

min::locatable_var<mexstack::jump_list>
    mexstack::jumps;

static void initialize ( void )
{
    mexstack::blocks =
	::block_stack_vec_type.new_stub ( 100 );
    mexstack::jumps =
	::jump_list_vec_type.new_stub ( 500 );
}
static min::initializer initializer ( ::initialize );


// Support Functions
// ------- ---------

void mexstack::init ( void )
{
    mexstack::run_stack_length		= 0;
    mexstack::lexical_level		= 0;
    mexstack::depth[0]			= 0;
    mexstack::lp[0]			= 0;
    mexstack::fp[0]			= 0;
    mexstack::run_stack_limit		= 0;

    min::pop ( mexstack::blocks,
               mexstack::blocks->length );
    min::pop ( mexstack::jumps,
               mexstack::jumps->length );
    mexstack::jump_element e =
        { min::MISSING(), 0, 0, 0, 0, 0, 0, 0 };
    min::push ( mexstack::jumps ) = e;  // Free head.
    min::push ( mexstack::jumps ) = e;  // Active head.
}

void mexstack::print_instr
	( min::uns32 location,
	  bool no_source, min::int32 stack_offset )
{
    mexstack::print print = mexstack::print_switch;
    min::printer printer =
	mexcom::input_file->printer;
    mex::module m = mexcom::output_module;

    if ( print == mexstack::NO_PRINT )
	return;

    min::phrase_position pp = m->position[location];
    if ( print == mexstack::PRINT_WITH_SOURCE
         &&
	 ! no_source )
	min::print_phrase_lines
	    ( printer, mexcom::input_file, pp );

    mex::instr instr = m[location];
    printer
        << min::bol << min::bom <<"    "
	<< "[" << pp.end.line
		  +
		  ( pp.end.offset != 0 )
	<< ":" << location
	<< ";" <<   mexstack::run_stack_length
	          + stack_offset
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
	<< " " << instr.immedC;
    if ( instr.immedD != min::MISSING() )
	printer << " "
	        << min::pgen_quote ( instr.immedD );
    if ( m->trace_info[location] != min::MISSING() )
	printer << "; "
	        << m->trace_info[location];
    printer << min::eom;
}

unsigned mexstack::jmp_clear ( void )
{
    mexstack::jump_list jlist = mexstack::jumps;

    min::ptr<mexstack::jump_element> free =
        jlist + 0;
    min::ptr<mexstack::jump_element> previous =
        jlist + 1;

    mex::module_ins m = mexcom::output_module;

    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexstack::jump_element> next =
	    jlist + n;
	if ( next->lexical_level < L )
	    break;
	MIN_ASSERT
	    ( next->lexical_level == L,
	      "jmp_clear not called at ENDF" );
	mexcom::compile_error
	    ( m->position[next->jmp_location],
	      "jump target undefined: ",
	      min::pgen ( next->target ),
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

unsigned mexstack::jmp_update ( void )
{
    mexstack::jump_list jlist = mexstack::jumps;

    min::ptr<mexstack::jump_element> previous =
        jlist + 1;

    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexstack::jump_element> next =
	    jlist + n;
	if ( next->lexical_level < L )
	    break;
	MIN_ASSERT
	    ( next->lexical_level == L,
	      "jmp_clear not called at ENDF" );
	if ( next->minimum_depth > mexstack::depth[L] )
	     next->minimum_depth = mexstack::depth[L];
	if ( next->run_stack_minimum > SP )
	    next->run_stack_minimum = SP;
	previous = next;
	++ count;
    }
    return count;
}

unsigned mexstack::jmp_target
	( min::gen target )
{
    MIN_ASSERT
        ( min::is_name ( target ),
	  "jmp_target target is not a MIN name" );

    mexstack::jump_list jlist = mexstack::jumps;

    min::ptr<mexstack::jump_element> free =
        jlist + 0;
    min::ptr<mexstack::jump_element> previous =
        jlist + 1;

    mex::module_ins m = mexcom::output_module;
    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexstack::jump_element> next = 
	    jlist + n;
	if ( next->lexical_level < L )
	    break;
	if ( target == next->target
	     &&
	     next->minimum_depth == mexstack::depth[L] )
	{
	    min::uns32 depth_diff =
	        next->depth - mexstack::depth[L];
	    min::phrase_position pp =
		m->position[next->jmp_location];
	    if ( SP > next->run_stack_minimum )
		mexcom::compile_error
		    ( pp, "code jumped over pushes"
		          " of values into the stack;"
			  " JMP unresolved" );
	    else if ( SP < next->run_stack_minimum )
		mexcom::compile_error
		    ( pp, "code jumped over pops"
		          " of values from the stack;"
			  " JMP unresolved" );
	    else
	    {
		min::ptr<mex::instr> instr =
		    m + next->jmp_location;
		instr->immedA = next->run_stack_length
			      - next->run_stack_minimum;
		instr->immedC = m->length
			      - next->jmp_location;
		instr->trace_depth = depth_diff;

		mexstack::print_instr
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

void mexstack::print_label
	( min::gen name,
	  const min::phrase_position & pp,
	  bool no_source, min::int32 stack_offset )
{
    mexstack::print print = mexstack::print_switch;
    min::printer printer =
	mexcom::input_file->printer;
    mex::module m = mexcom::output_module;

    if ( print == mexstack::NO_PRINT )
	return;

    if ( pp
         && 
	 print == mexstack::PRINT_WITH_SOURCE
         &&
	 ! no_source )
	min::print_phrase_lines
	    ( printer, mexcom::input_file, pp );

    printer
        << min::bol << min::bom <<"    [";
    if ( pp ) printer << pp.end.line
                         +
			 ( pp.end.offset != 0 )
                      << ":";
    printer
	<< m->length
	<< ";" <<   mexstack::run_stack_length
	          + stack_offset
	<< "] LABEL " << min::pgen_name ( name )
        << min::eom;
}

void mexstack::begx ( mex::instr & instr,
		   min::uns32 nvars, min::uns32 tvars,
	           min::gen trace_info,
                   const min::phrase_position & pp )
{
    mexstack::block_element e =
        { instr.op_code, 0,
	  mexstack::run_stack_length,
	  nvars,
	  mexcom::output_module->length };

    min::int32 stack_offset = 0;

    if ( instr.op_code == mex::BEGF )
    {
        MIN_ASSERT ( L < mex::max_lexical_level,
		     "mex::max_lexical_level"
		     " exceeded" );
	MIN_ASSERT ( tvars == 0,
	             "BEGF has trace variables" );
	e.end_op_code = mex::ENDF;
	e.run_stack_limit =
	    mexstack::run_stack_length + nvars;
	stack_offset = nvars;

	++ L;
	mexstack::depth[L] = 0;
	mexstack::lp[L] = mexstack::run_stack_length;
	mexstack::fp[L] = mexstack::lp[L] + nvars;

	instr.immedA = nvars;
	instr.immedB = L;
    }
    else if ( instr.op_code == mex::BEGL )
    {
	e.end_op_code = mex::ENDL;
	e.run_stack_limit =
	    mexstack::run_stack_length + nvars;
	stack_offset = nvars;

        ++ mexstack::depth[L];
	instr.immedA = tvars;
	instr.immedB = nvars;
    }
    else if ( instr.op_code == mex::BEG )
    {
	instr.immedA = tvars;
	e.end_op_code = mex::END;
        ++ mexstack::depth[L];
    }
    else
        MIN_ABORT
	    ( "bad instr.op_code to mexstack::begx" );

    min::push ( mexstack::blocks ) = e;
    mexstack::run_stack_limit = e.run_stack_limit;
    mexstack::push_instr
        ( instr, pp, trace_info, false, stack_offset );
}

unsigned mexstack::endx ( mex::instr & instr,
		       min::uns32 tvars,
	               min::gen trace_info,
                       const min::phrase_position & pp )
{
    if ( mexstack::blocks->length == 0 )
    {
	mexcom::compile_error
	    ( pp, "there is no block to end here" );
	return 0;
    }
    min::ptr<mexstack::block_element> bp =
          mexstack::blocks
	+ ( mexstack::blocks->length - 1 );
    if ( bp->end_op_code != instr.op_code )
    {
        // Check that ending other blocks will
	// succeed.
	//
	bool match_found = false;
	for ( int i = 2;
	      i <= (int) mexstack::blocks->length;
	      ++ i )
	{
	    bp = mexstack::blocks
	       + ( mexstack::blocks->length - i );
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
	    mexcom::compile_error
		( pp, "there is no block to end here" );
	    return 0;
	}
	min::uns8 end_op_code = instr.op_code;
	unsigned count = 0;
	do {
	    bp = mexstack::blocks
	       + ( mexstack::blocks->length - 1 );
	    instr.op_code = bp->end_op_code;
	    if ( instr.op_code != end_op_code )
	    {
	        min::obj_vec_ptr vp =
		    mexcom::op_code_table;
		mexcom::compile_error
		    ( pp, "inserting ",
			  min::pgen
			      ( vp[instr.op_code] ),
			  " before here" );
	    }
	    mexstack::endx ( instr, pp, trace_info );
	    ++ count;
	} while ( instr.op_code != end_op_code );
	return count;
    }

    mexstack::block_element e =
        min::pop ( mexstack::blocks );
 
    if ( instr.op_code == mex::ENDF )
    {
	min::ptr<mex::instr> ip =
	    mexcom::output_module + e.begin_location;
        ip->immedC = mexcom::output_module->length + 1
	           - e.begin_location;

	if ( mexstack::print_switch
	     ==
	     mexstack::PRINT_WITH_SOURCE )
	    min::print_phrase_lines
		( mexcom::input_file->printer,
		  mexcom::input_file, pp );
	mexstack::print_instr
	    ( e.begin_location, true );

	instr.immedB = L;
	mexstack::jmp_clear();
	mexstack::run_stack_length =
		e.run_stack_limit - e.nvars;
	-- L;
    }
    else if ( instr.op_code == mex::ENDL )
    {
	instr.immedA = mexstack::run_stack_length
	             - e.run_stack_limit + tvars;
	instr.immedB = e.nvars;
        instr.immedC = mexcom::output_module->length
	             - e.begin_location - 1;
	-- mexstack::depth[L];
	mexstack::run_stack_length =
		e.run_stack_limit - e.nvars;
	mexstack::jmp_update();
    }
    else // if mex::END
    {
	instr.immedA = mexstack::run_stack_length
	             - e.run_stack_limit + tvars;
	-- mexstack::depth[L];
	mexstack::run_stack_length =
		e.run_stack_limit;
	mexstack::jmp_update();
    }
    mexstack::pop_stacks();

    min::uns32 len = mexstack::blocks->length;
    mexstack::run_stack_limit =
        ( len == 0 ? 0 : 
	  (mexstack::blocks+(len-1))->run_stack_limit );
    mexstack::push_instr
        ( instr, pp, trace_info,
	  instr.op_code == mex::ENDF );

    return 0;
}

bool mexstack::cont ( mex::instr & instr,
		      min::uns32 loop_depth,
		      min::uns32 tvars,
	              min::gen trace_info,
                      const min::phrase_position & pp )
{
    min::uns32 i = mexstack::blocks->length;
    min::ptr<mexstack::block_element> bp;
    while ( true )
    {
        if ( i == 0 ) return false;
	bp = mexstack::blocks + ( -- i );
	if ( bp->begin_op_code != mex::BEGL )
	{
	    ++ instr.trace_depth;
	    continue;
	}
        if ( -- loop_depth == 0 ) break;
	++ instr.trace_depth;
    }

    instr.immedA = mexstack::run_stack_length
		 - bp->run_stack_limit + tvars;
    instr.immedB = bp->nvars;
    instr.immedC = mexcom::output_module->length
		 - bp->begin_location - 1;

    mexstack::push_instr ( instr, pp, trace_info );
    return true;
}


void mexstack::push_push_instr
        ( min::gen new_name, min::gen name,
	  min::uns32 index,
	  const min::phrase_position & pp,
	  bool no_source,
	  min::int32 stack_offset )
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
	instr.immedA = SP + stack_offset - index - 2;
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
    mexstack::push_instr
        ( instr, pp, trace_info,
	  no_source, stack_offset );
}
