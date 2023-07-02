// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Jul  1 23:05:15 EDT 2023
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
# include <cfenv>
# include <iostream>
# include <mex.h>


// Optimized Run Process
// --------- --- -------

// Run process that is optimized.  Run it until either
//     (1) the process terminates normally,
// or  (2) the instruction count limit is reached,
// or  (3) the next instruction would have an error if
//         executed,
// or  (4) the pc is invalid,
// or  (5) if min::interrupt() returns true just before
//         executing a backward jump or function return.
// Return true if (1), normal process termination, and
// false otherwise.
//
static bool optimized_run_process
	( mex::process p, min::uns32 limit )
{
    mex::module m = p->pc.module;
    min::uns32 i = p->pc.index;
    if ( m == min::NULL_STUB ) return i == 0;
    if ( i > m->length ) return false;
    mex::instr * pcbegin = ~ ( m + 0 );
    mex::instr * pc = ~ ( m + i );
    mex::instr * pcend = ~ ( m + m->length );

    i = p->sp;
    if ( i >= p->max_length ) return false;
    min::gen * spbegin = ~ ( p + 0 );
    min::gen * sp = ~ ( p + i );
    min::gen * spend = ~ ( p + p->max_length );

    bool result = true;

#   define CHECK1 \
	    if ( sp < spbegin \
	         || \
	         ! min::is_direct_float ( sp[0] ) ) \
	        goto ERROR_EXIT

#   define CHECK2 \
	    if ( sp < spbegin + 1 \
	         || \
	         ! min::is_direct_float ( sp[0] ) \
	         || \
	         ! min::is_direct_float ( sp[-1] ) ) \
	        goto ERROR_EXIT
#   define GF(x) min::new_direct_float_gen ( x )
#   define FG(x) min::unprotected::direct_float_of ( x )

    while ( true )
    {
        if ( pc == pcend ) goto EXIT;
	if ( limit == 0 ) goto ERROR_EXIT;
	switch ( pc->op_code )
	{
	case mex::ADD:
	    CHECK2;
	    sp[-1] = GF
	        ( FG ( sp[-1] ) + FG ( sp[0] ) );
	    -- sp;
	    break;
	case mex::NEG:
	    CHECK1;
	    sp[0] = GF ( - FG ( sp[0] ) );
	    break;
	case mex::PUSH:
	{
	    min::gen * q = sp - pc->immedA;
	    if ( q < spbegin || sp >= spend - 1 )
	        goto ERROR_EXIT;
	    * ++ sp = * q;
	    break;
	}
	case mex::POP:
	{
	    min::gen * q = sp - pc->immedA;
	    if ( q < spbegin ) // includes sp < spbegin
	        goto ERROR_EXIT;
	    * q = * sp --;
	    break;
	}
	}
	++ pc, -- limit;
    }

ERROR_EXIT:
    result = false;
EXIT:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
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
    NONA = 1,	// No arithmetic operands.
    A2 =   2,		// sp[0] and sp[-1] are the arithmetic
                // operands in that order.
    A2R =  3,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order.
    A2I =  4,	// sp[0] and immedD are the arithmetic
                // operands in that order.
    A2IR = 5,	// immedD and sp[0] are the arithmetic
                // operands in that order.
    A1 =   6,	// sp[0] is an arithmetic operand.
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

bool mex::run_process ( mex::process p, min::uns32 limit )
{
    const char * message;
    char message_buffer[200];

while ( true ) // Outer loop.
{
    mex::module m = p->pc.module;
    min::uns32 i = p->pc.index;
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
    mex::instr * pcbegin = ~ ( m + 0 );
    mex::instr * pc = ~ ( m + i );
    mex::instr * pcend = ~ ( m + m->length );

    i = p->sp;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large";
	goto FATAL;
    }
    min::gen * spbegin = ~ ( p + 0 );
    min::gen * sp = ~ ( p + i );
    min::gen * spend = ~ ( p + p->max_length );

    while ( true ) // Inner loop.
    {
        min::uns8 op_code = pc->op_code;
	if ( op_code > max_op_code )
	{
	    message = "Illegal op code: too large";
	    goto INNER_FATAL;
	}
	op_info * info = op_infos + op_code;
	min::gen arg1, arg2;

	switch ( info->op_type )
	{
	case NONA:
	    goto NON_ARITHMETIC;
	case A2:
	    if ( sp < spbegin + 1 )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = sp[0], arg2 = sp[-1];
	    break;
	case A2R:
	    if ( sp < spbegin + 1 )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = sp[-1], arg2 = sp[0];
	    break;
	case A2I:
	    if ( sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = sp[0], arg2 = pc->immedD;
	    break;
	case A2IR:
	    if ( sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = pc->immedD, arg2 = sp[0];
	    break;
	case A1:
	    if ( sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = sp[0];
	    arg2 = min::new_direct_float_gen ( 0 );
	        // To avoid error detector.
	    break;
	default:
	    message = "internal system error:"
	              " bad op_type";
	    goto INNER_FATAL;
	}

	// Process arithmetic operator, including
	// conditional JMPs.
	{

	    feclearexcept ( FE_ALL_EXCEPT );

	    // TBD

	    int excepts =
	        fetestexcept ( FE_ALL_EXCEPT );
	    p->excepts_accumulator |= excepts;
	    excepts &= p->excepts;
	    if ( excepts != 0 )
	    {
	        if ( excepts & FE_INVALID )
		    message = "invalid operand(s)";
	        else if ( excepts & FE_DIVBYZERO )
		    message = "divide by zero";
	        else if ( excepts & FE_OVERFLOW )
		    message = "numeric overflow";
	        else if ( excepts & FE_INEXACT )
		    message = "inexact numeric result";
	        else if ( excepts & FE_UNDERFLOW )
		    message = "numeric underflow";
		else
		    message =
		        "unknown numeric exception";
		// TBD
	    }

	    continue; // inner loop
	}

    NON_ARITHMETIC:
        ;

#   define CHECK1 \
	    if ( sp < spbegin ) \
	    { \
	        message = "illegal SP: too small"; \
		goto FATAL; \
	    } \
	    if ( ! min::is_direct_float ( sp[0] ) ) \
	    { \
	        message = \
		    "illegal argument: not float64"; \
		goto FATAL; \
	    }

#   define CHECK2 \
	    if ( sp < spbegin + 1 \
	         || \
	         ! min::is_direct_float ( sp[0] ) \
	         || \
	         ! min::is_direct_float ( sp[-1] ) ) \
	        goto FATAL
#   define GF(x) min::new_direct_float_gen ( x )
#   define FG(x) min::unprotected::direct_float_of ( x )

    }

    continue; // outer loop

INNER_FATAL:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
    goto FATAL;

RESTART:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
    break; // inner loop
}


FATAL:
    return false;
}
