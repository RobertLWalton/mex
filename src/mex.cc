// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Fri Oct 18 01:59:53 AM EDT 2024
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Support Functions
//	Optimized Run Process
//	Run Process
//	Create Functions
//	Init Functions


// Setup
// -----

# include <cstdlib>
# include <cstdio>
# include <cfenv>
# include <cmath>
# include <iostream>
# include <mex.h>
# define MUP min::unprotected

# define RW_UNS32 * (min::uns32 *) &
    // Change `const min::uns32' to `min::uns32'.

min::locatable_var<min::printer> mex::default_printer;
min::uns32 mex::trace_indent = 2;
char mex::trace_mark = '*';

static min::uns32 instr_gen_disp[] =
{
    min::DISP ( & mex::instr::immedD ),
    min::DISP_END
};

static min::uns32 module_gen_disp[] =
{
    min::DISP ( & mex::module_header::name ),
    min::DISP ( & mex::module_header::interface ),
    min::DISP_END
};

static min::uns32 module_stub_disp[] =
{
    min::DISP ( & mex::module_header::position ),
    min::DISP ( & mex::module_header::globals ),
    min::DISP ( & mex::module_header::trace_info ),
    min::DISP_END
};

static min::packed_vec<mex::instr,mex::module_header>
     module_vec_type
         ( "module_vec_type",
	   ::instr_gen_disp,
	   NULL,
	   ::module_gen_disp,
	   ::module_stub_disp );

static min::uns32 process_element_gen_disp[] =
{
    0, min::DISP_END
};

static min::uns32 process_stub_disp[] =
{
    min::DISP ( & mex::process_header::printer ),
    mex::DISP ( & mex::process_header::pc ),
    min::DISP ( & mex::process_header::return_stack ),
    min::DISP_END
};

static min::packed_vec<min::gen,mex::process_header>
     process_vec_type
         ( "process_vec_type",
	   ::process_element_gen_disp,
	   NULL,
	   NULL,
	   ::process_stub_disp );
    
static min::uns32 return_stack_element_stub_disp[] =
{
    mex::DISP ( & mex::ret::saved_pc ),
    min::DISP_END
};

static min::packed_vec<mex::ret>
     return_stack_vec_type
         ( "return_stack_vec_type",
	   NULL,
	   ::return_stack_element_stub_disp );

static min::gen error_func
    ( min::gen arg1, min::gen arg2 )
{
    std::feraiseexcept ( FE_INVALID );
    return mex::NOT_A_NUMBER ;
}
mex::op_func mex::error_func = ::error_func;

min::locatable_gen mex::ZERO;
min::locatable_gen mex::NOT_A_NUMBER;
min::locatable_gen mex::FALSE;
min::locatable_gen mex::TRUE;
static min::locatable_gen STAR;
static void check_op_infos ( void );
static void check_trace_class_infos ( void );
static void check_state_infos ( void );
static void initialize ( void )
{
    mex::ZERO = min::new_num_gen ( 0 );
    mex::NOT_A_NUMBER = min::new_num_gen ( NAN );
    mex::FALSE = min::new_str_gen ( "FALSE" );
    mex::TRUE = min::new_str_gen ( "TRUE" );
    ::STAR = min::new_str_gen ( "*" );

    check_op_infos();
    check_trace_class_infos();
    check_state_infos();
}
static min::initializer initializer ( ::initialize );

// Support Functions
//
struct pvar
{
    min::gen value;
    pvar ( min::gen value ) : value ( value ) {}
};

inline min::printer operator <<
    ( min::printer p, const ::pvar & pvar )
{
    if ( pvar.value == ::STAR )
        return p << "*";
    else
        return p << min::pgen_name ( pvar.value );
}

void mex::print_excepts
    ( min::printer printer, int excepts,
                            int highlight )
{
    const char * prefix = "";
    for ( int i = 0; i < mex::NUMBER_OF_EXCEPTS; ++ i )
    {
        mex::except_info & ei = mex::except_infos[i];
	if ( excepts & ei.mask )
	{
	    printer << prefix;
	    prefix = " ";
	    if ( highlight & ei.mask )
	        printer << "*";
	    printer << ei.name;
	}
    }
}

static bool excepts_test ( mex::process p )
{
    int flags =
        p->excepts_mask & p->excepts_accumulator;
    if ( flags == 0 ) return true;
    p->printer
        << min::bom
        << "PROCESS TERMINATION EXCEPTS ERROR(S): "
        << min::place_indent ( 0 );
    mex::print_excepts ( p->printer, flags );
    p->printer << min::eom;
    p->state = mex::EXCEPTS_ERROR;
    return false;
}
        

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
    const mex::instr * pcbegin =
        ~ min::begin_ptr_of ( m );
    const mex::instr * pc = pcbegin + i;
    const mex::instr * pcend = pcbegin + m->length;

    i = p->length;
    if ( i > p->max_length ) return false;
    min::gen * spbegin = ~ min::begin_ptr_of ( p );
    min::gen * sp = spbegin + i;
    min::gen * spend = spbegin + p->max_length;

    min::uns32 limit = p->counter_limit;
    if ( p->counter >= limit ) return false;
    limit -= p->counter;

    bool result = true;
    feclearexcept ( FE_ALL_EXCEPT );

    min::gen arg1, arg2;

#   define CHECK1 \
	    if ( sp < spbegin + 1 ) \
	        goto ERROR_EXIT; \
	    arg1 = sp[-1]; \
	    if ( ! min::is_num ( arg1 ) ) \
	        goto ERROR_EXIT

#   define CHECK1I \
	    if ( sp < spbegin + 1 ) \
	        goto ERROR_EXIT; \
	    arg1 = sp[-1]; \
	    arg2 = pc->immedD; \
	    if ( ! min::is_num ( arg1 ) ) \
	        goto ERROR_EXIT; \
	    if ( ! min::is_num ( arg2 ) ) \
	        goto ERROR_EXIT

#   define CHECK1RI \
	    if ( sp < spbegin + 1 ) \
	        goto ERROR_EXIT; \
	    arg1 = pc->immedD; \
	    arg2 = sp[-1]; \
	    if ( ! min::is_num ( arg1 ) ) \
	        goto ERROR_EXIT; \
	    if ( ! min::is_num ( arg2 ) ) \
	        goto ERROR_EXIT

#   define CHECK2 \
	    if ( sp < spbegin + 2 ) \
	        goto ERROR_EXIT; \
	    arg1 = sp[-2]; arg2 = sp[-1]; \
	    if ( ! min::is_num ( arg1 ) ) \
	        goto ERROR_EXIT; \
	    if ( ! min::is_num ( arg2 ) ) \
	        goto ERROR_EXIT
#   define CHECK2R \
	    if ( sp < spbegin + 2 ) \
	        goto ERROR_EXIT; \
	    arg1 = sp[-1]; arg2 = sp[-2]; \
	    if ( ! min::is_num ( arg1 ) ) \
	        goto ERROR_EXIT; \
	    if ( ! min::is_num ( arg2 ) ) \
	        goto ERROR_EXIT
#   define GF(x) min::new_direct_float_gen ( x )
#   define FG(x) MUP::direct_float_of ( x )

#   define A1F(f) \
	    CHECK1; \
	    sp[-1] = GF ( f ( FG ( sp[-1] ) ) ); \
	    break;

    while ( true )
    {
        if ( pc == pcend ) goto EXIT;
	if ( limit == 0 ) goto ERROR_EXIT;
	min::uns8 op_code = pc->op_code;
	switch ( op_code )
	{
	case mex::ADD:
	    CHECK2;
	    sp[-2] = GF ( FG ( arg1 ) + FG ( arg2 ) );
	    -- sp;
	    break;
	case mex::ADDI:
	    CHECK1I;
	    sp[-1] = GF ( FG ( arg1 ) + FG ( arg2 ) );
	    break;
	case mex::MUL:
	    CHECK2;
	    sp[-2] = GF ( FG ( arg1 ) * FG ( arg2 ) );
	    -- sp;
	    break;
	case mex::MULI:
	    CHECK1I;
	    sp[-1] = GF ( FG ( arg1 ) * FG ( arg2 ) );
	    break;
	case mex::SUB:
	    CHECK2;
	    sp[-2] = GF ( FG ( arg1 ) - FG ( arg2 ) );
	    -- sp;
	    break;
	case mex::SUBI:
	    CHECK1I;
	    sp[-1] = GF ( FG ( arg1 ) - FG ( arg2 ) );
	    break;
	case mex::SUBR:
	    CHECK2R;
	    sp[-2] = GF ( FG ( arg1 ) - FG ( arg2 ) );
	    -- sp;
	    break;
	case mex::SUBRI:
	    CHECK1RI;
	    sp[-1] = GF ( FG ( arg1 ) - FG ( arg2 ) );
	    break;
	case mex::DIV:
	    CHECK2;
	    sp[-2] = GF ( FG ( arg1 ) / FG ( arg2 ) );
	    -- sp;
	    break;
	case mex::DIVI:
	    CHECK1I;
	    sp[-1] = GF ( FG ( arg1 ) / FG ( arg2 ) );
	    break;
	case mex::DIVR:
	    CHECK2R;
	    sp[-2] = GF ( FG ( arg1 ) / FG ( arg2 ) );
	    -- sp;
	    break;
	case mex::DIVRI:
	    CHECK1RI;
	    sp[-1] = GF ( FG ( arg1 ) / FG ( arg2 ) );
	    break;
	case mex::MOD:
	    CHECK2;
	    sp[-2] = GF ( fmod ( FG ( arg1 ),
		                 FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::MODI:
	    CHECK1I;
	    sp[-1] = GF ( fmod ( FG ( arg1 ),
		                 FG ( arg2 ) ) );
	    break;
	case mex::MODR:
	    CHECK2R;
	    sp[-2] = GF ( fmod ( FG ( arg1 ),
		                 FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::MODRI:
	    CHECK1RI;
	    sp[-1] = GF ( fmod ( FG ( arg1 ),
		                 FG ( arg2 ) ) );
	    break;
	case mex::POW:
	    CHECK2;
	    sp[-2] = GF ( pow ( FG ( arg1 ),
	                        FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::POWI:
	    CHECK1I;
	    sp[-1] = GF ( pow ( FG ( arg1 ),
	                        FG ( arg2 ) ) );
	    break;
	case mex::POWR:
	    CHECK2R;
	    sp[-2] = GF ( pow ( FG ( arg1 ),
	                        FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::POWRI:
	    CHECK1RI;
	    sp[-1] = GF ( pow ( FG ( arg1 ),
	                        FG ( arg2 ) ) );
	    break;
	case mex::FLOOR:
	    A1F ( floor );
	case mex::CEIL:
	    A1F ( ceil );
	case mex::ROUND:
	    A1F ( rint );
	case mex::TRUNC:
	    A1F ( trunc );
	case mex::NEG:
	    CHECK1;
	    sp[-1] = GF ( - FG ( arg1 ) );
	    break;
	case mex::ABS:
	    A1F ( fabs );
	case mex::LOG:
	    A1F ( log );
	case mex::LOG10:
	    A1F ( log10 );
	case mex::EXP:
	    A1F ( exp );
	case mex::EXP10:
	    A1F ( exp10 );
	case mex::SIN:
	    A1F ( sin );
	case mex::COS:
	    A1F ( cos );
	case mex::TAN:
	    A1F ( tan );
	case mex::ASIN:
	    A1F ( asin );
	case mex::ACOS:
	    A1F ( acos );
	case mex::ATAN:
	    A1F ( atan );
	case mex::ATAN2:
	    CHECK2;
	    sp[-2] = GF ( atan2 ( FG ( arg1 ),
	                          FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::ATAN2R:
	    CHECK2R;
	    sp[-2] = GF ( atan2 ( FG ( arg1 ),
	                          FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::PUSHS:
	{
	    min::uns32 i = pc->immedA;
	    if ( sp >= spend || i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp = sp[-(int)i-1];
	    ++ sp;
	    break;
	}
	case mex::PUSHI:
	{
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    sp = mex::process_push
	        ( p, sp, pc->immedD );
	    break;
	}
	case mex::PUSHG:
	{
	    min::uns32 i = pc->immedA;
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
	    sp = mex::process_push
	        ( p, sp, globals[i] );
	    break;
	}
	case mex::PUSHL:
	{
	    min::uns32 i = pc->immedA;
	    min::uns32 j = pc->immedB;
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    if (    j == 0
	         && m->globals != min::NULL_STUB )
	    {
		if ( i >= m->globals->length )
		    goto ERROR_EXIT;
		* sp ++ = m->globals[i];
		break;
	    }
	    if ( j > p->level )
	        goto ERROR_EXIT;
	    i += p->fp[j];
	    if ( i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp ++ = spbegin[i];
	    break;
	}
	case mex::PUSHA:
	{
	    min::uns32 i = pc->immedA;
	    min::uns32 j = pc->immedB;
	    if ( j < 1 || j > p->level )
	        goto ERROR_EXIT;
	    min::uns32 k = p->fp[j];
	    if ( i < 1 || i > p->nargs[j] )
	        goto ERROR_EXIT;
	    k -= i;
	    if ( i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp ++ = spbegin[k];
	    break;
	}
	case mex::PUSHNARGS:
	{
	    min::uns32 immedB = pc->immedB;
	    if ( immedB < 1 || immedB > p->level )
	        goto ERROR_EXIT;
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    * sp ++ = GF ( p->nargs[immedB] );
	    break;
	}
	case mex::PUSHV:
	{
	    min::uns32 j = pc->immedB;
	    if ( j < 1 || j > p->level )
	        goto ERROR_EXIT;
	    if ( sp <= spbegin )
	        goto ERROR_EXIT;
	    min::float64 f = FG ( sp[-1] );
	    min::float64 ff = floor ( f );
	    if ( std::isnan ( ff )
	         ||
	         f != ff
		 ||
	         ff < 1 || ff > p->nargs[j] )
	    {
	        sp[-1] = GF ( NAN );
		feraiseexcept ( FE_INVALID );
	    }
	    else
	    {
		min::uns32 i = (int) ff;
		min::uns32 k = p->fp[j] - p->nargs[j];
		sp[-1] = spbegin[k + i - 1];
	    }
	    break;
	}
	case mex::POPS:
	{
	    min::uns32 i = pc->immedA;
	    if ( sp <= spbegin || i >= sp - spbegin )
	        goto ERROR_EXIT;
	    -- sp;
	    sp[-(int)i] = * sp;
	    break;
	}
	case mex::JMP:
	    goto EXECUTE_JMP;
	case mex::JMPEQ:
	{
	    if ( sp < spbegin + 2 )
		goto ERROR_EXIT;
	    arg1 = sp[-2]; arg2 = sp[-1];
	    sp -= 2;
	    if ( min::is_num ( arg1 )
	         &&
		 min::is_num ( arg2 ) )
	    {
	        if ( FG ( arg1 ) == FG ( arg2 ) )
		    goto EXECUTE_JMP;
		    // So +0.00 == -0.00
	    }
	    else if ( arg1 == arg2 ) goto EXECUTE_JMP;
	    if ( pc->immedB > 0 )
	    {
	    	sp[0] = sp[1];
		++ sp;
	    }
	    break;
	}
	case mex::JMPNEQ:
	{
	    if ( sp < spbegin + 2 )
		goto ERROR_EXIT;
	    arg1 = sp[-2]; arg2 = sp[-1];
	    sp -= 2;
	    if ( min::is_num ( arg1 )
	         &&
		 min::is_num ( arg2 ) )
	    {
	        if ( FG ( arg1 ) != FG ( arg2 ) )
		    goto EXECUTE_JMP;
		    // So +0.00 == -0.00
	    }
	    else if ( arg1 != arg2 ) goto EXECUTE_JMP;
	    if ( pc->immedB > 0 )
	    {
	    	sp[0] = sp[1];
		++ sp;
	    }
	    break;
	}
	case mex::JMPLT:
	case mex::JMPLEQ:
	case mex::JMPGT:
	case mex::JMPGEQ:
	{
	    CHECK2;
	    sp -= 2;

	    min::float64 farg1 = FG ( arg1 );
	    min::float64 farg2 = FG ( arg2 );
	    if ( std::isnan ( farg1 )
		 ||
		 std::isnan ( farg2 )
		 ||
		 (    std::isinf ( farg1 )
		   && std::isinf ( farg2 )
		   && farg1 * farg2 > 0 ) )
		goto ERROR_EXIT;

	    switch ( op_code )
	    {
	    case mex::JMPLT:
		if ( farg1 < farg2 ) goto EXECUTE_JMP;
		break;
	    case mex::JMPLEQ:
		if ( farg1 <= farg2 ) goto EXECUTE_JMP;
		break;
	    case mex::JMPGT:
		if ( farg1 > farg2 ) goto EXECUTE_JMP;
		break;
	    case mex::JMPGEQ:
		if ( farg1 >= farg2 ) goto EXECUTE_JMP;
		break;
	    }
	    if ( pc->immedB > 0 )
	    {
	    	sp[0] = sp[1];
		++ sp;
	    }
	    break;
	}
	case mex::JMPCNT:
	{
	    if ( pc->immedB >= sp - spbegin )
	        goto ERROR_EXIT;
	    min::gen & arg = sp[-(int)pc->immedB-1];
	    min::float64 farg = FG ( arg );
	    if ( ! std::isfinite ( farg ) )
	        goto ERROR_EXIT;
	    min::float64 immedD = FG ( pc->immedD );
	    if ( ! std::isfinite ( immedD ) )
	        goto ERROR_EXIT;
	    if ( farg <= 0 )
	        goto EXECUTE_JMP;
	    arg = min::new_num_gen ( farg - immedD );
	    break;
	}
	case mex::JMPTRUE:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( arg != mex::TRUE
		 &&
		 arg != mex::FALSE )
	        goto ERROR_EXIT;
	    if ( pc->immedB == 0 )
	    {
	        if ( arg == mex::TRUE )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
	        if ( arg == mex::FALSE )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::JMPINT:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::float64 farg = FG ( * -- sp );
	    if ( std::isfinite ( farg )
		 &&
		 farg < +1e15
		 &&
		 farg > -1e15
		 &&
		 (min::int64) farg == farg )
	    {
		if ( pc->immedB == 0 )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
		if ( pc->immedB > 0 )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::JMPFIN:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::float64 farg = FG ( * -- sp );
	    if ( std::isfinite ( farg ) )
	    {
		if ( pc->immedB == 0 )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
		if ( pc->immedB > 0 )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::JMPINF:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::float64 farg = FG ( * -- sp );
	    if ( std::isinf ( farg ) )
	    {
		if ( pc->immedB == 0 )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
		if ( pc->immedB > 0 )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::JMPNUM:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( min::is_num ( arg ) )
	    {
		if ( pc->immedB == 0 )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
		if ( pc->immedB > 0 )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::JMPSTR:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( min::is_str ( arg ) )
	    {
		if ( pc->immedB == 0 )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
		if ( pc->immedB > 0 )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::JMPOBJ:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( min::is_obj ( arg ) )
	    {
		if ( pc->immedB == 0 )
		    goto EXECUTE_JMP;
	    }
	    else
	    {
		if ( pc->immedB > 0 )
		    goto EXECUTE_JMP;
	    }
	    break;
	}
	case mex::BEG:
	    ++ p->trace_depth;
	    goto nop;
	case mex::END:
	    if ( p->trace_depth == 0 )
	        goto ERROR_EXIT;
	    -- p->trace_depth;
	    /* FALLTHRU */
	nop:
	case mex::NOP:
	{
	    min::uns32 i = pc->immedA;
	    if ( i > sp - spbegin )
	        goto ERROR_EXIT;
	    sp -= i;
	    break;
	}
	case mex::BEGL:
	{
	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedB = pc->immedB;
	    if ( immedA + immedB < immedA )
	        goto ERROR_EXIT;
	    if ( immedA + immedB > sp - spbegin )
	        goto ERROR_EXIT;
	    if ( sp + immedB > spend + immedA )
	        goto ERROR_EXIT;
	    sp -= (int) immedA;
	    min::gen * q1 = sp - (int) immedB;
	    min::gen * q2 = sp;
	    while ( q1 < q2 )
		* sp ++ = * q1 ++;
	    ++ p->trace_depth;
	    break;
	}
	case mex::ENDL:
	case mex::CONT:
	{
	    if ( min::pending() )
	        goto ERROR_EXIT;
	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedB = pc->immedB;
	    min::uns32 immedC = pc->immedC;
	    if ( immedA > sp - spbegin )
	        goto ERROR_EXIT;
	    if (   immedA + 2 * immedB
		 > sp - spbegin )
	        goto ERROR_EXIT;
	    if ( immedC + 1 > pc - pcbegin )
	        goto ERROR_EXIT;
	    sp -= (int) immedA;
	    for ( int i = immedB; 0 < i; -- i )
	        sp[-(int)immedB-i] = sp[-i];
	    pc -= immedC;
	    -- pc;
	    break;
	}
	case mex::SET_TRACE:
	case mex::TRACE:
	case mex::WARN:
	case mex::ERROR:
	case mex::SET_EXCEPTS:
	case mex::TRACE_EXCEPTS:
	case mex::SET_OPTIMIZE:
	    goto ERROR_EXIT;
	case mex::BEGF:
	{
	    min::uns32 immedB = pc->immedB;
	    min::uns32 immedC = pc->immedC;
	    if ( immedC > pc - pcbegin )
	        goto ERROR_EXIT;
	    if ( immedB > p->level + 1 )
	        goto ERROR_EXIT;
	    if ( immedB > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    pc += immedC;
	    -- pc;
	    break;
	}
	case mex::ENDF:
	{
	    min::uns32 immedB = pc->immedB;
	    if ( immedB != p->level )
	        goto ERROR_EXIT;
	    min::uns32 rp = p->return_stack->length;
	    if ( rp == 0 )
	        goto ERROR_EXIT;
	    -- rp;
	    const mex::ret * ret =
	       ~ ( p->return_stack + rp );

	    min::gen * new_sp =
	        spbegin + p->fp[immedB]
		        - (int) p->nargs[immedB];
	    min::uns32 new_fp = ret->saved_fp;
	    if ( new_fp > new_sp - spbegin )
		goto ERROR_EXIT;

	    mex::module em = ret->saved_pc.module;
	    min::uns32 new_pc = ret->saved_pc.index;
	    if ( em == min::NULL_STUB )
	    {
	        if ( new_pc != 0 )
		    goto ERROR_EXIT;
	    }
	    else
	    {
	        if ( new_pc > em->length )
		    goto ERROR_EXIT;
	    }
	    if ( p->trace_depth == 0 )
	        goto ERROR_EXIT;

	    mex::set_pc ( p, ret->saved_pc );
	    p->level = ret->saved_level;
	    p->fp[immedB] = new_fp;
	    p->nargs[immedB] = ret->saved_nargs;
	    RW_UNS32 p->return_stack->length = rp;

	    sp = new_sp;

	    -- p->trace_depth;

	    if ( em == min::NULL_STUB )
	        goto RET_EXIT;

            m = em;
	    pcbegin = ~ min::begin_ptr_of ( m );
	    pc = pcbegin + new_pc;
	    pcend = pcbegin + m->length;
	    break;
	}
	case mex::RET:
	{
	    min::uns32 immedB = pc->immedB;
	    min::uns32 immedC = pc->immedC;
	    if ( immedB != p->level )
	        goto ERROR_EXIT;
	    min::uns32 rp = p->return_stack->length;
	    if ( rp == 0 )
	        goto ERROR_EXIT;
	    -- rp;
	    const mex::ret * ret =
	       ~ ( p->return_stack + rp );
	    if ( immedC != ret->nresults )
	        goto ERROR_EXIT;

	    min::gen * new_sp =
	        spbegin + p->fp[immedB]
		        - (int) p->nargs[immedB];
	    min::uns32 new_fp = ret->saved_fp;
	    if ( new_fp > new_sp - spbegin )
		goto ERROR_EXIT;
	    if ( immedC > sp - new_sp )
	        goto ERROR_EXIT;

	    mex::module em = ret->saved_pc.module;
	    min::uns32 new_pc = ret->saved_pc.index;
	    if ( em == min::NULL_STUB )
	    {
	        if ( new_pc != 0 )
		    goto ERROR_EXIT;
	    }
	    else
	    {
	        if ( new_pc > em->length )
		    goto ERROR_EXIT;
	    }
	    if ( p->trace_depth == 0 )
	        goto ERROR_EXIT;

	    mex::set_pc ( p, ret->saved_pc );
	    p->level = ret->saved_level;
	    p->fp[immedB] = new_fp;
	    p->nargs[immedB] = ret->saved_nargs;
	    RW_UNS32 p->return_stack->length = rp;

	    min::gen * qend = sp;
	    min::gen * q = qend - (int) immedC;
	    while ( q < qend )
	        * new_sp ++ = * q ++;
	    sp = new_sp;

	    -- p->trace_depth;

	    if ( em == min::NULL_STUB )
	        goto RET_EXIT;

            m = em;
	    pcbegin = ~ min::begin_ptr_of ( m );
	    pc = pcbegin + new_pc;
	    pcend = pcbegin + m->length;
	    break;
	}
	case mex::CALLM:
	case mex::CALLG:
	{
	    min::uns32 immedC = pc->immedC;
	    mex::module cm =
	        ( op_code == mex::CALLG ?
		  pc->immedD :
		  m );
	    if ( cm == min::NULL_STUB )
	        goto ERROR_EXIT;
	    if ( immedC >= cm->length )
	        goto ERROR_EXIT;
	    const mex::instr * target =
	        ~ ( cm + immedC );
	    if ( target->op_code != mex::BEGF )
	        goto ERROR_EXIT;
	    min::uns32 level = target->immedB;
	    if ( level > p->level + 1 )
	        goto ERROR_EXIT;
	    if ( pc->immedA < target->immedA )
	        goto ERROR_EXIT;
	    min::uns32 rp = p->return_stack->length;
	    if ( rp >= p->return_stack->max_length )
	        goto ERROR_EXIT;

	    mex::ret * ret =
	        ~ min::begin_ptr_of ( p->return_stack )
		+ rp;
	    mex::pc new_pc =
	        { m, (min::uns32) ( pc - pcbegin ) };
	    mex::set_saved_pc ( p, ret, new_pc );
	    ret->saved_level = p->level;
	    ret->saved_fp = p->fp[level];
	    ret->saved_nargs = p->nargs[level];
	    p->level = level;
	    p->fp[level] = ( sp - spbegin );
	    p->nargs[level] = pc->immedA;
	    ret->nresults = pc->immedB;
	    RW_UNS32 p->return_stack->length = rp + 1;

	    new_pc = { cm, immedC };
	    mex::set_pc ( p, new_pc );
	    m = cm;
	    pcbegin = ~ min::begin_ptr_of ( m );
	    pc = pcbegin + immedC;
	    pcend = pcbegin + m->length;

	    ++ p->trace_depth;
	    break;
	}
	default:
	    goto ERROR_EXIT;

	} // end switch ( op_code )

	++ pc, -- limit;
	continue;

    EXECUTE_JMP:
        {
	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedC = pc->immedC;
	    if ( immedA > sp - spbegin
		 ||
		 immedC > pcend - pc
		 ||
		 immedC == 0 )
	        goto ERROR_EXIT;

	    if ( pc->trace_depth > p->trace_depth )
	        goto ERROR_EXIT;

	    p->trace_depth -= pc->trace_depth;
	    sp = sp - (int) immedA;
	    pc += immedC; -- limit;
	}
    }

ERROR_EXIT:
    result = false;
EXIT:
    RW_UNS32 p->pc.index = pc - pcbegin;
RET_EXIT:
    RW_UNS32 p->length = sp - spbegin;
    p->excepts_accumulator |= 
	fetestexcept ( FE_ALL_EXCEPT );
    p->optimized_counter += p->counter_limit - limit
                          - p->counter;
    p->counter = p->counter_limit - limit;
    return result;

#   undef CHECK1
#   undef CHECK2
#   undef FG
#   undef GF
#   undef A1F
}


// Run Process
// --- -------

min::uns32 mex::run_stack_limit = 1 << 14;
min::uns32 mex::run_return_stack_limit = 1 << 12;
min::uns32 mex::run_counter_limit = 1 << 20;
min::uns32 mex::run_trace_flags = 1 << mex::T_ALWAYS;
int mex::run_excepts_mask =
    FE_DIVBYZERO|FE_INVALID|FE_OVERFLOW;
bool mex::run_optimize = false;

// Operation Information Table.
//
mex::op_info mex::op_infos [ mex::NUMBER_OF_OP_CODES ] =
{   { 0, 0, T_ALWAYS, mex::error_func, "NONE", "" },
    { mex::ADD, A2, T_AOP, mex::error_func,
      "ADD", "+" },
    { mex::ADDI, A2I, T_AOP, mex::error_func,
      "ADDI", "+" },
    { mex::MUL, A2, T_AOP, mex::error_func,
      "MUL", "*" },
    { mex::MULI, A2I, T_AOP, mex::error_func,
      "MULI", "*" },
    { mex::SUB, A2, T_AOP, mex::error_func,
      "SUB", "-" },
    { mex::SUBR, A2R, T_AOP, mex::error_func,
      "SUBR", "-" },
    { mex::SUBI, A2I, T_AOP, mex::error_func,
      "SUBI", "-" },
    { mex::SUBRI, A2RI, T_AOP, mex::error_func,
      "SUBRI", "-" },
    { mex::DIV, A2, T_AOP, mex::error_func,
      "DIV", "/" },
    { mex::DIVR, A2R, T_AOP, mex::error_func,
      "DIVR", "/" },
    { mex::DIVI, A2I, T_AOP, mex::error_func,
      "DIVI", "/" },
    { mex::DIVRI, A2RI, T_AOP, mex::error_func,
      "DIVRI", "/" },
    { mex::MOD, A2, T_AOP, mex::error_func,
      "MOD", "fmod" },
    { mex::MODR, A2R, T_AOP, mex::error_func,
      "MODR", "fmod" },
    { mex::MODI, A2I, T_AOP, mex::error_func,
      "MODI", "fmod" },
    { mex::MODRI, A2RI, T_AOP, mex::error_func,
      "MODRI", "fmod" },
    { mex::POW, A2, T_AOP, mex::error_func,
      "POW", "pow" },
    { mex::POWR, A2R, T_AOP, mex::error_func,
      "POWR", "pow" },
    { mex::POWI, A2I, T_AOP, mex::error_func,
      "POWI", "pow" },
    { mex::POWRI, A2RI, T_AOP, mex::error_func,
      "POWRI", "pow" },
    { mex::FLOOR, A1, T_AOP, mex::error_func,
      "FLOOR", "floor" },
    { mex::CEIL, A1, T_AOP, mex::error_func,
      "CEIL", "ceil" },
    { mex::TRUNC, A1, T_AOP, mex::error_func,
      "TRUNC", "trunc" },
    { mex::ROUND, A1, T_AOP, mex::error_func,
      "ROUND", "round" },
    { mex::NEG, A1, T_AOP, mex::error_func,
      "NEG", "-" },
    { mex::ABS, A1, T_AOP, mex::error_func,
      "ABS", "abs" },
    { mex::LOG, A1, T_AOP, mex::error_func,
      "LOG", "log" },
    { mex::LOG10, A1, T_AOP, mex::error_func,
      "LOG10", "log10" },
    { mex::EXP, A1, T_AOP, mex::error_func,
      "EXP", "exp" },
    { mex::EXP10, A1, T_AOP, mex::error_func,
      "EXP10", "exp10" },
    { mex::SIN, A1, T_AOP, mex::error_func,
      "SIN", "sin" },
    { mex::ASIN, A1, T_AOP, mex::error_func,
      "ASIN", "asin" },
    { mex::COS, A1, T_AOP, mex::error_func,
      "COS", "cos" },
    { mex::ACOS, A1, T_AOP, mex::error_func,
      "ACOS", "acos" },
    { mex::TAN, A1, T_AOP, mex::error_func,
      "TAN", "tan" },
    { mex::ATAN, A1, T_AOP, mex::error_func,
      "ATAN", "atan" },
    { mex::ATAN2, A2, T_AOP, mex::error_func,
      "ATAN2", "atan2" },
    { mex::ATAN2R, A2R, T_AOP, mex::error_func,
      "ATAN2R", "atan2" },
    { mex::PUSHS, NONA, T_PUSH, NULL, "PUSHS", NULL },
    { mex::PUSHL, NONA, T_PUSH, NULL, "PUSHL", NULL },
    { mex::PUSHI, NONA, T_PUSH, NULL, "PUSHI", NULL },
    { mex::PUSHG, NONA, T_PUSH, NULL, "PUSHG", NULL },
    { mex::POPS, NONA, T_POP, NULL, "POPS", NULL },
    { mex::JMP, J, T_JMP, NULL, "JMP", NULL },
    { mex::JMPEQ, J2, T_JMPS, NULL, "JMPEQ", "==" },
    { mex::JMPNEQ, J2, T_JMPS, NULL, "JMPNEQ", "!=" },
    { mex::JMPLT, J2, T_JMPS, mex::error_func,
      "JMPLT", "<" },
    { mex::JMPLEQ, J2, T_JMPS, mex::error_func,
      "JMPLEQ", "<=" },
    { mex::JMPGT, J2, T_JMPS, mex::error_func,
      "JMPGT", ">" },
    { mex::JMPGEQ, J2, T_JMPS, mex::error_func,
      "JMPGEQ", ">=" },
    { mex::JMPCNT, JS, T_JMPS, NULL, "JMPCNT", NULL },
    { mex::JMPTRUE, J1, T_JMPS, NULL, "JMPTRUE", NULL },
    { mex::JMPINT, J1, T_JMPS, NULL, "JMPINT", NULL },
    { mex::JMPFIN, J1, T_JMPS, NULL, "JMPFIN", NULL },
    { mex::JMPINF, J1, T_JMPS, NULL, "JMPINF", NULL },
    { mex::JMPNUM, J1, T_JMPS, NULL, "JMPNUM", NULL },
    { mex::JMPSTR, J1, T_JMPS, NULL, "JMPSTR", NULL },
    { mex::JMPOBJ, J1, T_JMPS, NULL, "JMPOBJ", NULL },
    { mex::BEG, NONA, T_BEG, NULL, "BEG", NULL },
    { mex::NOP, NONA, T_NOP, NULL, "NOP", NULL },
    { mex::END, NONA, T_END, NULL, "END", NULL },
    { mex::BEGL, NONA, T_BEGL, NULL, "BEGL", NULL },
    { mex::ENDL, NONA, T_ENDL, NULL, "ENDL", NULL },
    { mex::CONT, NONA, T_CONT, NULL, "CONT", NULL },
    { mex::BEGF, NONA, T_BEGF, NULL, "BEGF", NULL },
    { mex::ENDF, NONA, T_ENDF, NULL, "ENDF", NULL },
    { mex::CALLM, NONA, T_CALLM, NULL, "CALLM", NULL },
    { mex::CALLG, NONA, T_CALLG, NULL, "CALLG", NULL },
    { mex::RET, NONA, T_RET, NULL, "RET", NULL },
    { mex::PUSHA, NONA, T_PUSH, NULL, "PUSHA", NULL },
    { mex::PUSHNARGS, NONA, T_PUSH, NULL,
      "PUSHNARGS", NULL },
    { mex::PUSHV, A1, T_PUSH, NULL, "PUSHV", "pushv" },
    { mex::SET_TRACE, NONA, T_ALWAYS,
                      NULL, "SET_TRACE", NULL },
    { mex::TRACE, NONA, T_ALWAYS, NULL, "TRACE", NULL },
    { mex::WARN, NONA, T_ALWAYS, NULL, "WARN", NULL },
    { mex::ERROR, NONA, T_ALWAYS, NULL, "ERROR", NULL },
    { mex::SET_EXCEPTS, NONA, T_SET_EXCEPTS,
                        NULL, "SET_EXCEPTS", NULL },
    { mex::TRACE_EXCEPTS, NONA, T_ALWAYS,
                          NULL, "TRACE_EXCEPTS", NULL },
    { mex::SET_OPTIMIZE, NONA, T_SET_OPTIMIZE,
                         NULL, "SET_OPTIMIZE", NULL },
};

// Trace Class Information Table.
//
mex::trace_class_info mex::trace_class_infos
	[ mex::NUMBER_OF_TRACE_CLASSES ] =
{
    { T_NEVER, "NEVER" },
    { T_ALWAYS, "ALWAYS" },
    { T_AOP, "AOP" },
    { T_PUSH, "PUSH" },
    { T_POP, "POP" },
    { T_JMP, "JMP" },
    { T_JMPS, "JMPS" },
    { T_JMPF, "JMPF" },
    { T_BEG, "BEG" },
    { T_END, "END" },
    { T_BEGL, "BEGL" },
    { T_ENDL, "ENDL" },
    { T_CONT, "CONT" },
    { T_BEGF, "BEGF" },
    { T_ENDF, "ENDF" },
    { T_CALLM, "CALLM" },
    { T_CALLG, "CALLG" },
    { T_RET, "RET" },
    { T_NOP, "NOP" },
    { T_SET_EXCEPTS, "SET_EXCEPTS" },
    { T_SET_OPTIMIZE, "SET_OPTIMIZE" },
};

// Excepts Information Table.
//
mex::except_info mex::except_infos
	[ mex::NUMBER_OF_EXCEPTS ] =
{
    { FE_DIVBYZERO, "DIVBYZERO" },
    { FE_INEXACT, "INEXACT" },
    { FE_INVALID, "INVALID" },
    { FE_OVERFLOW, "OVERFLOW" },
    { FE_UNDERFLOW, "UNDERFLOW" }
};

// State Information Table.
//
mex::state_info mex::state_infos
	[ mex::NUMBER_OF_STATES ] =
{
    { NEVER_STARTED, "NEVER_STARTED",
                     "program never started" },
    { RUNNING, "RUNNING",
               "program still running" },
    { MODULE_END, "MODULE_END",
                  "module initialization terminated"
		  " normally" },
    { CALL_END, "CALL_END",
                "function call terminated normally" },
    { COUNTER_LIMIT_STOP, "COUNTER_LIMIT_STOP",
    		  "instruction counter reached limit" },
    { STACK_LIMIT_STOP, "STACK_LIMIT_STOP",
    		  "stack size reached limit" },
    { RETURN_STACK_LIMIT_STOP,
                  "RETURN_STACK_LIMIT_STOP",
    		  "return stack size reached limit" },
    { ERROR_STOP, "ERROR_STOP",
                  "ERROR instruction executed" },
    { JMP_ERROR, "JMP_ERROR",
                 "invalid operand to JMP instruction" },
    { FORMAT_ERROR, "FORMAT_ERROR",
                    "bad program format" },
    { EXCEPTS_ERROR, "EXCEPTS_ERROR",
                     "illegal exception was raised"
		     " during otherwise successful"
		     " program execution" }
};

static void check_op_infos ( void )
{
    mex::op_info * p = mex::op_infos;
    mex::op_info * endp = p + mex::NUMBER_OF_OP_CODES;
    while  ( p < endp )
    {
        if ( p - mex::op_infos != p->op_code )
	{
	    std::cerr << "BAD OP_INFOS["
	              << p - mex::op_infos
	              << "] != " << p->op_code
		      << std::endl;
	    std::exit ( 1 );
	}
	++ p;
    }
}

static void check_trace_class_infos ( void )
{
    mex::trace_class_info * p =
        mex::trace_class_infos;
    mex::trace_class_info * endp =
        p + mex::NUMBER_OF_TRACE_CLASSES;
    while  ( p < endp )
    {
        if (    p - mex::trace_class_infos
	     != p->trace_class )
	{
	    std::cerr << "BAD TRACE_CLASS_INFOS["
	              << p - mex::trace_class_infos
	              << "] != " << p->trace_class
		      << std::endl;
	    std::exit ( 1 );
	}
	++ p;
    }
}

static void check_state_infos ( void )
{
    mex::state_info * p = mex::state_infos;
    mex::state_info * endp =
        p + mex::NUMBER_OF_STATES;
    while  ( p < endp )
    {
        if ( p - mex::state_infos != p->state )
	{
	    std::cerr << "BAD STATE_INFOS["
	              << p - mex::state_infos
	              << "] != " << p->state
		      << std::endl;
	    std::exit ( 1 );
	}
	++ p;
    }
}

inline min::printer print_header
	( mex::process p,
	  min::phrase_position pp,
	  int sp_change )
{
    p->printer << min::bom;

    unsigned i = p->trace_depth % 10;
    unsigned j = ( i + 1 ) * mex::trace_indent;
    while ( 1 < j )
    {
	p->printer << mex::trace_mark;
	-- j;
    }
    p->printer << ' ';

    p->printer << "{";

    if ( pp )
	p->printer
	    << pp.end.line
	           // First line is 1 because
		   // pp.end.line is line after
		   // phrase.
	    << ":";

    p->printer << p->pc.index
	       << ","
	       << p->length + sp_change
	       << ","
	       << p->counter + 1
	       << "}";
    p->printer << min::place_indent ( 1 );
    return p->printer;
}

bool mex::run_process ( mex::process p )
{
    mex::module m = p->pc.module;
    min::uns32 i = p->pc.index;
    const mex::instr * pcbegin;
    const mex::instr * pc;
    const mex::instr * pcend;
    min::gen * spbegin;
    min::gen * sp;
    min::gen * spend;
    const char * message;
    min::uns32 limit;

    min::uns8 op_code;
    min::uns8 trace_class;
    op_info * op_info;
    min::gen arg1, arg2;
    min::locatable_gen result;
    int sp_change;
    min::uns32 trace_flags; 

    if ( m == min::NULL_STUB && i != 0 )
    {
	message = "Illegal PC: no module and index > 0";
	goto FATAL;
    }
    if ( i > m->length )
    {
	message = "Illegal PC: PC index greater than"
	          " module length";
	goto FATAL;
    }
    pcbegin = ~ min::begin_ptr_of ( m );
    pc = pcbegin + i;
    pcend = pcbegin + m->length;

    i = p->length;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large; process"
	          " length > process max_length";
	goto FATAL;
    }
    spbegin = ~ min::begin_ptr_of ( p );
    sp = spbegin + i;
    spend = spbegin + p->max_length;

    limit = p->counter_limit;
    if ( p->counter >= limit )
    {
        p->state = mex::COUNTER_LIMIT_STOP;
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
	RW_UNS32 p->pc.index = pc - pcbegin; \
	RW_UNS32 p->length = sp - spbegin; \
	p->counter = p->counter_limit - limit;

#   define RET_SAVE \
	RW_UNS32 p->length = sp - spbegin; \
	p->counter = p->counter_limit - limit;

#   define RESTORE \
	pcbegin = ~ min::begin_ptr_of ( m ); \
	pc = pcbegin + p->pc.index; \
	pcend = pcbegin + m->length; \
	spbegin = ~ min::begin_ptr_of ( p ); \
	sp = spbegin + p->length; \
	spend = spbegin + p->max_length; \
        limit = p->counter_limit - p->counter;

    p->state = mex::RUNNING;

    while ( true ) // Inner loop.
    {
        if ( pc == pcend )
	{
	    SAVE;
	    p->state = mex::MODULE_END;
	    return excepts_test ( p );
	}
	if ( limit == 0 )
	{
	    SAVE;
	    p->state = mex::COUNTER_LIMIT_STOP;
	    return false;
	}

	if ( p->optimize )
	{
	    SAVE;
	    if ( optimized_run_process ( p ) )
	    {
	        if ( p->pc.module == min::NULL_STUB )
		    p->state = mex::CALL_END;
		else
		    p->state = mex::MODULE_END;
	        return excepts_test ( p );
	    }
	    min::interrupt();
	    mex::module om = p->pc.module;
	    min::uns32 oi = p->pc.index;
	    if ( om == min::NULL_STUB )
	    {
		if ( oi == 0 )
		{
		    p->state = mex::CALL_END;
		    return excepts_test ( p );
		}
		message = "Illegal PC: no module and"
		          " index > 0";
		goto FATAL;
	    }
	    if ( oi >= m->length )
	    {
		if ( oi == m->length )
		{
		    p->state = mex::MODULE_END;
		    return excepts_test ( p );
		}
		message = "Illegal PC: PC index greater"
			  " than module length";
		goto FATAL;
	    }

	    if ( p->counter >= p->counter_limit )
	    {
		p->state = mex::COUNTER_LIMIT_STOP;
		return false;
	    }

	    RESTORE;
	}

        op_code = pc->op_code;
	trace_class = pc->trace_class;
	sp_change = 0;
	trace_flags = p->trace_flags; 

	if ( op_code >= mex::NUMBER_OF_OP_CODES )
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
	    sp_change = -1;
	    if ( sp - 2 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-2];
	    arg2 = sp[-1];
	    goto ARITHMETIC;
	case A2R:
	    sp_change = -1;
	    if ( sp - 2 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = sp[-2];
	    goto ARITHMETIC;
	case A2I:
	    if ( sp - 1 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = pc->immedD;
	    goto ARITHMETIC;
	case A2RI:
	    if ( sp - 1 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = pc->immedD;
	    arg2 = sp[-1];
	    goto ARITHMETIC;
	case A1:
	    if ( sp - 1 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = mex::ZERO;
	        // To avoid error detector.
	    goto ARITHMETIC;
	case J1:
	    sp_change = -1;
	    if ( sp - 1 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-1];
	    goto JUMP;
	case JS:
	    if ( pc->immedB >= sp - spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-(int)pc->immedB-1];
	    goto JUMP;
	case J2:
	    sp_change = -2;
	    if ( sp - 2 < spbegin )
	        goto STACK_TOO_SMALL;
	    arg1 = sp[-2];
	    arg2 = sp[-1];
	    goto JUMP;
	case J:
	    goto JUMP;
	default:
	    message = "internal system error:"
	              " bad op_type";
	    goto INNER_FATAL;
	}

	ARITHMETIC:
	    // Process arithmetic operator, excluding
	    // JMPs.

	    feclearexcept ( FE_ALL_EXCEPT );

	    if ( ! min::is_num ( arg1 )
	         ||
		 ! min::is_num ( arg2 ) )
	    {
	        result = mex::op_infos[op_code].op_func
			    ( arg1, arg2 );
		goto ARITHMETIC_TRACE;
	    }

	{
	    min::float64 fresult;

	    min::float64 farg1 = FG ( arg1 );
	    min::float64 farg2 = FG ( arg2 );


	    switch ( op_code )
	    {
	    case mex::ADD:
	    case mex::ADDI:
	        fresult = farg1 + farg2;
		break;
	    case mex::MUL:
	    case mex::MULI:
	        fresult = farg1 * farg2;
		break;
	    case mex::SUB:
	    case mex::SUBI:
	    case mex::SUBR:
	    case mex::SUBRI:
	        fresult = farg1 - farg2;
		break;
	    case mex::DIV:
	    case mex::DIVI:
	    case mex::DIVR:
	    case mex::DIVRI:
	        fresult = farg1 / farg2;
		break;
	    case mex::MOD:
	    case mex::MODI:
	    case mex::MODR:
	    case mex::MODRI:
	        fresult = fmod ( farg1, farg2 );
		break;
	    case mex::POW:
	    case mex::POWI:
	    case mex::POWR:
	    case mex::POWRI:
	        fresult = pow ( farg1, farg2 );
		break;
	    case mex::FLOOR:
	        fresult = floor ( farg1 );
		break;
	    case mex::CEIL:
	        fresult = ceil ( farg1 );
		break;
	    case mex::ROUND:
	        fresult = rint ( farg1 );
		break;
	    case mex::TRUNC:
	        fresult = trunc ( farg1 );
		break;
	    case mex::NEG:
	        fresult = - farg1;
		break;
	    case mex::ABS:
	        fresult = fabs ( farg1 );
		break;
	    case mex::LOG:
	        fresult = log ( farg1 );
		break;
	    case mex::LOG10:
	        fresult = log10 ( farg1 );
		break;
	    case mex::EXP:
	        fresult = exp ( farg1 );
		break;
	    case mex::EXP10:
	        fresult = exp10 ( farg1 );
		break;
	    case mex::SIN:
	        fresult = sin ( farg1 );
		break;
	    case mex::COS:
	        fresult = cos ( farg1 );
		break;
	    case mex::TAN:
	        fresult = tan ( farg1 );
		break;
	    case mex::ASIN:
	        fresult = asin ( farg1 );
		break;
	    case mex::ACOS:
	        fresult = acos ( farg1 );
		break;
	    case mex::ATAN:
	        fresult = atan ( farg1 );
		break;
	    case mex::ATAN2:
	    case mex::ATAN2R:
	        fresult = atan2 ( farg1, farg2 );
		break;
	    case mex::PUSHV:
	    {
		min::uns32 j = pc->immedB;
		if ( j < 1 || j > p->level )
		{
		    message = "PUSHV immedB is 0 or"
		              " greater than current"
			      " lexical level";
		    goto INNER_FATAL;
		}
		min::float64 ff = floor ( farg1 );
		if ( std::isnan ( ff )
		     ||
		     farg1 != ff
		     ||
		     ff < 1 || ff > p->nargs[j] )
		{
		    result = NOT_A_NUMBER;
		    feraiseexcept ( FE_INVALID );
		}
		else
		{
		    min::uns32 i = (min::uns32) ff;
		    min::uns32 k =
		        p->fp[j] - p->nargs[j];
		    result = spbegin[k + i - 1];
		}
		break;
	    }
	    } // end switch ( op_code )

	    if ( op_code != mex::PUSHV )
		result = min::new_num_gen ( fresult );
	}

	ARITHMETIC_TRACE:

	{
	    int excepts =
	        fetestexcept ( FE_ALL_EXCEPT );
	    p->excepts_accumulator |= excepts;
	    excepts &= p->excepts_mask;

	    if (    excepts != 0
	         || trace_flags & (1 << trace_class) )
	    {
	        SAVE;

		if ( excepts != 0 )
		{
		    p->printer << min::bol
			       << "!!! ERROR: ";
		    if ( excepts & FE_INVALID )
			p->printer
			    << "invalid operand(s)";
		    else if ( excepts & FE_DIVBYZERO )
			p->printer
			    << "divide by zero";
		    else if ( excepts & FE_OVERFLOW )
			p->printer
			    << "numeric overflow";
		    else if ( excepts & FE_INEXACT )
			p->printer
			    << "inexact numeric result";
		    else if ( excepts & FE_UNDERFLOW )
			p->printer
			    << "numeric underflow";
		    else
			p->printer <<
			    "unknown numeric exception";
		    p->printer << min::eol;
		}

		min::phrase_position pp =
		    p->pc.index < m->position->length ?
		    m->position[p->pc.index] :
		    min::MISSING_PHRASE_POSITION;

		print_header ( p, pp, sp_change )
		    << " " << op_info->name << ": ";

		if ( m->trace_info != min::NULL_STUB  )
		{
		    min::gen trace_info =
		        m->trace_info[p->pc.index];
		    p->printer << ::pvar ( trace_info );
		}
		else
		    p->printer << "*";

		if ( op_code == mex::PUSHV )
		    p->printer
		        << " <= sp[fp["
			<< pc->immedB
			<< "]-nargs["
			<< pc->immedB
			<< "]+"
			<< min::pgen ( arg1 )
			<< "-1] = "
			<< min::pgen ( result );

		else if ( op_info->op_type == mex::A1 )
		    p->printer
		        << " = "
			<< min::pgen ( result )
			<< " <= "
			<< op_info->oper
			<< " "
			<< min::pgen ( arg1 );

		else
		    p->printer
		        << " = "
			<< min::pgen ( result )
			<< " <= "
			<< min::pgen ( arg1 )
			<< " "
			<< op_info->oper
			<< " "
			<< min::pgen ( arg2 );

	        RESTORE;
	    }

	    sp += sp_change;
	    sp[-1] = result;

	    goto LOOP;
	}

	JUMP:
	{
	    // Process JMP.

	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedB = 0;
	        // Left 0 for JMP, JMPTRUE, ..., JMPCNT
	    min::uns32 immedC = pc->immedC;

	    if ( immedA > ( sp + sp_change ) - spbegin )
	    {
	        message = "JMP immedA is greater than"
		          " stack size";
	        goto INNER_FATAL;
	    }
	    if ( pc + immedC > pcend )
	    {
	        message = "JMP immedC too large; target"
		          " is beyond module end";
	        goto INNER_FATAL;
	    }
	    if ( immedC == 0 )
	    {
	        message = "JMP immedC == 0; illegal";
	        goto INNER_FATAL;
	    }

	    bool bad_jmp = false;
	    bool execute_jmp = true;
	    if ( op_code == mex::JMP )
	        /* do nothing */;
	    else if ( op_code == mex::JMPCNT )
	    {
		min::float64 immedD = FG ( pc->immedD );
		if ( ! std::isfinite ( immedD ) )
		{
		    message = "JMPCNT immedD is not"
			      " a finite number";
		    goto INNER_FATAL;
		}
		min::float64 farg1 = FG ( arg1 );
		min::uns32 i = pc->immedB;
		if ( ! std::isfinite ( farg1 ) )
		    bad_jmp = true;
		else if ( farg1 > 0 )
		{
		    execute_jmp = false;
		    sp[-(int)i-1] = min::new_num_gen
		        ( farg1 - immedD );
		}
	    }
	    else if ( op_info->op_type == mex::J1 )
	    {
	        switch ( op_code )
		{
		case mex::JMPTRUE:
		{
		    if ( arg1 != mex::TRUE
		         &&
			 arg1 != mex::FALSE )
		        bad_jmp = true;
		    else
		        execute_jmp =
			    ( arg1 == mex::TRUE );
		    break;
		}
		case mex::JMPINT:
		{
		    min::float64 farg = FG ( arg1 );
		    execute_jmp =
		        ( std::isfinite ( farg )
			  &&
			  farg < +1e15
			  &&
			  farg > -1e15
			  &&
			  (min::int64) farg == farg );
		    break;
		}
		case mex::JMPFIN:
		    execute_jmp =
		        std::isfinite ( FG ( arg1 ) );
		    break;
		case mex::JMPINF:
		    execute_jmp =
		        std::isinf ( FG ( arg1 ) );
		    break;
		case mex::JMPNUM:
		    execute_jmp = min::is_num ( arg1 );
		    break;
		case mex::JMPSTR:
		    execute_jmp = min::is_str ( arg1 );
		    break;
		case mex::JMPOBJ:
		    execute_jmp = min::is_obj ( arg1 );
		    break;
		}
		if ( pc->immedB > 0 )
		    execute_jmp = ! execute_jmp;
	    }
	    else if ( ! min::is_num ( arg1 )
	              ||
		      ! min::is_num ( arg2 ) )
	    {
		immedB = pc->immedB;
		if ( immedB > 1 )
		{
		    message = "JMP immedB != 0 or 1;"
		              " illegal ";
		    goto INNER_FATAL;
		}
	        if ( op_code == mex::JMPEQ )
		    execute_jmp = ( arg1 == arg2 );
	        else if ( op_code == mex::JMPNEQ )
		    execute_jmp = ( arg1 != arg2 );
		else
		{
		    min::gen r =
		        ( mex::op_infos[op_code]
			      .op_func )
			( arg1, arg2 );
		    if ( r == mex::TRUE )
		        execute_jmp = true;
		    else if ( r == mex::FALSE )
		        execute_jmp = false;
		    else bad_jmp = true;
		}
	    }
	    else
	    {
		min::float64 farg1 = FG ( arg1 );
		min::float64 farg2 = FG ( arg2 );

		immedB = pc->immedB;
		if ( immedB > 1 )
		{
		    message = "JMP immedB != 0 or 1;"
		              " illegal ";
		    goto INNER_FATAL;
		}

		if ( std::isnan ( farg1 )
		     ||
		     std::isnan ( farg2 )
		     ||
		     (    std::isinf ( farg1 )
		       && std::isinf ( farg2 )
		       && farg1 * farg2 > 0 ) )
		    bad_jmp = true;
		else switch ( op_code )
		{
		case mex::JMPEQ:
		    execute_jmp = ( farg1 == farg2 );
		    break;
		case mex::JMPNEQ:
		    execute_jmp = ( farg1 != farg2 );
		    break;
		case mex::JMPLT:
		    execute_jmp = ( farg1 < farg2 );
		    break;
		case mex::JMPLEQ:
		    execute_jmp = ( farg1 <= farg2 );
		    break;
		case mex::JMPGT:
		    execute_jmp = ( farg1 > farg2 );
		    break;
		case mex::JMPGEQ:
		    execute_jmp = ( farg1 >= farg2 );
		    break;
		}
	    }

	    if ( execute_jmp
	         &&
		 pc->trace_depth > p->trace_depth )
	    {
	        message = "if JMP were executed,"
		          " trace_depth would become"
		          " negative";
	        goto INNER_FATAL;
	    }

	    min::uns8 jmp_trace_class = trace_class;
	    if ( ! execute_jmp
	         &&
		 trace_class == mex::T_JMPS )
	        ++ jmp_trace_class;

	    if ( execute_jmp )
	        sp_change -= (int) immedA;
	    else if ( immedB == 1 )
	        sp_change = -1;

	    if ( bad_jmp
	         ||
		 trace_flags & (1 << jmp_trace_class ) )
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

		min::uns32 index = p->pc.index;
		min::phrase_position pp =
		    index < m->position->length ?
		    m->position[index] :
		    min::MISSING_PHRASE_POSITION;

		print_header ( p, pp, sp_change )
		    << " " << op_info->name;
		if ( (& m[index])->immedB )
		    p->printer << "*";

		min::gen trace_info = min::MISSING();
		if ( m->trace_info != min::NULL_STUB
		     &&
		     index < m->trace_info->length )
		    trace_info = m->trace_info[index];
		if ( min::is_name ( trace_info )
		     &&
		     ! min::is_num ( trace_info ) )
		    p->printer << " "
			       << ::pvar ( trace_info );
		else
		    p->printer
		        << " location "
		        << index
			   +
			   (& m[index])->immedC;

		if ( bad_jmp )
		{
		    p->printer << " with invalid"
		                  " operand(s)"
			       << min::eom;
		    p->state = mex::JMP_ERROR;
		    return false;
		}

		if ( op_code != mex::JMP )
		{
		    if ( execute_jmp )
			p->printer
			    << " is successful: true";
		    else if ( ! bad_jmp )
			p->printer
			    << " is UNsuccessful:"
			       " false";
		    p->printer << " <= ";

		    if ( op_code == mex::JMPCNT )
		        p->printer
			    << min::pgen ( arg1 )
			    << " <= 0";
		    else if ( op_info->oper == NULL )
		        p->printer
			    << min::pgen ( arg1 );
		    else
		    {
			p->printer
			    << min::pgen ( arg1 )
			    << " "
			    << op_info->oper
			    << " "
			    << min::pgen ( arg2 );
		    }
		}

		p->printer << min::eom;

		RESTORE;
	    }

	    if ( ! execute_jmp )
	    {
	        if ( immedB == 1 ) sp[-2] = sp[-1];
	    }
	    else
	    {
		p->trace_depth -= pc->trace_depth;
		pc += immedC;
		-- pc;
	    }
	    sp += sp_change;

	    goto LOOP;
	}

	NON_ARITHMETIC:
        {
	    // Process instructions that push and
	    // pop.

	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedB = pc->immedB;
	    min::uns32 immedC = pc->immedC;
	    min::gen immedD = pc->immedD;

	    min::gen value;
	        // Value to be pushed or popped.
		// Computed early for tracing.

	    min::uns32 location = pc - pcbegin;
	    min::gen tinfo  = min::MISSING();
	    if (   location
		 < m->trace_info->length )
		tinfo = m->trace_info[location];
		// CALL.../RET/ENDF replace this
		// before tracing is executed.
	    min::uns8 to_op_code = mex::_NONE_;
		// CALL.../RET/ENDF replace this
		// before tracing is executed.
	     

	    // Pre-trace check for fatal errors.
	    // Also compute value for PUSH... and
	    // POP... .
	    //
	    switch ( op_code )
	    {
	    case mex::PUSHS:
	        if ( immedA + 1 > sp - spbegin )
		{
		    message = "PUSHS immedA too large;"
		              " variable location is"
			      " before stack beginning";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		value = sp[-(int)immedA-1];
		sp_change = +1;
		break;
	    case mex::PUSHI:
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		value = immedD;
		sp_change = +1;
		break;
	    case mex::PUSHG:
	    {
	        mex::module mg = (mex::module) immedD;
		if ( mg == min::NULL_STUB )
		{
		    message = "PUSHG immedD is not a"
		              " module";
		    goto INNER_FATAL;
		}
		if ( mg->globals == min::NULL_STUB )
		{
		    message = "PUSHG module has no"
		              " globals";
		    goto INNER_FATAL;
		}
	        if ( immedA >= mg->globals->length  )
		{
		    message = "PUSHG immedA equal to"
		              " or larger than the"
			      " number of globals in"
			      " the module";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		value = mg->globals[immedA];
		sp_change = +1;
		break;
	    }
	    case mex::PUSHL:
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		if (    immedB == 0
		     && m->globals != min::NULL_STUB )
		{
		    if ( immedA >= m->globals->length )
		    {
			message = "PUSHL with"
			          " immedB = 0 and"
				  " current module"
				  " globals set; immedA"
				  " is equal to or"
				  " larger than the"
				  " number of globals"
				  " in the module";
			goto INNER_FATAL;
		    }
		    value = m->globals[immedA];
		    break;
		}

		if ( immedB > p->level )
		{
		    message = "PUSHL immedB larger than"
		              " current lexical level";
		    goto INNER_FATAL;
		}
		if (    p->fp[immedB] + immedA
		     >= sp - spbegin )
		{
		    message = "PUSHL immedA too large;"
		              " addresses variable"
			      " beyond the end of the"
			      " stack";
		    goto INNER_FATAL;
		}
		value = spbegin[p->fp[immedB] + immedA];
		sp_change = +1;
		break;
	    case mex::PUSHA:
		if ( immedB < 1 || immedB > p->level )
		{
		    message = "PUSHA: invalid immedB; 0"
		              " or greater than current"
			      " lexical level";
		    goto INNER_FATAL;
		}
		if (    immedA < 1
		     || immedA > p->nargs[immedB] )
		{
		    message = "PUSHA immedA out of"
		              " range; addresses"
			      " non-extant argument";
		    goto INNER_FATAL;
		}
		value = spbegin
		    [p->fp[immedB] - (int) immedA];
		sp_change = +1;
		break;
	    case mex::PUSHNARGS:
		if ( immedB < 1 || immedB > p->level )
		{
		    message = "PUSHNARGS: invalid"
		              " immedB; 0 or greater"
			      " than current lexical"
			      " level";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		value = min::new_num_gen
		    ( p->nargs[immedB] );
		sp_change = +1;
		break;
	    case mex::POPS:
		if ( sp <= spbegin )
		{
		    message =
		        "POP: cannot pop an empty"
			" stack";
		    goto INNER_FATAL;
		}
	        if ( immedA >= sp - spbegin )
		{
		    message = "POP: immedA equal to"
		              " or larger than stack"
		              " length";
		    goto INNER_FATAL;
		}
		value = sp[-1];
		sp_change = -1;
		break;
	    case mex::END:
	        if ( p->trace_depth == 0 )
		{
		    message = "END: trace depth would"
		              " become negative if"
			      " instruction was"
			      " executed";
		    goto INNER_FATAL;
		}
		/* FALLTHRU */
	    case mex::BEG:
	    case mex::NOP:
	    {
		if ( immedA > sp - spbegin )
		{
		    message = "BEG/NOP/END: immedA"
                              " larger than stack"
			      " length";
		    goto INNER_FATAL;
		}
		sp_change = - (int) immedA;
		break;
	    }
	    case mex::BEGL:
	        if ( immedA + immedB < immedA
		     ||
	             immedA + immedB > sp - spbegin )
		{
		    message = "BEGL: immedA+immedB"
		              " larger than stack size";
		    goto INNER_FATAL;
		}
		if ( sp + immedB > spend + immedA )
		    goto STACK_LIMIT_STOP;
		sp_change = - (int) immedA + immedB;
	        break;
	    case mex::ENDL:
	    case mex::CONT:
	    {
	        if ( immedA > sp - spbegin )
		{
		    message = "ENDL/CONT: immedA larger"
		              " than stack size";
		    goto INNER_FATAL;
		}
	        if (    2 * immedB < immedB
		     || immedA + 2 * immedB < immedA
		     ||   immedA + 2 * immedB
		        > sp - spbegin )
		{
		    message =
		        "ENDL/CONT: immedA + 2 * immedB"
			" larger than stack size";
		    goto INNER_FATAL;
		}
		min::uns32 location = pc - pcbegin;
	        if ( immedC + 1 > location )
		{
		    message =
		        "ENDL/CONT: immedC too large;"
			" associated BEGL non-extant";
		    goto INNER_FATAL;
		}

		// ENDL/CONT uses BEGL trace_info.
		//
		if (   immedC
		     < m->trace_info->length )
		    tinfo = m->trace_info
		        [location - immedC - 1];
		else
		    tinfo  = min::MISSING();

		// ENDL/CONT updates stack before trace.
		//
		sp -= immedA;
		for ( int i = immedB; 0 < i; -- i )
		    sp[-(int)immedB-i] = sp[-i];

		break;
	    }

	    case mex::SET_TRACE:
	    case mex::SET_EXCEPTS:
	    case mex::SET_OPTIMIZE:
	        break;
	    case mex::TRACE:
	    case mex::TRACE_EXCEPTS:
	    case mex::WARN:
	    case mex::ERROR:
		if ( immedA > sp - spbegin )
		{
		    message =
		        "immedA larger than stack size";
		    goto INNER_FATAL;
		}
		sp_change = - (int) immedA;
	        break;
	    case mex::BEGF:
	        if ( immedC > pcend - pc )
		{
		    message = "BEGF: immedC too large;"
		              " target address beyond"
			      " module length";
		    goto INNER_FATAL;
		}
		if (    immedB != p->level + 1 )
		{
		    message = "BEGF: immedB != current"
		              " lexical level + 1"; 
		    goto INNER_FATAL;
		}
	        if ( immedB > mex::max_lexical_level )
		{
		    message = "BEGF: immedB larger than"
		              " max_lexical_level";
		    goto INNER_FATAL;
		}
		break;
	    case mex::ENDF:
		immedC = 0;
		// Fall through
	    case mex::RET:
	    {
		if ( immedB != p->level )
		{
		    message = "RET/ENDF: immedB !="
		              " current lexical level";
		    goto INNER_FATAL;
		}
		min::uns32 rp = p->return_stack->length;
		if ( rp == 0 )
		{
		    message = "RET/ENDF: return stack"
		              " is empty";
		    goto INNER_FATAL;
		}
		-- rp;
		const mex::ret * ret =
		   ~ ( p->return_stack + rp );
		if ( immedC != ret->nresults )
		{
		    message =
		        ( op_code == mex::ENDF ?
			  "ENDF: return stack nresults"
			  " is not zero" :
			  "RET: immedC != return stack"
		              " nresults" );
		    goto INNER_FATAL;
		}

	        min::gen * new_sp =
		    spbegin + p->fp[immedB]
		            - (int) p->nargs[immedB];

		if (  immedC > sp - new_sp )
		{
		    // Not possible for ENDF.
		    message =
		        "RET: immedC is"
			" larger than portion of stack"
			" at current lexical level";
		    goto INNER_FATAL;
		}
		mex::module em = ret->saved_pc.module;
		min::uns32 new_pc = ret->saved_pc.index;

		// RET/ENDF uses CALL trace_info.
		//
		if ( em == min::NULL_STUB )
		{
		    if ( new_pc != 0 )
		    {
			message = "RET/ENDF: Illegal"
			          " saved_pc:"
			          " no module and"
				  " index > 0";
			goto INNER_FATAL;
		    }
		    tinfo  = min::MISSING();
		}
		else
		{
		    if ( new_pc > em->length )
		    {
			message = "RET/ENDF: Illegal"
			          " saved_pc:"
			          " index greater than"
	                          " module length";
			goto INNER_FATAL;
		    }

		    if (   new_pc
			 < em->trace_info->length )
			tinfo = em->trace_info[new_pc];
		    else
			tinfo  = min::MISSING();

		    to_op_code =
		        (& em[new_pc])->op_code;
		}

		min::uns32 new_fp = ret->saved_fp;
		if ( new_fp > new_sp - spbegin )
		{
		    message = "RET/ENDF: saved_fp"
		              " larger than stack size";
		    goto INNER_FATAL;
		}

	        if ( p->trace_depth == 0 )
		{
		    message = "RET/ENDF: trace depth"
		              " would become negative"
			      " if instruction were"
			      " executed";
		    goto INNER_FATAL;
		}

		// RET/ENDF updates stack before trace.
		//
		min::gen * qend = sp;
		min::gen * q = qend - (int) immedC;
		while ( q < qend )
		    * new_sp ++ = * q ++;
		sp = new_sp;

		break;
	    }
	    case mex::CALLM:
	    case mex::CALLG:
	    {
		mex::module cm =
		    ( op_code == mex::CALLG ?
		      pc->immedD :
		      m );
		if ( cm == min::NULL_STUB )
		{
		    message = "CALL immedD is not a"
		              " module";
		    goto INNER_FATAL;
		}
		if ( immedC >= cm->length )
		{
		    message = "CALL immedC is equal to"
		              " or larger than module"
			      " length";
		    goto INNER_FATAL;
		}
		const mex::instr * target =
		    ~ ( cm + immedC );
		if ( target->op_code != mex::BEGF )
		{
		    message =
		        "CALL target is not a BEGF";
		    goto INNER_FATAL;
		}
		min::uns32 level = target->immedB;
		if (    level == 0
		     || level > p->level + 1 )
		{
		    message =
		        "BEGF immedB is 0 or larger"
			" than CALL lexical level + 1";
		    goto INNER_FATAL;
		}
		if ( immedA < target->immedA )
		{
		    message =
		        "CALL immedA < BEGF immedA;"
			" too few arguments"
			" provided by CALL";
		    goto INNER_FATAL;
		}
		min::uns32 rp = p->return_stack->length;
		if ( rp >= p->return_stack->max_length )
		    goto RETURN_STACK_LIMIT_STOP;

		// CALL uses BEGF trace_info.
		//
		if (   immedC
		     < cm->trace_info->length )
		    tinfo = cm->trace_info[immedC];
		else
		    tinfo  = min::MISSING();

		to_op_code = target->op_code;

		// CALL does not change sp or the stack.

		break;
	    }

	    } // end switch ( op_code )

	    if ( trace_flags & (1 << trace_class ) )
	    {
		SAVE;

		p->printer << min::bol;
		if ( op_code == mex::ERROR )
		    p->printer
		        << "!!! FATAL ERROR: "
			<< min::eol;

		min::phrase_position pp =
		    p->pc.index < m->position->length ?
		    m->position[p->pc.index] :
		    min::MISSING_PHRASE_POSITION;

		print_header ( p, pp, sp_change )
		    << " " << op_infos[op_code].name;

		switch ( op_code )
		{
		case mex::PUSHS:
		case mex::PUSHL:
		case mex::PUSHG:
		case mex::PUSHA:
		case mex::POPS:
		{
		    p->printer << ":";
		    min::lab_ptr lp ( tinfo );
		    if ( op_code != mex::PUSHG
		         &&
			 lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) == 2 )
		    {
		        p->printer << " "
			           << ::pvar ( lp[1] )
				   << " <= "
				   << ::pvar ( lp[0] )
				   << " =";
		    }
		    else if ( op_code == mex::PUSHG
		              &&
		              lp != min::NULL_STUB
		              &&
			      min::lablen ( lp ) == 3 )
		    {
		        p->printer << " "
			           << ::pvar ( lp[2] )
				   << " <= "
				   << ::pvar ( lp[0] )
				   << " in "
				   << lp[1]
				   << " =";
		    }
		    else
		        p->printer << " * <= * =";
		    p->printer << " " << value;
		    break;
		}
		case mex::PUSHI:
		case mex::PUSHNARGS:
		{
		    p->printer << ": "
			       << ::pvar ( tinfo )
			       << " <=";
		    if ( op_code == mex::PUSHNARGS )
		        p->printer << " nargs["
			           << immedB
				   << "] =";
		    p->printer << " " << value;
		    break;
		}
		case mex::SET_TRACE:
		{
		    p->printer
		        << ":"
		        << min::place_indent ( 1 );
		    for ( unsigned i = 0;
		          i < mex::
			    NUMBER_OF_TRACE_CLASSES;
			  ++ i )
		    {
		        if ( immedA & (1 << i) )
			{
			    p->printer
			        << " "
				<< mex::
				     trace_class_infos
				         [i].name;
			}
		    }
		    break;
		}
		case mex::SET_EXCEPTS:
		{
		    p->printer
		        << ": "
		        << min::place_indent ( 1 );
		    print_excepts
		        ( p->printer, immedA,
			  p->excepts_accumulator );
		    break;
		}
		case mex::TRACE_EXCEPTS:
		{
		    p->printer
		        << ": "
		        << min::place_indent ( 1 );
		    print_excepts
		        ( p->printer,
			  p->excepts_accumulator,
			  p->excepts_mask );
		    break;
		}
		case mex::SET_OPTIMIZE:
		{
		    p->printer
		        << ": "
		        << ( ( immedA & 1 ) ?
			     "ON" : "OFF" )
		        << " <= "
		        << ( p->optimize ?
			     "ON" : "OFF" );
		    break;
		}
		case mex::BEGF:
		{
		    // trace_info only used by CALL...
		    //
		    break;
		}

		default:
		{
		    if ( to_op_code != mex::_NONE_ )
		        p->printer << " to "
			           << mex::op_infos
				        [ to_op_code ]
					.name;

		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) > 0 )
		    {
			p->printer
			    << ": "
		            << min::place_indent ( 0 );

		        min::uns32 len =
			    min::lablen ( lp );
		        p->printer << ::pvar ( lp[0] );
			for ( min::uns32 i = 1;
			      i < len; ++ i )
			{
			    min::uns32 j = len - i;
			    p->printer
			        << ", "
				<< ::pvar ( lp[i] )
				<< "=";
			    if ( j <= p->length )
			        p->printer
				    << p[p->length-j];
			    else
			        p->printer << "?";
			}
		    }
		    break;
		}

		}
		p->printer << min::eom;

		if ( op_code == mex::ERROR )
		{
		    p->state = mex::ERROR_STOP;
		    return false;
		}

		RESTORE;
	    }

	    // Execute instruction.
	    //
	    switch ( op_code )
	    {
	    case mex::PUSHS:
	    case mex::PUSHA:
	    case mex::PUSHNARGS:
	    {
		* sp ++ = value;
		break;
	    }
	    case mex::PUSHI:
	    case mex::PUSHG:
	    case mex::PUSHL:
	    {
		sp = mex::process_push
		    ( p, sp, value );
		break;
	    }
	    case mex::POPS:
	    {
		-- sp;
		sp[-(int)immedA] = value;
		break;
	    }
	    case mex::BEG:
		++ p->trace_depth;
		// Fall Through
	    case mex::NOP:
		sp -= (int) immedA;
		break;
	    case mex::END:
		sp -= (int) immedA;
		-- p->trace_depth;
		break;
	    case mex::BEGL:
	    {
		sp -= (int) immedA;
		min::gen * q1 = sp - (int) immedB;
		min::gen * q2 = sp;
		while ( q1 < q2 )
		    * sp ++ = * q1 ++;
		++ p->trace_depth;
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
		pc -= immedC;
		-- pc;
		break;
	    }
	    case mex::SET_TRACE:
	        p->trace_flags =
		    immedA | ( 1 << mex::T_ALWAYS );
	        p->trace_flags &=
		    ~ ( 1 << mex::T_NEVER );
	        break;
	    case mex::SET_EXCEPTS:
	        p->excepts_mask = immedA;
	        break;
	    case mex::SET_OPTIMIZE:
	        p->optimize = ( immedA & 1 );
	        break;
	    case mex::TRACE:
	    case mex::TRACE_EXCEPTS:
	    case mex::WARN:
	    {
	        sp -= immedA;
	        break;
	    }
	    case mex::BEGF:
	    {
		pc += immedC;
		-- pc;
		break;
	    }
	    case mex::ENDF:
	    case mex::RET:
	    {
		min::uns32 rp = p->return_stack->length;
		-- rp;
		const mex::ret * ret =
		   ~ ( p->return_stack + rp );
		mex::module em = ret->saved_pc.module;
		min::uns32 new_pc = ret->saved_pc.index;

		mex::set_pc ( p, ret->saved_pc );
		p->fp[immedB] = ret->saved_fp;
		p->nargs[immedB] = ret->saved_nargs;
		RW_UNS32 p->return_stack->length = rp;

		p->level = ret->saved_level;
		-- p->trace_depth;

		m = em;
		if ( m == min::NULL_STUB )
		{
		    RET_SAVE;
		    p->state = mex::CALL_END;
		    return excepts_test ( p );
		}

		pcbegin = ~ min::begin_ptr_of ( m );
		pc = pcbegin + new_pc;
		pcend = pcbegin + m->length;
		break;
	    }
	    case mex::CALLM:
	    case mex::CALLG:
	    {
		mex::module cm =
		    ( op_code == mex::CALLG ?
		      pc->immedD :
		      m );
		const mex::instr * target =
		    ~ ( cm + immedC );
		min::uns32 level = target->immedB;
		min::uns32 rp = p->return_stack->length;
		mex::ret * ret =
		    ~ min::begin_ptr_of
		          ( p->return_stack )
		    + rp;

		mex::pc new_pc =
		    { m, (min::uns32)
			 ( pc - pcbegin ) };
		mex::set_saved_pc ( p, ret, new_pc );
		ret->saved_level = p->level;
		ret->saved_fp = p->fp[level];
		ret->saved_nargs = p->nargs[level];
		p->level = level;
		p->fp[level] = ( sp - spbegin );
		p->nargs[level] = immedA;
		ret->nresults = immedB;
		RW_UNS32 p->return_stack->length =
		    rp + 1;

		new_pc = { cm, immedC };
		mex::set_pc ( p, new_pc );

		m = cm;
		pcbegin = ~ min::begin_ptr_of ( m );
		pc = pcbegin + immedC;
		pcend = pcbegin + m->length;
		++ p->trace_depth;
		break;
	    }
	    default:
		message = "internal system error:"
			  " unrecognized NONA"
			  " instruction";
		goto INNER_FATAL;

	    } // end switch ( op_code )

	    goto LOOP;
	}

	LOOP:
	    ++ pc; -- limit;
	    continue; // loop

	STACK_LIMIT_STOP:
	    SAVE;
	    p->state = mex::STACK_LIMIT_STOP;
	    return false;

	RETURN_STACK_LIMIT_STOP:
	    SAVE;
	    p->state = mex::RETURN_STACK_LIMIT_STOP;
	    return false;

	STACK_TOO_SMALL:
	    message = "illegal SP: stack to small"
		      " for instruction";
	    goto INNER_FATAL;

	// Fatal error discovered in loop.
	//
	INNER_FATAL:
	    SAVE;
	    goto FATAL;

#	undef FG

    } // end loop

// Come here with fatal error `message'.  At this point
// there is no instruction to pin the blame on - its a
// process state error - which can only happen if the
// compiler has made a mistake.
//
FATAL:
    p->state = mex::FORMAT_ERROR;
    char fatal_buffer[100];
    char instr_buffer_1[1000];
    char instr_buffer_2[1000] = { 0 };
    char * q = instr_buffer_1;
    q += sprintf ( q, "OP CODE = " );
    if ( p->pc.module == min::NULL_STUB
         ||
	 p->pc.index >= p->pc.module->length )
	q += sprintf ( q, "<NOT AVAILABLE>" );
    else
    {
        const mex::instr * instr =
	    ~ ( p->pc.module + p->pc.index );
	op_code = instr->op_code;
	op_info =
	    ( op_code < NUMBER_OF_OP_CODES ?
	      op_infos + op_code : NULL );
	if ( op_info != NULL )
	    q += sprintf ( q, "%s", op_info->name );
	else
	    q += sprintf
	        ( q, "%u (TOO LARGE)", op_code );
	q += sprintf
	    ( q, ", TRACE CLASS = %u",
	         instr->trace_class );
	q += sprintf
	    ( q, ", TRACE DEPTH = %u,",
	         instr->trace_depth );
	q = instr_buffer_2;
	q += sprintf
	    ( q, "IMMEDA = %u", instr->immedA );
	q += sprintf
	    ( q, ", IMMEDB = %u", instr->immedB );
	q += sprintf
	    ( q, ", IMMEDC = %u", instr->immedC );
	if ( min::is_stub ( instr->immedD ) )
	{
	    mex::module im =
	        (mex::module) instr->immedD;
	    if ( im == min::NULL_STUB )
	        q += sprintf
		    ( q, ", IMMEDD = <UNKNOWN STUB>" );
	    else if ( im->position == min::NULL_STUB
	              ||
	                 im->position->file
		      == min::NULL_STUB
		      ||
		      ! min::is_str
		            ( im->position->file
			                  ->file_name )
		    )
	        q += sprintf
		    ( q, ", IMMEDD = UNNAMED MODULE" );
	    else
	    {
	        min::str_ptr sp =
		    im->position->file->file_name;
		q += sprintf
		    ( q, ", IMMEDD = MODULE %s",
		         ~ min::begin_ptr_of ( sp ) );
	    }
	}
	else
	    q += sprintf
		( q, ", IMMEDD = %.15g",
		     MUP::direct_float_of
		         ( instr->immedD ) );
    }

    p->printer << min::bol
               << "!!! FATAL PROGRAM FORMAT ERROR: "
               << min::bom
               << message
	       << min::indent
	       << "PC->MODULE = "
	       << ( p->pc.module == min::NULL_STUB ?
	            min::new_str_gen
		        ( "<NULL MODULE>"  ):
		       p->pc.module->position->file
		    == min::NULL_STUB ?
		    min::new_str_gen
		        ( "<NULL FILE>" ):
		       p->pc.module->position->file
		                   ->file_name
		    == min::MISSING() ?
		    min::new_str_gen
		        ( "<NO FILE NAME>" ):
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
	       << instr_buffer_1;
    if ( instr_buffer_2[0] != 0 )
	p->printer << min::indent << instr_buffer_2;
    p->printer << min::indent
    	       << "ARG1 = " << min::pgen ( arg1 )
	       << min::indent
    	       << "ARG2 = " << min::pgen ( arg2 )
	       << min::indent
    	       << "RESULT = " << min::pgen ( result )
	       << min::indent
	       << "STACK POINTER = " << p->length
	       << ", PROCESS MAX_LENGTH = "
	       << p->max_length
	       << min::indent
	       << "RETURN STACK POINTER = "
	       << p->return_stack->length
	       << ", RETURN STACK MAX_LENGTH = "
	       << p->return_stack->max_length
	       << min::indent
	       << "PROCESS LEXICAL LEVEL = "
	       << p->level
	       << min::eom;
		        
    return false;

} // mex::run_process

// Create Functions
// ------ ---------

min::uns32 mex::module_length = 1 << 12;

mex::module mex::create_module ( min::file f )
{
    min::locatable_var<mex::module_ins> m =
        ( (mex::module_ins) ::module_vec_type.new_stub
	     ( mex::module_length ) );
    mex::name_ref(m) = f->file_name;
    mex::interface_ref(m) = min::MISSING();
    mex::globals_ref(m) = min::NULL_STUB;
    mex::trace_info_ref(m) =
        min::gen_packed_vec_type.new_stub
	    ( mex::module_length );

    min::locatable_var
        <min::phrase_position_vec_insptr> position;
    min::init
        ( min::ref<min::phrase_position_vec_insptr>
	      (position),
	  f, min::MISSING_PHRASE_POSITION,
	  mex::module_length );
    mex::position_ref(m) = position;

    return m;
}

mex::process mex::create_process
	( min::printer printer )
{
    MIN_ASSERT ( printer != min::NULL_STUB,
    		 "mex::create_process printer"
		 " argument is NULL_STUB" );

    mex::process p =
        (mex::process) ::process_vec_type.new_stub
	    ( mex::run_stack_limit );
    mex::return_stack_ref(p) =
        (mex::return_stack)
	::return_stack_vec_type.new_stub
	    ( mex::run_return_stack_limit );
    mex::printer_ref(p) = printer;
    mex::pc pc = { min::NULL_STUB, 0 };
    mex::set_pc ( p, pc );
    p->optimize = mex::run_optimize;
    p->trace_flags = mex::run_trace_flags;
    p->excepts_mask = mex::run_excepts_mask;
    p->counter_limit = mex::run_counter_limit;

    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->nargs[i] = 0;
    p->level = 0;
    p->trace_depth = 0;
    p->counter = 0;
    p->optimized_counter = 0;
    p->excepts_accumulator = 0;
    p->state = mex::NEVER_STARTED;

    return p;
}

// Init Functions
// ---- ---------

mex::process mex::init_process
	( mex::module m, mex::process p )
{
    if ( p == min::NULL_STUB )
        p = mex::create_process();
    RW_UNS32 p->length = 0;
    RW_UNS32 p->return_stack->length = 0;
    mex::pc pc = { m, 0 };
    mex::set_pc ( p, pc );

    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->nargs[i] = 0;
    p->level = 0;
    p->trace_depth = 0;
    p->counter = 0;
    p->optimized_counter = 0;
    p->excepts_accumulator = 0;
    p->state = mex::NEVER_STARTED;

    return p;
}

mex::process mex::init_process
	( mex::pc pc, mex::process p )
{
    if ( p == min::NULL_STUB )
        p = mex::create_process();
    RW_UNS32 p->length = 0;
    RW_UNS32 p->return_stack->length = 0;

    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->nargs[i] = 0;
    p->level = 0;
    p->trace_depth = 0;
    p->counter = 0;
    p->optimized_counter = 0;
    p->excepts_accumulator = 0;
    p->state = mex::NEVER_STARTED;

    if ( pc.index >= pc.module->length )
        goto ERROR_EXIT;
    {
	const mex::instr * instr =
	    ~ ( pc.module + pc.index );
	if ( instr->op_code != mex::BEGF )
	    goto ERROR_EXIT;
	if ( instr->immedB != 1 )
	    goto ERROR_EXIT;
    }

    {   // Execute CALLG.

	mex::ret * ret =
	    ~ min::begin_ptr_of ( p->return_stack );
	mex::pc saved_pc = { min::NULL_STUB, 0 };
	mex::set_saved_pc ( p, ret, saved_pc );
	ret->saved_level = 0;
	ret->saved_fp = 0;
	ret->saved_nargs = 0;
	ret->nresults = 0;
	RW_UNS32 p->return_stack->length = 1;

	p->level = 1;
	p->trace_depth = 1;

	RW_UNS32 pc.index = pc.index + 1;
	mex::set_pc ( p, pc );

	return p;
    }

ERROR_EXIT:
    {   // Set p->pc illegal and return NULL_STUB.

        pc = { min::NULL_STUB,1 };
	set_pc ( p, pc );
	return min::NULL_STUB;
    }
}
