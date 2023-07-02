// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun Jul  2 18:32:21 EDT 2023
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

min::locatable_var<min::printer> mex::default_printer;


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
    J2 =   7,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order, and the
		// operation is a jump.
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
	min::float64 arg1, arg2;
	bool jmp = false;

#	define F(x) * (min::float64 *) & (x)
	    // If x is a min::gen that is not a direct
	    // float, it will be a signalling NaN that
	    // will raise the invalid exception and
	    // generate a non-signalling NaN result.

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
	    arg1 = F ( sp[0] ), arg2 = F ( sp[-1] );
	    goto ARITHMETIC;
	case A2R:
	    if ( sp < spbegin + 1 )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = F ( sp[-1] ), arg2 = F ( sp[0] );
	    goto ARITHMETIC;
	case A2I:
	    if ( sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = F ( sp[0] ), arg2 = F ( pc->immedD );
	    goto ARITHMETIC;
	case A2IR:
	    if ( sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = F ( pc->immedD ), arg2 = F ( sp[0] );
	    goto ARITHMETIC;
	case A1:
	    if ( sp < spbegin )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = F ( sp[0] );
	    arg2 = 0; // To avoid error detector.
	    goto ARITHMETIC;
	case J2:
	    if ( sp < spbegin + 1 )
	    {
	        message = "illegal SP: too small";
		goto INNER_FATAL;
	    }
	    arg1 = F ( sp[0] ), arg2 = F ( sp[-1] );
	    goto JUMP;
	default:
	    message = "internal system error:"
	              " bad op_type";
	    goto INNER_FATAL;
	}
#	undef F

	ARITHMETIC:
	{
	    // Process arithmetic operator, excluding
	    // JMPs.

	    feclearexcept ( FE_ALL_EXCEPT );

	    switch ( op_code )
	    {
	    case mex::ADD:
	        * -- sp = min::new_num_gen
		              ( arg1 + arg2 );
		break;
	    case mex::ADDI:
	        * sp = min::new_num_gen
		              ( arg1 + arg2 );
	    }


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

	JUMP:
	{
	    // Process conditional JMP.

	    bool execute_jmp, bad_jmp;
	    if ( ! std::isnan ( arg1 )
	         &&
		 ! std::isnan ( arg2 ) )
		bad_jmp = true;
	    else if ( std::isinf ( arg1 )
	              &&
		      std::isinf ( arg2 )
		      &&
		      arg1 * arg2 > 0 )
		bad_jmp = true;
	    else
	    {
	        bad_jmp = false;
		switch ( op_code )
		{
		case mex::JMPEQ:
		    execute_jmp = ( arg1 == arg2 );
		}
	    }

	    // TBD


	    continue; // inner loop
	}

	NON_ARITHMETIC:
        {
	}

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

    } // end inner loop

RESTART:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
    continue; // outer loop

// Fatal error discovered in inner loop.
//
INNER_FATAL:
    * (min::uns32 *) & p->pc.index = pc - pcbegin;
    p->sp = sp - spbegin;
    p->length = p->sp + 1;
    goto FATAL;
}

// Come here with fatal error `message'.  At this point
// there is no instruction to pin the blame on - its a
// process state error - which can only happen if the
// compiler has made a mistake.
//
FATAL:
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
	op_info * info = ( op_code <= max_op_code ?
	                   op_infos + op_code :
			   NULL );
	if ( info != NULL )
	    q += sprintf ( q, "%s", info->name );
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

}
