// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat Jul  1 18:21:34 EDT 2023
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
    A2IR = 5,	// immedD and sp[0 are the arithmetic
                // operands in that order.
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
    const char * message = NULL;

    mex::module m = p->pc.module;
    min::uns32 i = p->pc.index;
    if ( m == min::NULL_STUB )
    {
        if ( i == 0 ) return true;
	message = "Illegal PC: no module and index > 0";
	goto ERROR_EXIT;
    }
    if ( i > m->length )
    {
	message = "Illegal PC: index too large";
	goto ERROR_EXIT;
    }
    mex::instr * pcbegin = ~ ( m + 0 );
    mex::instr * pc = ~ ( m + i );
    mex::instr * pcend = ~ ( m + m->length );

    i = p->sp;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large";
	goto ERROR_EXIT;
    }
    min::gen * spbegin = ~ ( p + 0 );
    min::gen * sp = ~ ( p + i );
    min::gen * spend = ~ ( p + p->max_length );

#   define CHECK1 \
	    if ( sp < spbegin ) \
	    { \
	        message = "illegal SP: too small"; \
		goto ERROR_EXIT; \
	    } \
	    if ( ! min::is_direct_float ( sp[0] ) ) \
	    { \
	        message = \
		    "illegal argument: not float64"; \
		goto ERROR_EXIT; \
	    }

#   define CHECK2 \
	    if ( sp < spbegin + 1 \
	         || \
	         ! min::is_direct_float ( sp[0] ) \
	         || \
	         ! min::is_direct_float ( sp[-1] ) ) \
	        goto ERROR_EXIT
#   define GF(x) min::new_direct_float_gen ( x )
#   define FG(x) min::unprotected::direct_float_of ( x )
}
