// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Jun 30 23:51:41 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Program Instructions


// Setup
// -----

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
    if ( i > p->length ) return false;
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
	    if ( q < spbegin || sp >= spend )
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
    }

ERROR_EXIT:
    result = false;
EXIT:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
    return result;
}

