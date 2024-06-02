// MIN System Execution Engine Assembler
//
// File:	mexstack.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jun  2 15:26:50 EDT 2024
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

mexstack::print mexstack::print_switch =
    mexstack::NO_PRINT;

# define L mexstack::lexical_level
# define SP mexstack::var_stack_length

min::uns32 mexstack::var_stack_length = 0;
min::uns32 mexstack::func_stack_length = 0;
min::uns32 mexstack::func_var_stack_length = 0;

min::uns8 mexstack::lexical_level;
min::uns8 mexstack::depth[mex::max_lexical_level+1];
min::uns32 mexstack::lp[mex::max_lexical_level+1];
min::uns32 mexstack::fp[mex::max_lexical_level+1];

static min::packed_vec<mexstack::block_element>
     block_stack_vec_type
         ( "block_stack_vec_type" );

min::locatable_var<mexstack::block_stack>
    mexstack::blocks;

min::uns32 mexstack::stack_limit;

static min::uns32 jump_element_gen_disp[] =
{
    min::DISP ( & mexstack::jump_element::target_name ),
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

void mexstack::print_instr
	( min::uns32 location,
	  bool no_source, min::uns32 stack_offset )
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
	<< ":" << location
	<< ";" <<   mexstack::var_stack_length
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
	<< " " << instr.immedC
	<< " " << instr.immedD
	<< "; "
	<< m->trace_info[location]
	<< min::eom;
}

unsigned mexstack::jump_list_delete
	( mexstack::jump_list jlist )
{
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
	if (   next->lexical_level
	     < mexstack::lexical_level )
	    break;
	mexcom::compile_error
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

unsigned mexstack::jump_list_update
	( mexstack::jump_list jlist )
{
    min::ptr<mexstack::jump_element> previous =
        jlist + 1;

    unsigned count = 0;
    while ( min::uns32 n = previous->next )
    {
        min::ptr<mexstack::jump_element> next =
	    jlist + n;
	if ( next->lexical_level < L )
	    break;
	if ( next->maximum_depth > mexstack::depth[L] )
	    next->maximum_depth = mexstack::depth[L];
	if ( next->var_stack_minimum > SP )
	    next->var_stack_minimum = SP;
	previous = next;
	++ count;
    }
    return count;
}

unsigned mexstack::jump_list_resolve
	( mexstack::jump_list jlist,
	  min::gen target_name )
{
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
	if ( target_name == next->target_name
	     &&
	     next->maximum_depth >= mexstack::depth[L] )
	{
	    min::uns32 depth_diff =
	        next->depth - mexstack::depth[L];
	    min::phrase_position pp =
		m->position[next->jmp_location];
	    if ( SP > next->var_stack_minimum )
		mexcom::compile_error
		    ( pp, "code jumped over pushes"
		          " values into the stack;"
			  " JMP unresolved" );
	    else if ( SP < next->var_stack_minimum )
		mexcom::compile_error
		    ( pp, "code jumped over pops"
		          " values from the stack;"
			  " JMP unresolved" );
	    else
	    {
		min::ptr<mex::instr> instr =
		    m + next->jmp_location;
		instr->immedA = next->var_stack_length
			      - next->var_stack_minimum;
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

void mexstack::begx ( mex::instr & instr,
		   min::uns32 nvars, min::uns32 tvars,
	           min::gen trace_info,
                   const min::phrase_position & pp )
{
    mexstack::block_element e =
        { instr.op_code, 0,
	  mexstack::var_stack_length,
	  mexstack::func_stack_length, 0,
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
	e.stack_limit =
	    mexstack::var_stack_length + nvars;

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
	    ( "bad instr.op_code to mexstack::begx" );

    min::push ( mexstack::blocks ) = e;
    mexstack::stack_limit = e.stack_limit;
    mexcom::push_instr ( instr, pp, trace_info );
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
    mexstack::func_stack_length =
        e.func_stack_length;
    mexstack::func_var_stack_length =
        e.func_var_stack_length;
 
    if ( instr.op_code == mex::ENDF )
    {
	min::ptr<mex::instr> ip =
	    mexcom::output_module + e.begin_location;
        ip->immedC = mexcom::output_module->length + 1
	           - e.begin_location;
	mexstack::print_instr
	    ( e.begin_location, true );
	instr.immedB = L;
	mexstack::jump_list_delete ( mexstack::jumps );
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
	mexstack::jump_list_update ( mexstack::jumps );
    }
    else // if mex::END
    {
	instr.immedA = mexstack::var_stack_length
	             - e.stack_limit + tvars;
	-- mexstack::depth[L];
	mexstack::var_stack_length =
		e.stack_limit;
	mexstack::jump_list_update ( mexstack::jumps );
    }
    mexstack::pop_stacks();

    min::uns32 len = mexstack::blocks->length;
    mexstack::stack_limit =
        ( len == 0 ? 0 : 
	  (mexstack::blocks+(len-1))->stack_limit );
    mexcom::push_instr ( instr, pp, trace_info );

    return 0;
}

void mexstack::cont ( mex::instr & instr,
		   min::uns32 tvars,
	           min::gen trace_info,
                   const min::phrase_position & pp )
{
    if ( mexstack::blocks->length == 0 )
    {
	mexcom::compile_error
	    ( pp, "not in a BEGL ... ENDL block;"
	          " instruction ignored" );
	return;
    }
    min::ptr<mexstack::block_element> bp =
          mexstack::blocks
	+ ( mexstack::blocks->length - 1 );
    if ( bp->begin_op_code != mex::BEGL )
    {
	mexcom::compile_error
	    ( pp, "not in a BEGL ... ENDL block;"
	          " instruction ignored" );
	return;
    }

    instr.immedA = mexstack::var_stack_length
		 - bp->stack_limit + tvars;
    instr.immedB = bp->nvars;
    instr.immedC = mexcom::output_module->length
		 - bp->begin_location - 1;

    mexcom::push_instr ( instr, pp, trace_info );
}


void mexstack::push_push_instr
        ( min::gen new_name, min::gen name,
	  min::uns32 index,
	  const min::phrase_position & pp,
	  min::uns32 sp_offset )
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
	instr.immedA = SP + sp_offset - index - 1;
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
    mexcom::push_instr ( instr, pp, trace_info );
}