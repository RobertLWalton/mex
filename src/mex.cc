// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Wed Jul  5 18:56:57 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Optimized Run Process
//	Run Process


// Setup
// -----

# include <cstdlib>
# include <cstdio>
# include <cfenv>
# include <cmath>
# include <iostream>
# include <mex.h>
# define MUP min::unprotected

min::locatable_var<min::printer> mex::default_printer;


// Optimized Run Process
// --------- --- -------

// Run process that is optimized.  Run it until either
//     (1) the process terminates normally,
// or  (2) the instruction count limit is reached,
// or  (3) the next instruction would have an error if
//         executed,
// or  (4) the pc is invalid,
// or  (5) if min::pending() returns true just before
//         executing a backward jump or function return.
// Return true if (1), normal process termination, and
// false otherwise.
//
static bool optimized_run_process ( mex::process p )
{
    mex::module m = p->pc.module;
    min::uns32 i = p->pc.index;
    if ( m == min::NULL_STUB ) return i == 0;
    if ( i >= m->length ) return i == m->length;
    mex::instr * pcbegin = ~ ( m + 0 );
    mex::instr * pc = ~ ( m + i );
    mex::instr * pcend = ~ ( m + m->length );

    i = p->sp;
    if ( i > p->max_length ) return false;
    min::gen * spbegin = ~ ( p + 0 );
    min::gen * sp = ~ ( p + i );
    min::gen * spend = ~ ( p + p->max_length );

    min::uns32 limit = p->limit;
    if ( p->counter >= limit ) return false;
    limit -= p->counter;

    bool result = true;
    feclearexcept ( FE_ALL_EXCEPT );

#   define CHECK1 \
	    if ( sp < spbegin + 1 \
	         || \
	         ! min::is_direct_float ( sp[-1] ) ) \
	        goto ERROR_EXIT

#   define CHECK2 \
	    if ( sp < spbegin + 2 \
	         || \
	         ! min::is_direct_float ( sp[-1] ) \
	         || \
	         ! min::is_direct_float ( sp[-2] ) ) \
	        goto ERROR_EXIT
#   define GF(x) min::new_direct_float_gen ( x )
#   define FG(x) MUP::direct_float_of ( x )

    while ( true )
    {
        if ( pc == pcend ) goto EXIT;
	if ( limit == 0 ) goto ERROR_EXIT;
	min::uns8 op_code = pc->op_code;
	switch ( op_code )
	{
	case mex::ADD:
	    CHECK2;
	    sp[-2] = GF
	        ( FG ( sp[-2] ) + FG ( sp[-1] ) );
	    -- sp;
	    break;
	case mex::NEG:
	    CHECK1;
	    sp[-1] = GF ( - FG ( sp[-1] ) );
	    break;
	case mex::PUSH:
	{
	    int i = pc->immedA;
	    if ( sp >= spend || i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp = sp[-i-1];
	    ++ sp;
	    break;
	}
	case mex::PUSHG:
	{
	    int i = pc->immedA;
	    mex::module mg = (mex::module) pc->immedD;
	    if ( mg == min::NULL_STUB )
	        goto ERROR_EXIT;
	    min::packed_vec_ptr<min::gen> globals =
	        mg->globals;
	    if ( globals == min::NULL_STUB )
	        goto ERROR_EXIT;
	    if ( i >= globals->length )
	        goto ERROR_EXIT;
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    * sp ++ = globals[i];
	    break;
	}
	case mex::PUSHM:
	{
	    int i = pc->immedA;
	    if ( i >= sp - spbegin )
	        goto ERROR_EXIT;
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    * sp ++ = spbegin[i];
	    break;
	}
	case mex::POP:
	{
	    int i = pc->immedA;
	    if ( sp <= spbegin || i >= sp - spbegin )
	        goto ERROR_EXIT;
	    -- sp;
	    sp[-i] = * sp;
	    break;
	}
	case mex::POPM:
	{
	    int i = pc->immedA;
	    if ( sp <= spbegin || i >= sp - spbegin )
	        goto ERROR_EXIT;
	    -- sp;
	    spbegin[i] = * sp;
	    break;
	}
	case mex::BEG:
	case mex::NOP:
	    break;
	case mex::END:
	{
	    int i = pc->immedA;
	    if ( i > sp - spbegin )
	        goto ERROR_EXIT;
	    sp -= i;
	    break;
	}
	case mex::BEGL:
	{
	    int i = pc->immedB;
	    if ( i > sp - spbegin )
	        goto ERROR_EXIT;
	    if ( i > spend - sp )
	        goto ERROR_EXIT;
	    min::gen * q1 = sp - i;
	    min::gen * q2 = sp;
	    while ( q1 < q2 )
		* sp ++ = * q1 ++;
	    break;
	}
	case mex::JMP:
	case mex::JMPEQ:
	case mex::JMPNE:
	{
	    int immedA = pc->immedA;
	    int immedB = pc->immedB;
	    int immedC = pc->immedC;
	    min::gen immedD = pc->immedD;
	    min::gen * new_sp = sp;
	    bool execute_jmp = true;
	    if ( op_code != mex::JMP )
	    {
		min::float64 arg1, arg2;
		if ( sp < spbegin + 2 )
		    goto ERROR_EXIT;
		new_sp -= 2;
		arg1 = FG ( new_sp[0] );
		arg2 = FG ( new_sp[1] );
		if ( std::isnan ( arg1 )
		     ||
		     std::isnan ( arg2 )
		     ||
		     (    std::isinf ( arg1 )
		       && std::isinf ( arg2 )
		       && arg1 * arg2 > 0 ) )
		    goto ERROR_EXIT;

		switch ( op_code )
		{
		case mex::JMPEQ:
		    execute_jmp = ( arg1 == arg2 );
		    break;
		case mex::JMPNE:
		    execute_jmp = ( arg1 != arg2 );
		    break;
		}
	    }
	    if ( immedA > new_sp - spbegin
	         ||
		 immedB - immedA > spend - new_sp
		 ||
		 immedC > pcend - pc )
	        goto ERROR_EXIT;

	    if ( ! execute_jmp )
	    {
	        sp = new_sp;
		break;
	    }

	    new_sp -= immedA;
	    sp = new_sp + immedB;
	    while ( new_sp < sp ) * new_sp ++ = immedD;

	    pc += immedC;
	    -- pc;
	    break;
	}
	case mex::ENDL:
	case mex::CONT:
	{
	    if ( min::pending() )
	        goto ERROR_EXIT;
	    int immedA = pc->immedA;
	    int immedB = pc->immedB;
	    int immedC = pc->immedC;
	    if ( immedA > sp - spbegin )
	        goto ERROR_EXIT;
	    if (   immedA + 2 * immedB
		 > sp - spbegin )
	        goto ERROR_EXIT;
	    if ( immedC > pc - pcbegin )
	        goto ERROR_EXIT;
	    sp -= immedA;
	    for ( int i = immedB; 0 < i; -- i )
	        sp[-immedB-i] = sp[-i];
	    pc -= immedC;
	    -- pc;
	    break;
	}
	case mex::SET_TRACE:
	case mex::ERROR:
	    goto ERROR_EXIT;
	}
	++ pc, -- limit;
    }

ERROR_EXIT:
    result = false;
EXIT:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
    p->excepts_accumulator |= 
	fetestexcept ( FE_ALL_EXCEPT );
    p->counter = p->limit - limit;
    return result;

#   undef CHECK1
#   undef CHECK2
#   undef FG
#   undef GF
}


// Run Process
// --- -------

// Operation Information Table.
//
enum
{
    NONA = 1,	// No arithmetic operands, not a JMP.
    A2 =   2,	// sp[0] and sp[-1] are the arithmetic
                // operands in that order.
    A2R =  3,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order.
    A2I =  4,	// sp[0] and immedD are the arithmetic
                // operands in that order.
    A2IR = 5,	// immedD and sp[0] are the arithmetic
                // operands in that order.
    A1 =   6,	// sp[0] is an arithmetic operand.
    J2 =   7,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order, and the
		// operation is a jump.
    J =    8,	// JMP, no arithmetic operands.
};

static
struct op_info
{
    min::uns8 op_code;
        // Op code as a check: e.g., mex::POP.
	// It should be true that:
	//     op_infos[mex::POP].op_code = mex::POP
    min::uns8 op_type;
        // See above.
    const char * name;
        // Name of op_code: e.g, "POP".
    const char * oper;
        // Name of op_code operator: e.g, "+" or "<=".
} op_infos[] =
{   { 0, 0, "", "" },
    { mex::ADD, A2, "ADD", "+" },
    { mex::MUL, A2, "MUL", "*" }
};

static min::uns8 max_op_code = 0;
    // Set by init_op_infos.


void init_op_infos ( void )
{
    op_info * p = op_infos;
    op_info * endp = (op_info *)
        ((char *) op_infos + sizeof ( op_infos ) );
    max_op_code = endp - p - 1;
    while  ( p < endp )
    {
        if ( p - op_infos != p->op_code )
	{
	    std::cerr << "BAD OP_INFOS[" << p - op_infos
	              << "] != " << p->op_code
		      << std::endl;
	    std::exit ( 1 );
	}
    }
}

bool mex::run_process ( mex::process p )
{
    mex::module m = p->pc.module;
    min::uns32 i = p->pc.index;
    mex::instr * pcbegin;
    mex::instr * pc;
    mex::instr * pcend;
    min::gen * spbegin;
    min::gen * sp;
    min::gen * spend;
    const char * message;
    min::uns32 limit;

    if ( m == min::NULL_STUB )
    {
        if ( i == 0 ) return true;
	message = "Illegal PC: no module and index > 0";
	goto FATAL;
    }
    if ( i >= m->length )
    {
        if ( i == m->length ) return true;
	message = "Illegal PC: index too large";
	goto FATAL;
    }
    pcbegin = ~ ( m + 0 );
    pc = ~ ( m + i );
    pcend = ~ ( m + m->length );

    i = p->sp;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large";
	goto FATAL;
    }
    spbegin = ~ ( p + 0 );
    sp = ~ ( p + i );
    spend = ~ ( p + p->max_length );

    limit = p->limit;
    if ( p->counter >= limit )
    {
        p->finish_state = mex::LIMIT_STOP;
	return false;
    }
    limit -= p->counter;

    // Macros to save and restore pc... and sp... .
    // We assume p->pc.index and p->sp do not change
    // between SAVE and RESTORE.
    //
    // Printing and interrupts may move module and
    // process and must be done between a SAVE and
    // a RESTORE.
    //
#   define SAVE \
	* (min::uns32 *) & p->pc.index = pc - pcbegin; \
	p->sp = p->length = sp - spbegin; \
	p->counter = p->limit - limit;

#   define RESTORE \
	pcbegin = ~ ( m + 0 ); \
	pc = pcbegin + p->pc.index; \
	pcend = pcbegin + m->length; \
	spbegin = ~ ( p + 0 ); \
	sp = spbegin + p->sp; \
	spend = sp + p->max_length; \
        limit = p->limit - p->counter;

    while ( true ) // Inner loop.
    {
        if ( pc == pcend )
	{
	    SAVE;
	    p->finish_state = mex::MODULE_END;
	    return true;
	}
	if ( limit == 0 )
	{
	    SAVE;
	    p->finish_state = mex::LIMIT_STOP;
	    return false;
	}

	if ( p->optimize )
	{
	    SAVE;
	    if ( optimized_run_process ( p ) )
	    {
	        if ( p->pc.module == min::NULL_STUB )
		    p->finish_state = mex::CALL_END;
		else
		    p->finish_state = mex::MODULE_END;
	        return true;
	    }
	    min::interrupt();
	    mex::module om = p->pc.module;
	    min::uns32 oi = p->pc.index;
	    if ( om == min::NULL_STUB )
	    {
		if ( oi == 0 )
		{
		    p->finish_state = mex::CALL_END;
		    return true;
		}
		message = "Illegal PC: no module and"
		          " index > 0";
		goto FATAL;
	    }
	    if ( oi >= m->length )
	    {
		if ( oi == m->length )
		{
		    p->finish_state = mex::MODULE_END;
		    return true;
		}
		message = "Illegal PC: index too large";
		goto FATAL;
	    }

	    if ( p->counter >= p->limit )
	    {
		p->finish_state = mex::LIMIT_STOP;
		return false;
	    }

	    RESTORE;
	}

        min::uns8 op_code = pc->op_code;
	op_info * op_info;
	min::float64 arg1, arg2;
	min::gen * new_sp = sp;
	min::uns8 trace_flags = pc->trace_flags; 

	if ( op_code > max_op_code )
	{
	    message = "Illegal op code: too large";
	    goto INNER_FATAL;
	}
	op_info = op_infos + op_code;

#	define FG(x) MUP::direct_float_of ( x )
	    // If x is a min::gen that is not a direct
	    // float, it will be a signalling NaN that
	    // will raise the invalid exception and
	    // generate a non-signalling NaN result.

	switch ( op_info->op_type )
	{
	case NONA:
	    goto NON_ARITHMETIC;
	case A2:
	    new_sp -= 2;
	    if ( new_sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = FG ( new_sp[0] );
	    arg2 = FG ( new_sp[1] );
	    goto ARITHMETIC;
	case A2R:
	    new_sp -= 2;
	    if ( new_sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = FG ( new_sp[1] );
	    arg2 = FG ( new_sp[0] );
	    goto ARITHMETIC;
	case A2I:
	    new_sp -= 1;
	    if ( new_sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = FG ( new_sp[0] );
	    arg2 = FG ( pc->immedD );
	    goto ARITHMETIC;
	case A2IR:
	    new_sp -= 1;
	    if ( new_sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = FG ( pc->immedD );
	    arg2 = FG ( new_sp[0] );
	    goto ARITHMETIC;
	case A1:
	    if ( new_sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = FG ( new_sp[0] );
	    arg2 = 0; // To avoid error detector.
	    goto ARITHMETIC;
	case J2:
	    new_sp -= 2;
	    if ( new_sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = FG ( new_sp[0] );
	    arg2 = FG ( new_sp[1] );
	    goto JUMP;
	case J:
	    goto JUMP;
	default:
	    message = "internal system error:"
	              " bad op_type";
	    goto INNER_FATAL;
	}
#	undef FG

	ARITHMETIC:
	{
	    // Process arithmetic operator, excluding
	    // JMPs.

	    min::float64 result;

	    feclearexcept ( FE_ALL_EXCEPT );

	    switch ( op_code )
	    {
	    case mex::ADD:
	    case mex::ADDI:
	        result = arg1 + arg2;
		break;
	    case mex::SUB:
	    case mex::SUBI:
	    case mex::SUBR:
	    case mex::SUBRI:
	        result = arg1 + arg2;
		break;
	    }

	    int excepts =
	        fetestexcept ( FE_ALL_EXCEPT );
	    p->excepts_accumulator |= excepts;
	    excepts &= p->excepts;

	    trace_flags &= p->trace_flags;

	    if (    excepts != 0
	         || trace_flags & mex::TRACE )
	    {
	        SAVE;

		if ( excepts != 0 )
		{
		    p->printer << min::bol
			       << "!!! ERROR: ";
		    if ( excepts & FE_INVALID )
			p->printer << "invalid operand(s)";
		    else if ( excepts & FE_DIVBYZERO )
			p->printer << "divide by zero";
		    else if ( excepts & FE_OVERFLOW )
			p->printer << "numeric overflow";
		    else if ( excepts & FE_INEXACT )
			p->printer << "inexact numeric result";
		    else if ( excepts & FE_UNDERFLOW )
			p->printer << "numeric underflow";
		    else
			p->printer <<
			    "unknown numeric exception";
		    p->printer << min::eol;
		}

		min::phrase_position pp =
		    p->pc.index < m->position->length ?
		    m->position[p->pc.index] :
		    min::MISSING_PHRASE_POSITION;

		if ( pp
		     &&
		     ( trace_flags & mex::TRACE_PHRASE ) )
		    min::print_phrase_lines
		        ( p->printer,
			  m->position->file,
			  pp );

		{
		    unsigned i = ( p->trace_depth + 1 )
		               * mex::trace_indent;
		    while ( 1 < i )
		    {
		        p->printer << mex::trace_mark;
			-- i;
		    }
		    p->printer << ' ';
		}
		p->printer << min::bom;

		if ( pp )
		    p->printer
		        << min::pline_numbers
			    ( m->position->file, pp )
			<< ": ";

		char buffer[200];
	        sprintf
		    ( buffer, ": %s: %.15g %s %.15g",
		      op_info->name,
		      arg1, op_info->oper, arg2 );
		p->printer << buffer
		           << min::eom;

	        RESTORE;
	    }

	    * new_sp ++ = min::new_num_gen ( result );
	    sp = new_sp;

	    goto LOOP;
	}

	JUMP:
	{
	    // Process JMP.

	    int immedA = pc->immedA;
	    int immedB = pc->immedB;
	    int immedC = pc->immedC;
	    min::gen immedD = pc->immedD;

	    if ( immedA > new_sp - spbegin )
	    {
	        message = "immedA too large";
	        goto INNER_FATAL;
	    }
	    if ( immedB - immedA > spend - new_sp )
	    {
	        message = "immedB too large";
	        goto INNER_FATAL;
	    }
	    if ( pc + immedC > pcend )
	    {
	        message = "immedC too large";
	        goto INNER_FATAL;
	    }

	    bool bad_jmp = false;
	    bool execute_jmp = true;
	    if ( op_code != mex::JMP )
	    {
		if ( std::isnan ( arg1 )
		     ||
		     std::isnan ( arg2 )
		     ||
		     (    std::isinf ( arg1 )
		       && std::isinf ( arg2 )
		       && arg1 * arg2 > 0 ) )
		    bad_jmp = true;
		else switch ( op_code )
		{
		case mex::JMPEQ:
		    execute_jmp = ( arg1 == arg2 );
		    break;
		case mex::JMPNE:
		    execute_jmp = ( arg1 != arg2 );
		    break;
		}
	    }

	    if ( bad_jmp )
	        trace_flags |= mex::TRACE
		             + mex::TRACE_PHRASE;
	    else
	    {
	        trace_flags &= pc->trace_flags;
		if ( ! execute_jmp
		     &&
		        (   trace_flags
		          & mex::TRACE_NOJUMP )
		     == 0 )
		    trace_flags &= ~ mex::TRACE;
	    }

	    if ( trace_flags & mex::TRACE )
	    {
		SAVE;

		p->printer << min::bol;
		if ( bad_jmp )
		    p->printer
		        << "!!! FATAL ERROR: "
			   " invalid operands to a"
			   " conditional jump"
			   " instruction"
			<< min::eol;

		min::phrase_position pp =
		    p->pc.index < m->position->length ?
		    m->position[p->pc.index] :
		    min::MISSING_PHRASE_POSITION;

		if ( pp
		     &&
		     ( trace_flags & mex::TRACE_PHRASE ) )
		    min::print_phrase_lines
		        ( p->printer,
			  m->position->file,
			  pp );
		{
		    unsigned i = ( p->trace_depth + 1 )
		               * mex::trace_indent;
		    while ( 1 < i )
		    {
		        p->printer << mex::trace_mark;
			-- i;
		    }
		    p->printer << ' ';
		}
		p->printer << min::bom;

		if ( pp )
		    p->printer
		        << min::pline_numbers
			    ( m->position->file, pp )
			<< ": ";

		min::gen tinfo  = min::MISSING();
		if ( p->pc.index < m->trace_info->length)
		    tinfo = m->trace_info[p->pc.index];
		min::gen name = min::MISSING();
		min::uns32 tinfo_length = 0;
		if ( min::is_obj ( tinfo ) )
		{
		    min::obj_vec_ptr vp ( tinfo );
		    tinfo_length = min::size_of ( vp );
		    if ( tinfo_length >= 1 )
		        name = vp[0];
		}
		else if ( min::is_str ( tinfo ) )
		    name = tinfo;

		if ( execute_jmp )
		    p->printer << "successful ";
		else if ( ! bad_jmp )
		    p->printer << "UNsuccessful ";

		if ( name == min::MISSING() )
		    p->printer << op_info->name;
		else
		    p->printer << name;

		if ( bad_jmp )
		    p->printer << " with invalid"
		                  " operand(s)";

		char buffer[200];
	        sprintf ( buffer, ": %.15g %s %.15g",
			  arg1, op_info->oper, arg2 );
		p->printer << buffer;

		if ( tinfo_length > 1
		     &&
		     p->trace_function != NULL )
		    (p->trace_function) ( p, tinfo );     

		p->printer << min::eom;

		if ( bad_jmp )
		{
		    p->finish_state = mex::JMP_ERROR;
		    return false;
		}

		RESTORE;
	    }

	    if ( ! execute_jmp )
		sp = new_sp;
	    else
	    {
		new_sp -= immedA;
		sp = new_sp + immedB;
		while ( new_sp < sp )
		    * new_sp ++ = immedD;
		pc += immedC;
		-- pc;
	    }

	    goto LOOP;
	}

	NON_ARITHMETIC:
        {
	    // Process instructions that push and
	    // pop.

	    int immedA = pc->immedA;
	    int immedB = pc->immedB;
	    int immedC = pc->immedC;
	    min::gen immedD = pc->immedD;

	    bool fatal_error = false;

	    // Pre-trace check for fatal errors.
	    //
	    switch ( op_code )
	    {
	    case mex::PUSH:
	        if ( immedA + 1 > sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		break;
	    case mex::PUSHI:
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		break;
	    case mex::PUSHG:
	    {
	        mex::module mg = (mex::module) immedD;
		if ( mg == min::NULL_STUB )
		{
		    message = "immedD is not a module";
		    goto INNER_FATAL;
		}
		if ( mg->globals == min::NULL_STUB )
		{
		    message = "module has no globals";
		    goto INNER_FATAL;
		}
	        if ( immedA >= mg->globals->length  )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		break;
	    }
	    case mex::PUSHM:
	        if ( immedA >= sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		break;
	    case mex::POP:
		if ( sp <= spbegin )
		{
		    message =
		        "stack empty for pop";
		    goto INNER_FATAL;
		}
	        if ( immedA + 1 > sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
		break;
	    case mex::BEG:
	        break;
	    case mex::NOP:
	        break;
	    case mex::END:
	    {
		if ( immedA > sp - spbegin )
		    goto INNER_FATAL;
		break;
	    }
	    case mex::BEGL:
	        if ( immedB > sp - spbegin )
		{
		    message = "immedB too large";
		    goto INNER_FATAL;
		}
		if ( sp + immedB > spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
	        break;
	    case mex::ENDL:
	    case mex::CONT:
	        if ( immedA > sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
	        if (   immedA + 2 * immedB
		     > sp - spbegin )
		{
		    message =
		        "immedA + 2 * immedB too large";
		    goto INNER_FATAL;
		}
	        if ( immedC > pc - pcbegin )
		{
		    message = "immedC too large";
		    goto INNER_FATAL;
		}
		break;
	    case mex::SET_TRACE:
	        break;
	    case mex::ERROR:
	        if (    immedB != 0 )
		    fatal_error = true;
		else if ( immedA > sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
	        break;
	    }

	    if ( fatal_error )
	        trace_flags |= mex::TRACE
		             + mex::TRACE_PHRASE;
	    else
	        trace_flags &= pc->trace_flags;

	    if ( trace_flags & mex::TRACE )
	    {
		SAVE;

		p->printer << min::bol;
		if ( fatal_error )
		    p->printer
		        << "!!! FATAL ERROR: "
			   " ERROR instruction with"
			   " zero immedB"
			<< min::eol;

		min::phrase_position pp =
		    p->pc.index < m->position->length ?
		    m->position[p->pc.index] :
		    min::MISSING_PHRASE_POSITION;

		if ( pp
		     &&
		     ( trace_flags & mex::TRACE_PHRASE ) )
		    min::print_phrase_lines
		        ( p->printer,
			  m->position->file,
			  pp );
		{
		    unsigned i = ( p->trace_depth + 1 )
		               * mex::trace_indent;
		    while ( 1 < i )
		    {
		        p->printer << mex::trace_mark;
			-- i;
		    }
		    p->printer << ' ';
		}
		p->printer << min::bom;

		if ( pp )
		    p->printer
		        << min::pline_numbers
			    ( m->position->file, pp )
			<< ": ";

		min::gen tinfo  = min::MISSING();
		if ( p->pc.index < m->trace_info->length)
		    tinfo = m->trace_info[p->pc.index];
		min::gen name = min::MISSING();
		min::uns32 tinfo_length = 0;
		if ( min::is_obj ( tinfo ) )
		{
		    min::obj_vec_ptr vp ( tinfo );
		    tinfo_length = min::size_of ( vp );
		    if ( tinfo_length >= 1 )
		        name = vp[0];
		}
		else if ( min::is_str ( tinfo ) )
		    name = tinfo;

		if ( name == min::MISSING() )
		    p->printer << op_info->name;
		else
		    p->printer << name;

		if ( tinfo_length > 1
		     &&
		     p->trace_function != NULL )
		    (p->trace_function) ( p, tinfo );     

		p->printer << min::eom;

		if ( fatal_error )
		{
		    p->finish_state = mex::ERROR_STOP;
		    return false;
		}

		RESTORE;
	    }

	    // Execute instruction.
	    //
	    switch ( op_code )
	    {
	    case mex::PUSH:
	    {
		int i = pc->immedA;
		* sp = sp[-i-1];
		++ sp;
		break;
	    }
	    case mex::PUSHI:
	    {
		* sp ++ = pc->immedD;
		break;
	    }
	    case mex::PUSHG:
	    {
	        mex::module mg =
		    (mex::module) pc->immedD;
		* sp ++ = mg->globals[pc->immedA];
		break;
	    }
	    case mex::PUSHM:
	        * sp ++ = spbegin[pc->immedA];
		break;
	    case mex::POP:
	    {
		int i = pc->immedA;
		-- sp;
		sp[-i] = * sp;
		break;
	    }
	    case mex::POPM:
	    {
		int i = pc->immedA;
		-- sp;
		spbegin[i] = * sp;
		break;
	    }
	    case mex::BEG:
	        break;
	    case mex::NOP:
	        break;
	    case mex::END:
	    {
		int i = pc->immedA;
		sp -= i;
		break;
	    }
	    case mex::BEGL:
	    {
	        int i = pc->immedB;
		min::gen * q1 = sp - i;
		min::gen * q2 = sp;
		while ( q1 < q2 )
		    * sp ++ = * q1 ++;
	        break;
	    }
	    case mex::ENDL:
	    case mex::CONT:
	    {
		if ( min::pending() )
		{
		    SAVE;
		    min::interrupt();
		    RESTORE;
		}
		int immedA = pc->immedA;
		int immedB = pc->immedB;
		int immedC = pc->immedC;
		sp -= immedA;
		for ( int i = immedB; 0 < i; -- i )
		    sp[-immedB-i] = sp[-i];
		pc -= immedC;
		-- pc;
		break;
	    }
	    case mex::SET_TRACE:
	        p->trace_flags = (min::uns8) pc->immedA;
	        break;
	    case mex::ERROR:
	        break;
	    }

	    goto LOOP;
	}

	LOOP:
	    ++ pc; -- limit;
	    continue; // loop


	// Fatal error discovered in loop.
	//
	INNER_FATAL:
	    SAVE;
	    goto FATAL;

    } // end loop

// Come here with fatal error `message'.  At this point
// there is no instruction to pin the blame on - its a
// process state error - which can only happen if the
// compiler has made a mistake.
//
FATAL:
    p->finish_state = mex::FORM_ERROR;
    char fatal_buffer[100];
    char instr_buffer[400];
    char * q = instr_buffer;
    q += sprintf ( q, "OP CODE = " );
    if ( p->pc.module == min::NULL_STUB
         ||
	 p->pc.index >= p->pc.module->length )
	q += sprintf ( q, "<NOT AVAILABLE>" );
    else
    {
        mex::instr * instr =
	    ~ ( p->pc.module + p->pc.index );
	min::uns8 op_code = instr->op_code;
	op_info * op_info = ( op_code <= max_op_code ?
	                   op_infos + op_code :
			   NULL );
	if ( op_info != NULL )
	    q += sprintf ( q, "%s", op_info->name );
	else
	    q += sprintf
	        ( q, "%u (TOO LARGE)", op_code );
	q += sprintf
	    ( q, ", IMMEDA = %u", instr->immedA );
	q += sprintf
	    ( q, ", IMMEDB = %u", instr->immedB );
	q += sprintf
	    ( q, ", IMMEDC = %u", instr->immedC );
    }

    p->printer << min::bol << "FATAL ERROR: " << min::bom
               << message
	       << min::indent
	       << "PC->MODULE = "
	       << ( p->pc.module == min::NULL_STUB ?
	            min::new_str_gen ( "<NULL MODULE>"  ):
		       p->pc.module->position->file
		    == min::NULL_STUB ?
		    min::new_str_gen ( "<NULL FILE>" ):
		       p->pc.module->position->file
		                   ->file_name
		    == min::MISSING() ?
		    min::new_str_gen ( "<NO FILE NAME>" ):
		    p->pc.module->position->file
		                ->file_name )
	       << ", PC INDEX = " << p->pc.index
	       << min::indent
	       << "MODULE LENGTH (CODE VECTOR SIZE) = "
	       << ( p->pc.module == min::NULL_STUB ?
	                "<NOT AVAILABLE>" :
		    ( sprintf ( fatal_buffer, "%u",
		                p->pc.module->length ),
		      fatal_buffer ) )
	       << min::indent
	       << instr_buffer
	       << min::indent
	       << "SP = " << p->sp
	       << ", PROCESS MAX_LENGTH = "
	       << p->max_length
	       << min::indent
	       << "RP = " << p->rp
	       << ", RETURN STACK MAX_LENGTH = "
	       << p->return_stack->max_length
	       << min::eom;
		        
    return false;

} // mex::run_process
