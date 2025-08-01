// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Wed Jul 30 10:19:11 PM EDT 2025
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

min::locatable_gen mex::NOT_A_NUMBER;
min::locatable_gen mex::FALSE;
min::locatable_gen mex::TRUE;
static min::locatable_gen ZERO;
static min::locatable_gen STAR;
static min::gen VOID;
    // Has GEN_ILLEGAL value distinct from every other
    // min::gen value.
static void check_op_infos ( void );
static void check_trace_class_infos ( void );
static void check_state_infos ( void );
static void initialize ( void )
{
    mex::NOT_A_NUMBER = min::new_num_gen ( NAN );
    mex::FALSE = min::FALSE();
    mex::TRUE = min::TRUE();
    ::ZERO = min::new_num_gen ( 0 );
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
    else if ( ! min::is_name ( pvar.value ) )
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

bool mex::excepts_check ( mex::process p )
{
    int flags =
        p->excepts_mask & p->excepts_accumulator;
    if ( flags == 0 ) return true;
    p->printer
        << min::bom
	<< "!!!!!!!!!!!!!!!!!!!!!!!!!"
        << " WARNING: TERMINATED PROCESS HAD"
	<< min::bol
        << min::place_indent ( 4 )
	<< min::indent
	<< " EXCEPTS ERROR(S): ";
    mex::print_excepts ( p->printer, flags );
    p->printer << min::eom;
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
    i = p->length;
    if ( i > p->max_length ) return false;

    const mex::instr * pcbegin;
    const mex::instr * pc;
    const mex::instr * pcend;
    min::gen * spbegin;
    min::gen * sp;
    min::gen * spend;

#   define RESTORE \
	pcbegin = ~ min::begin_ptr_of ( m ); \
	pc = pcbegin + p->pc.index; \
        pcend = pcbegin + m->length; \
	spbegin = ~ min::begin_ptr_of ( p ); \
	sp = spbegin + p->length; \
	spend = spbegin + p->max_length;
#   define SAVE \
	RW_UNS32 p->pc.index = pc - pcbegin; \
	RW_UNS32 p->length = sp - spbegin;

    RESTORE;

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

#   define SHIFT_CHECK \
	    min::float64 farg2 = FG ( arg2 ); \
	    if ( ! mex::isfinite ( farg2 ) \
		 || \
		 farg2 >= +1e15 \
		 || \
		 farg2 <= -1e15 ) \
	        goto ERROR_EXIT; \
	    int shift = (int) farg2; \
	    if ( shift != farg2 ) \
	        goto ERROR_EXIT;

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
	    sp[-2] = GF ( mex::fmod ( FG ( arg1 ),
		                      FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::MODI:
	    CHECK1I;
	    sp[-1] = GF ( mex::fmod ( FG ( arg1 ),
		                      FG ( arg2 ) ) );
	    break;
	case mex::MODR:
	    CHECK2R;
	    sp[-2] = GF ( mex::fmod ( FG ( arg1 ),
		                      FG ( arg2 ) ) );
	    -- sp;
	    break;
	case mex::MODRI:
	    CHECK1RI;
	    sp[-1] = GF ( mex::fmod ( FG ( arg1 ),
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
	case mex::LSH:
	{
	    CHECK2;
	    SHIFT_CHECK;
	    sp[-2] =
	        GF ( ldexp ( FG ( arg1 ), shift ) );
	    -- sp;
	    break;
	}
	case mex::LSHI:
	{
	    CHECK1I;
	    SHIFT_CHECK;
	    sp[-1] =
	        GF ( ldexp ( FG ( arg1 ), shift ) );
	    break;
	}
	case mex::RSH:
	{
	    CHECK2;
	    SHIFT_CHECK;
	    sp[-2] =
	        GF ( ldexp ( FG ( arg1 ), - shift ) );
	    -- sp;
	    break;
	}
	case mex::RSHI:
	{
	    CHECK1I;
	    SHIFT_CHECK;
	    sp[-1] =
	        GF ( ldexp ( FG ( arg1 ), - shift ) );
	    break;
	}
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
	    min::gen v = pc->immedD;
	    if (    min::is_obj ( v )
	         && ! min::public_flag_of ( v ) )
	        goto ERROR_EXIT;
	    sp = mex::process_push ( p, sp, v );
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
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    min::uns32 k = p->ap[j];
	    if ( i >= p->fp[j] - k )
	        goto ERROR_EXIT;
	    * sp ++ = spbegin[k+i];
	    break;
	}
	case mex::PUSHNARGS:
	{
	    min::uns32 immedB = pc->immedB;
	    if ( immedB < 1 || immedB > p->level )
	        goto ERROR_EXIT;
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    * sp ++ =
	        GF ( p->fp[immedB] - p->ap[immedB] );
	    break;
	}
	case mex::PUSHV:
	{
	    min::uns32 j = pc->immedB;
	    min::uns32 k = p->ap[j];
	    if ( j < 1 || j > p->level )
	        goto ERROR_EXIT;
	    if ( sp <= spbegin )
	        goto ERROR_EXIT;
	    min::float64 f = FG ( sp[-1] );
	    if ( ! mex::isfinite ( f ) )
	        goto ERROR_EXIT;
	    min::float64 ff = floor ( f );
	    if ( f != ff || ff < 0 )
	        goto ERROR_EXIT;
	    if ( ff >= p->fp[j] - k )
	        sp[-1] = min::NONE();
	    else
		sp[-1] = spbegin[k + (int) ff];
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
	case mex::DEL:
	{
	    min::uns32 i = pc->immedA;
	    min::uns32 j = pc->immedC;
	    if (    sp < spbegin
	         || i + j > sp - spbegin )
	        goto ERROR_EXIT;
	    min::gen * sp1 = sp - (int) i;
	    min::gen * sp2 = sp1 - (int) j;
	    while ( sp1 < sp ) * sp2 ++ = * sp1 ++;
	    sp = sp2;
	    break;
	}
	case mex::PUSHOBJ:
	{
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    SAVE;
	    min::locatable_gen value
	        ( min::new_obj_gen
		      ( pc->immedA, pc->immedC ) );
	    RESTORE;
	    sp = mex::process_push
	        ( p, sp, value );
	    break;
	}
	case mex::COPY:
	{
	    if ( sp < spbegin + 1 )
	        goto ERROR_EXIT;
	    min::obj_vec_ptr vp = sp[-1];
	    if ( vp == min::NULL_STUB )
	        goto ERROR_EXIT;
	    SAVE;
	    min::locatable_gen value
	        ( min::copy
		      ( vp,
			  min::unused_size_of ( vp )
			+ 20 ) );
	    RESTORE;
	    sp = mex::process_push
	        ( p, sp - 1, value );
	    break;
	}
	case mex::COPYI:
	{
	    if ( sp >= spend )
	        goto ERROR_EXIT;
	    min::obj_vec_ptr vp = pc->immedD;
	    if ( vp == min::NULL_STUB )
	        goto ERROR_EXIT;
	    SAVE;
	    min::locatable_gen value
	        ( min::copy
		      ( vp,
			  min::unused_size_of ( vp )
			+ 20 ) );
	    RESTORE;
	    sp = mex::process_push
	        ( p, sp, value );
	    break;
	}
	case mex::VPUSH:
	{
	    min::uns32 i = pc->immedA;
	    if ( sp < spbegin || i >= sp - spbegin )
	        goto ERROR_EXIT;

	    min::gen obj = sp[-(int)i-1];
	    if ( ! min::is_obj ( obj ) )
		goto ERROR_EXIT;
	    if ( min::public_flag_of ( obj ) )
	        goto ERROR_EXIT;

	    min::obj_vec_insptr vp = obj;
	    min::locatable_gen value ( * -- sp );
	    if (    min::get ( value,
		               min::dot_initiator )
		 == pc->immedD )
	    {
	        SAVE;
	        min::obj_vec_ptr vp1 = value;
		min::uns32 s = min::size_of ( vp1 );
		for ( min::uns32 i = 0; i < s; ++ i )
		    min::attr_push ( vp ) = vp1[i];
	    }
	    else
	    {
	        SAVE;
	        min::attr_push ( vp ) = value;
	    }
	    RESTORE;

	    break;
	}
	case mex::VPOP:
	{
	    min::uns32 i = pc->immedA;
	    if ( sp < spbegin || i >= sp - spbegin
	                      || sp >= spend )
	        goto ERROR_EXIT;

	    min::gen obj = sp[-(int)i-1];
	    if ( ! min::is_obj ( obj ) )
		goto ERROR_EXIT;
	    if ( min::public_flag_of ( obj ) )
	        goto ERROR_EXIT;

	    min::obj_vec_insptr vp = obj;
	    if ( min::size_of ( vp ) == 0 )
		sp = mex::process_push
		    ( p, sp, min::NONE() );
	    else
		sp = mex::process_push
		    ( p, sp, min::attr_pop ( vp ) );

	    break;
	}
	case mex::VSIZE:
	{
	    if ( sp < spbegin + 1 )
	        goto ERROR_EXIT;

	    min::obj_vec_ptr vp = sp[-1];
	    if ( vp == min::NULL_STUB )
	        goto ERROR_EXIT;
	    sp = mex::process_push
	        ( p, sp - 1,
		  GF ( min::size_of ( vp ) ) );

	    break;
	}
	case mex::GET:
	{
	    if ( sp <= spbegin )
	        goto ERROR_EXIT;
	    min::uns32 i = pc->immedA;
	    min::uns32 j = sp - spbegin;
	    if ( i >= j )
	        goto ERROR_EXIT;

	    min::gen label = sp[-1];

	    if ( min::is_num ( label ) )
	    {
		min::obj_vec_ptr vp = sp[-(int)i-1];
		if ( vp == min::NULL_STUB )
		    goto ERROR_EXIT;
		min::uns32 s = min::size_of ( vp );

		min::float64 flabel = FG ( label );
		if ( ! mex::isfinite ( flabel )
		     ||
		     flabel <= -1e9
		     ||
		     flabel >= +1e9
		     ||
		     floor ( flabel ) != flabel )
		    goto ERROR_EXIT;
		sp = mex::process_push
		    ( p, -- sp,
		      ( flabel < 0 || flabel >= s ) ?
		      min::NONE() :
		      vp[(int)flabel] );
	    }
	    else if ( min::is_name ( label ) )
	    {
		min::gen obj = sp[-(int)i-1];
		if ( ! min::is_obj ( obj ) )
		    goto ERROR_EXIT;
		sp = mex::process_push
		    ( p, -- sp,
		      min::get ( obj, label ) );
	    }
	    else
	        goto ERROR_EXIT;

	    break;
	}
	case mex::GETI:
	{
	    if ( sp <= spbegin || sp >= spend )
	        goto ERROR_EXIT;
	    min::uns32 i = pc->immedA;
	    min::uns32 k = sp - spbegin;
	    if ( i >= k )
	        goto ERROR_EXIT;
	    min::gen obj = sp[-(int)i-1];
	    min::gen label = pc->immedD;
	    if ( ! min::is_obj ( obj ) )
	        goto ERROR_EXIT;
	    if ( min::is_num ( label )
	         ||
		 ! min::is_name ( label ) )
	        goto ERROR_EXIT;
	    sp = mex::process_push
	        ( p, sp, min::get ( obj, label ) );
	    break;
	}
	case mex::SET:
	{
	    if ( sp <= spbegin )
	        goto ERROR_EXIT;
	    min::uns32 i = pc->immedA;
	    min::uns32 j = sp - spbegin;
	    if ( i >= j || 2 > j )
	        goto ERROR_EXIT;

	    min::gen obj = sp[-(int)i-1];
	    min::obj_vec_updptr vp = obj;
	    if ( vp == min::NULL_STUB )
		goto ERROR_EXIT;
	    if ( min::public_flag_of ( vp ) )
		goto ERROR_EXIT;
	    min::gen label = sp[-2];
	    if ( min::is_num ( label ) )
	    {
		min::uns32 s = min::size_of ( vp );

		min::float64 flabel = FG ( label );
		if ( ! mex::isfinite ( flabel )
		     ||
		     flabel <= -1e9
		     ||
		     flabel >= +1e9
		     ||
		     floor ( flabel ) != flabel )
		    goto ERROR_EXIT;
		if ( flabel < 0 || flabel >= s )
		    goto ERROR_EXIT;
		vp[(int)flabel] = * -- sp;
	    }
	    else if ( min::is_name ( label ) )
	    {
		vp = min::NULL_STUB;
		min::gen v = * -- sp;
		SAVE;
		min::set ( obj, label, v );
		RESTORE;
	    }
	    else
	        goto ERROR_EXIT;

	    -- sp;  // For label.

	    break;
	}
	case mex::SETI:
	{
	    if ( sp <= spbegin + 1 )
	        goto ERROR_EXIT;
	    min::uns32 i = pc->immedA;
	    min::uns32 k = sp - spbegin;
	    if ( i >= k )
	        goto ERROR_EXIT;
	    min::gen obj = sp[-(int)i-1];
	    min::gen label = pc->immedD;
	    if ( ! min::is_obj ( obj ) )
	        goto ERROR_EXIT;
	    if ( min::public_flag_of ( obj ) )
	        goto ERROR_EXIT;
	    if ( min::is_num ( label )
	         ||
		 ! min::is_name ( label ) )
	        goto ERROR_EXIT;
	    min::gen v = * -- sp;
	    SAVE;
	    min::set ( obj, label, v );
	    RESTORE;
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
	    if ( mex::isnan ( farg1 )
		 ||
		 mex::isnan ( farg2 )
		 ||
		 (    mex::isinf ( farg1 )
		   && mex::isinf ( farg2 )
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
	    if ( ! mex::isfinite ( farg ) )
	        goto ERROR_EXIT;
	    min::float64 immedD = FG ( pc->immedD );
	    if ( ! mex::isfinite ( immedD ) )
	        goto ERROR_EXIT;
	    if ( immedD <= 0 )
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
	    if ( arg == mex::TRUE )
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
	case mex::JMPFALSE:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( arg == mex::FALSE )
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
	case mex::JMPNONE:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( arg == min::NONE() )
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
	case mex::JMPINT:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::float64 farg = FG ( * -- sp );
	    if ( mex::isfinite ( farg )
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
	    if ( mex::isfinite ( farg ) )
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
	    if ( mex::isinf ( farg ) )
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
	case mex::JMPNAN:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    min::float64 farg = FG ( arg );
	    if ( mex::isnan ( farg )
	         &&
		 min::is_direct_float ( arg ) )
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
	case mex::JMPTRUTH:
	{
	    if ( sp < spbegin + 1 )
		goto ERROR_EXIT;
	    min::gen arg = * -- sp;
	    if ( arg == mex::TRUE
	         ||
		 arg == mex::FALSE )
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
	    min::uns32 immedB = pc->immedB;
	    if ( immedB > sp - spbegin )
	        goto ERROR_EXIT;
	    if ( sp + immedB > spend )
	        goto ERROR_EXIT;
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

	    if ( pc->trace_depth > p->trace_depth )
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
	    p->trace_depth -= pc->trace_depth;
	    pc -= immedC;
	    -- pc;
	    break;
	}
	case mex::SET_TRACE:
	case mex::TRACE:
	case mex::DUMP:
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
	    if ( 0 != ret->nresults
	         &&
		 mex::ALL_RESULTS != ret->nresults )
	        goto ERROR_EXIT;

	    min::gen * new_sp =
	        spbegin + p->ap[immedB];
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

	    mex::set_pc ( p, ret->saved_pc );
	    p->level = ret->saved_level;
	    p->fp[immedB] = new_fp;
	    p->ap[immedB] = ret->saved_ap;
	    p->trace_depth = ret->trace_depth;
	    RW_UNS32 p->return_stack->length = rp;

	    sp = new_sp;

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
	    min::uns32 nresults = ret->nresults;
	    if ( nresults == mex::ALL_RESULTS )
	        nresults = immedC;
	    else if ( immedC < ret->nresults )
	        goto ERROR_EXIT;

	    min::gen * new_sp =
	        spbegin + p->ap[immedB];
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

	    mex::set_pc ( p, ret->saved_pc );
	    p->level = ret->saved_level;
	    p->fp[immedB] = new_fp;
	    p->ap[immedB] = ret->saved_ap;
	    p->trace_depth = ret->trace_depth;
	    RW_UNS32 p->return_stack->length = rp;

	    min::gen * q = sp - (int) immedC;
	    min::gen * qend = q + nresults;
	    while ( q < qend )
	        * new_sp ++ = * q ++;
	    sp = new_sp;

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
		  (mex::module) pc->immedD :
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
	    min::uns32 fp = ( sp - spbegin );
	    min::uns32 nargs = pc->immedA;
	    if ( nargs > fp )
	        goto ERROR_EXIT;

	    mex::ret * ret =
	        ~ min::begin_ptr_of ( p->return_stack )
		+ rp;
	    mex::pc new_pc =
	        { m, (min::uns32) ( pc - pcbegin ) };
	    mex::set_saved_pc ( p, ret, new_pc );
	    ret->saved_level = p->level;
	    ret->saved_fp = p->fp[level];
	    ret->saved_ap = p->ap[level];
	    ret->trace_depth = p->trace_depth;
	    p->level = level;
	    p->fp[level] = fp;
	    p->ap[level] = fp - nargs;
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

#   undef RESTORE
#   undef SAVE
#   undef CHECK1
#   undef CHECK1I
#   undef CHECK1RI
#   undef CHECK2
#   undef CHECK2R
#   undef FG
#   undef GF
#   undef A1F
#   undef SHIFT_CHECK
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
min::uns32 mex::run_test = 0;

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
    { mex::LSH, A2, T_AOP, mex::error_func,
      "LSH", "<<" },
    { mex::LSHI, A2I, T_AOP, mex::error_func,
      "LSHI", "<<" },
    { mex::RSH, A2, T_AOP, mex::error_func,
      "RSH", ">>" },
    { mex::RSHI, A2I, T_AOP, mex::error_func,
      "RSHI", ">>" },
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
    { mex::DEL, NONA, T_POP, NULL, "DEL", NULL },
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
    { mex::JMPFALSE, J1, T_JMPS, NULL,
      "JMPFALSE", NULL },
    { mex::JMPNONE, J1, T_JMPS, NULL, "JMPNONE", NULL },
    { mex::JMPINT, J1, T_JMPS, NULL, "JMPINT", NULL },
    { mex::JMPFIN, J1, T_JMPS, NULL, "JMPFIN", NULL },
    { mex::JMPINF, J1, T_JMPS, NULL, "JMPINF", NULL },
    { mex::JMPNAN, J1, T_JMPS, NULL, "JMPNAN", NULL },
    { mex::JMPNUM, J1, T_JMPS, NULL, "JMPNUM", NULL },
    { mex::JMPTRUTH, J1, T_JMPS, NULL,
      "JMPTRUTH", NULL },
    { mex::JMPSTR, J1, T_JMPS, NULL, "JMPSTR", NULL },
    { mex::JMPOBJ, J1, T_JMPS, NULL, "JMPOBJ", NULL },
    { mex::BEG, NONA, T_BEG, NULL, "BEG", NULL },
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
    { mex::PUSHV, NONA, T_PUSH, NULL, "PUSHV", NULL },
    { mex::SET_TRACE, NONA, T_ALWAYS,
                      NULL, "SET_TRACE", NULL },
    { mex::NOP, NONA, T_NOP, NULL, "NOP", NULL },
    { mex::TRACE, NONA, T_ALWAYS, NULL, "TRACE", NULL },
    { mex::DUMP, NONA, T_ALWAYS, NULL, "DUMP", NULL },
    { mex::WARN, NONA, T_ALWAYS, NULL, "WARN", NULL },
    { mex::ERROR, NONA, T_ALWAYS, NULL, "ERROR", NULL },
    { mex::SET_EXCEPTS, NONA, T_SET_EXCEPTS,
                        NULL, "SET_EXCEPTS", NULL },
    { mex::TRACE_EXCEPTS, NONA, T_ALWAYS,
                          NULL, "TRACE_EXCEPTS", NULL },
    { mex::SET_OPTIMIZE, NONA, T_SET_OPTIMIZE,
                         NULL, "SET_OPTIMIZE", NULL },
    { mex::PUSHOBJ, NONA, T_SET,
                    NULL, "PUSHOBJ", NULL },
    { mex::COPY, NONA, T_SET, NULL, "COPY", NULL },
    { mex::COPYI, NONA, T_SET, NULL, "COPYI", NULL },
    { mex::VPUSH, NONA, T_SET, NULL, "VPUSH", NULL },
    { mex::VPOP, NONA, T_GET, NULL, "VPOP", NULL },
    { mex::VSIZE, NONA, T_GET, NULL, "VSIZE", NULL },
    { mex::SET, NONA, T_SET, NULL, "SET", NULL },
    { mex::SETI, NONA, T_SET, NULL, "SETI", NULL },
    { mex::GET, NONA, T_GET, NULL, "GET", NULL },
    { mex::GETI, NONA, T_GET, NULL, "GETI", NULL },
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
    { T_GET, "GET" },
    { T_SET, "SET" },
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
    { PERMANENT_ERROR, "PERMANENT_ERROR",
                    "bad program counter or"
		    " stack pointer" },
    { FORMAT_ERROR, "FORMAT_ERROR",
                    "bad program format" }
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
	       +
	       ( pp.end.offset != 0 )
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
    mex::module m;
    min::uns32 i;
    const mex::instr * pcbegin;
    const mex::instr * pc;
    const mex::instr * pcend;
    min::gen * spbegin;
    min::gen * sp;
    min::gen * spend;
    const char * message;
    min::uns32 limit;

    min::uns32 frame_length;
    min::uns8 op_code;
    min::uns8 trace_class;
    op_info * op_info;
    min::gen arg1, arg2;
#   define obj arg1
#   define label arg2
    min::locatable_gen result;
    int sp_change;
    min::uns32 trace_flags; 

    m = p->pc.module;
    i = p->pc.index;

    if ( m == min::NULL_STUB && i != 0 )
    {
	message = "Illegal PC: no module and index > 0";
	goto PERMANENT_ERROR;
    }
    if ( i > m->length )
    {
	message = "Illegal PC: PC index greater than"
	          " module length";
	goto PERMANENT_ERROR;
    }
    pcbegin = ~ min::begin_ptr_of ( m );
    pc = pcbegin + i;
    pcend = pcbegin + m->length;

    i = p->length;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large; process"
	          " length > process max_length";
	goto PERMANENT_ERROR;
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

TEST_LOOP:	// Come here after fatal error processed
		// if p->test > 0.

    p->state = mex::RUNNING;

    while ( true ) // Inner loop.
    {
	if ( pc < pcbegin  )
	{
	    message = "Illegal PC: pc < pcbegin";
	    goto PERMANENT_ERROR;
	}
	if ( pc > pcend  )
	{
	    message = "Illegal PC: pc > pcend";
	    goto PERMANENT_ERROR;
	}
	if ( sp < spbegin  )
	{
	    message = "Illegal SP: sp < spbegin";
	    goto PERMANENT_ERROR;
	}
	if ( sp > spend  )
	{
	    message = "Illegal SP: sp > spend";
	    goto PERMANENT_ERROR;
	}

        if ( pc == pcend )
	{
	    SAVE;
	    p->state = mex::MODULE_END;
	    return true;
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
	        return true;
	    }
	    min::interrupt();
	    mex::module om = p->pc.module;
	    min::uns32 oi = p->pc.index;
	    if ( om == min::NULL_STUB )
	    {
		if ( oi == 0 )
		{
		    p->state = mex::CALL_END;
		    return true;
		}
		message = "Illegal PC: no module and"
		          " index > 0 after optimized"
			  " run";
		goto PERMANENT_ERROR;
	    }
	    if ( oi >= m->length )
	    {
		if ( oi == m->length )
		{
		    p->state = mex::MODULE_END;
		    return true;
		}
		message = "Illegal PC: PC index greater"
			  " than module length after"
			  " optimized run";
		goto PERMANENT_ERROR;
	    }
	    if ( p->length > p->max_length )
	    {
		message = "Illegal SP: p->length >"
			  " p->max_length after"
			  " optimized run";
		goto PERMANENT_ERROR;
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

	frame_length =
		( sp - spbegin ) - p->fp[p->level];
	switch ( op_info->op_type )
	{
	case NONA:
	    arg1 = ::VOID;
	    arg2 = ::VOID;
	    goto NON_ARITHMETIC;
	case A2:
	    sp_change = -1;
	    if ( 2 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-2];
	    arg2 = sp[-1];
	    goto ARITHMETIC;
	case A2R:
	    sp_change = -1;
	    if ( 2 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = sp[-2];
	    goto ARITHMETIC;
	case A2I:
	    if ( 1 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = pc->immedD;
	    goto ARITHMETIC;
	case A2RI:
	    if ( 1 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = pc->immedD;
	    arg2 = sp[-1];
	    goto ARITHMETIC;
	case A1:
	    if ( 1 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = ::ZERO;
	        // To avoid error detector.
	    goto ARITHMETIC;
	case J1:
	    sp_change = -1;
	    if ( 1 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-1];
	    arg2 = ::VOID;
	    goto JUMP;
	case JS:
	    if ( pc->immedB >= frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-(int)pc->immedB-1];
	    arg2 = pc->immedD;
	    goto JUMP;
	case J2:
	    sp_change = -2;
	    if ( 2 > frame_length )
	        goto FRAME_TOO_SMALL;
	    arg1 = sp[-2];
	    arg2 = sp[-1];
	    goto JUMP;
	case J:
	    arg1 = ::VOID;
	    arg2 = ::VOID;
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
	        fresult = mex::fmod ( farg1, farg2 );
		break;
	    case mex::POW:
	    case mex::POWI:
	    case mex::POWR:
	    case mex::POWRI:
	        fresult = pow ( farg1, farg2 );
		break;
	    case mex::LSH:
	    case mex::LSHI:
	    case mex::RSH:
	    case mex::RSHI:
	    {
		if ( ! mex::isfinite ( farg2 )
		     ||
		     farg2 >= +1e15
		     ||
		     farg2 <= -1e15
		     ||
		     (int) farg2 != farg2 )
		{
		    std::feraiseexcept ( FE_INVALID );
		    fresult = NAN;
		}

		else
		{
		    int shift = (int) farg2;
		    if ( op_code == RSH
		         ||
			 op_code == RSHI )
		        shift = - shift;
		    fresult = ldexp ( farg1, shift );
		}

		break;
	    }
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
	    } // end switch ( op_code )

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
		    p->printer
		        << min::bol
	                << "!!!!!!!!!!!!!!!!!!!!!!!!!"
		        << " ERROR: ";
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
		    m->position != min::NULL_STUB
		    &&
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

		if ( op_info->op_type == mex::A1 )
		    p->printer
		        << " = "
			<< min::pgen_quote ( result )
			<< " <= "
			<< op_info->oper
			<< " "
			<< min::pgen_quote ( arg1 );

		else
		    p->printer
		        << " = "
			<< min::pgen_quote ( result )
			<< " <= "
			<< min::pgen_quote ( arg1 )
			<< " "
			<< op_info->oper
			<< " "
			<< min::pgen_quote ( arg2 );
		p->printer << min::eol;

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
		arg2 = pc->immedD;
		min::float64 immedD = FG ( arg2 );
		if (    ! mex::isfinite ( immedD )
		     || immedD <= 0 )
		{
		    message = "JMPCNT immedD is not"
			      " a finite number > 0";
		    goto INNER_FATAL;
		}
		min::float64 farg1 = FG ( arg1 );
		min::uns32 i = pc->immedB;
		if ( ! mex::isfinite ( farg1 ) )
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
		    execute_jmp =
		        ( arg1 == mex::TRUE );
		    break;
		case mex::JMPFALSE:
		    execute_jmp =
		        ( arg1 == mex::FALSE );
		    break;
		case mex::JMPNONE:
		    execute_jmp =
		        ( arg1 == min::NONE() );
		    break;
		case mex::JMPINT:
		{
		    min::float64 farg = FG ( arg1 );
		    execute_jmp =
		        ( mex::isfinite ( farg )
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
		        mex::isfinite ( FG ( arg1 ) );
		    break;
		case mex::JMPINF:
		    execute_jmp =
		        mex::isinf ( FG ( arg1 ) );
		    break;
		case mex::JMPNAN:
		    execute_jmp =
		        mex::isnan ( FG ( arg1 ) )
			&&
			min::is_direct_float ( arg1 );
		    break;
		case mex::JMPNUM:
		    execute_jmp = min::is_num ( arg1 );
		    break;
		case mex::JMPTRUTH:
		    execute_jmp =
		        ( arg1 == mex::TRUE
			  ||
			  arg1 == mex::FALSE );
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
	        MIN_REQUIRE
		    ( op_info->op_type == mex::J2 );
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
	        MIN_REQUIRE
		    ( op_info->op_type == mex::J2 );

		min::float64 farg1 = FG ( arg1 );
		min::float64 farg2 = FG ( arg2 );

		immedB = pc->immedB;
		if ( immedB > 1 )
		{
		    message = "JMP immedB != 0 or 1;"
		              " illegal ";
		    goto INNER_FATAL;
		}

		if ( mex::isnan ( farg1 )
		     ||
		     mex::isnan ( farg2 )
		     ||
		     (    mex::isinf ( farg1 )
		       && mex::isinf ( farg2 )
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

	    if ( bad_jmp ) execute_jmp = false;

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

		min::uns32 index = p->pc.index;

		min::phrase_position pp =
		    m->position != min::NULL_STUB
		    &&
		    index < m->position->length ?
		    m->position[index] :
		    min::MISSING_PHRASE_POSITION;

		p->printer << min::bol;

		if ( bad_jmp )
		{
		    p->printer
		        << min::bom
			<< min::place_indent ( 4 )
			<< "!!!!!!!!!!!!!!!!!!!!!!!!!"
			<< " FATAL PROGRAM ERROR:";
		    p->printer
			<< min::indent
		        << "invalid operands to a"
			   " conditional jump"
			   " instruction";

		    if ( pp )
		    {
			min::file input_file =
			    m->position->file;

			p->printer
			    << min::indent
			    << min::pline_numbers
				      ( input_file, pp )
			    << ": ";
			p->printer << min::bol;
			min::print_phrase_lines
			    ( p->printer, input_file,
			      pp );
		    }

		    p->printer << min::eom;
		}

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
		                  " operand(s)";
		    if (    arg1 != ::VOID
		         || arg2 != ::VOID )
		    {
			p->printer
			    << min::indent << "ARGS =";
			if ( arg1 != ::VOID )
			    p->printer
				<< " "
				<< min::pgen_quote
				       ( arg1 );
			if ( arg2 != ::VOID )
			    p->printer
				<< " "
				<< min::pgen_quote
				       ( arg2 );
		    }
		}
		else if ( op_code != mex::JMP )
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
			    << min::pgen_quote ( arg1 )
			    << " <= 0";
		    else if ( op_info->oper == NULL )
		        p->printer
			    << min::pgen_quote ( arg1 );
		    else
		    {
			p->printer
			    << min::pgen_quote ( arg1 )
			    << " "
			    << op_info->oper
			    << " "
			    << min::pgen_quote ( arg2 );
		    }
		}

		p->printer << min::eol;

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

	    if ( bad_jmp )
	    {
	    	SAVE;
		if ( p->test == 0 )
		{
		    p->state = mex::JMP_ERROR;
		    return false;
		}
		p->printer
		    << min::bol
		    << "TREATING JMP AS UNSUCCESSFUL;"
		       " CONTINUING BECAUSE"
		       " PROCESS->TEST == "
		    << p->test
		    << " > 0"
		    << min::eol;	  
		-- p->test;
		RESTORE;
	    }

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

	    min::locatable_gen value;
	        // Value to be pushed or popped.
		// Computed early for tracing.
	    min::gen arg;
	        // Argment computed for tracing.

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
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		value = sp[-(int)immedA-1];
		sp_change = +1;
		break;
	    case mex::PUSHI:
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		value = immedD;
		if (    min::is_obj ( value )
		     && ! min::public_flag_of
		              ( value ) )
		{
		    message = "PUSHI immedD is a"
		              " writable object";
		    goto INNER_FATAL;
		}
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
		     >= ( p->level == immedB ?
		          sp - spbegin :
			  p->ap[immedB+1] ) )
		{
		    message = "PUSHL immedA too large;"
		              " addresses variable"
			      " beyond the end of the"
			      " function frame";
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
		if (    immedA
		     >= p->fp[immedB] - p->ap[immedB] )
		{
		    message = "PUSHA immedA too"
		              " large; addresses"
			      " non-extant argument";
		    goto INNER_FATAL;
		}
		value = spbegin
		    [p->ap[immedB] + immedA];
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
		    ( p->fp[immedB] - p->ap[immedB] );
		sp_change = +1;
		break;
	    case mex::PUSHV:
	    {
		min::uns32 j = immedB;
		if ( j < 1 || j > p->level )
		{
		    message = "PUSHV immedB is 0 or"
		              " greater than current"
			      " lexical level";
		    goto INNER_FATAL;
		}
		min::uns32 k = p->ap[j];
		arg = sp[-1];
		min::float64 farg = FG ( arg );
		if ( ! mex::isfinite ( farg ) )
		{
		    message = "PUSHV: top of stack is"
		              " not a non-negative"
			      " integer";
		    goto INNER_FATAL;
		}
		min::float64 ff = floor ( farg );
		if ( farg != ff || ff < 0 )
		{
		    message = "PUSHV: top of stack is"
		              " not a non-negative"
			      " integer";
		    goto INNER_FATAL;
		}
		if ( ff >= p->fp[j] - k )
		    value = min::NONE();
		else
		    value = spbegin[k + (int) ff];

		break;
	    }
	    case mex::POPS:
		if ( sp <= spbegin )
		{
		    message =
		        "POPS: cannot pop an empty"
			" stack";
		    goto INNER_FATAL;
		}
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;
		value = sp[-1];
		sp_change = -1;
		break;
	    case mex::DEL:
	        if ( immedA + immedC < immedA
		     ||
		     immedA + immedC > frame_length )
		    goto FRAME_TOO_SMALL;
		sp_change = - (int ) immedC;
		break;
	    case mex::PUSHOBJ:
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		SAVE;
		value = min::new_obj_gen
		    ( immedA, immedC );
		RESTORE;
		sp_change = +1;
		break;
	    case mex::COPY:
	    {
		if ( sp <= spbegin )
		{
		    message = "COPY: stack is empty";
		    goto INNER_FATAL;
		}
		min::obj_vec_ptr vp = sp[-1];
		if ( vp == min::NULL_STUB )
		{
		    message = "COPY: top of stack is"
		              " not an object";
		    goto INNER_FATAL;
		}
		SAVE;
		value =
		    ( min::copy
			  ( vp,
			      min::unused_size_of ( vp )
			    + 20 ) );
		RESTORE;
		break;
	    }
	    case mex::COPYI:
	    {
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;
		min::obj_vec_ptr vp = immedD;
		if ( vp == min::NULL_STUB )
		{
		    message = "COPYI: immedD is"
		              " not an object";
		    goto INNER_FATAL;
		}
		SAVE;
		value =
		    ( min::copy
			  ( vp,
			      min::unused_size_of ( vp )
			    + 20 ) );
		RESTORE;
		sp_change = 1;
		break;
	    }
	    case mex::VPUSH:
	    {
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;

		obj = sp[-(int)immedA-1];
		if ( ! min::is_obj ( obj ) )
		    goto NOT_AN_OBJECT;
		 if ( min::public_flag_of ( obj ) )
		{
		    message =
			"VPUSH: trying to change"
			" read-only object";
		    goto INNER_FATAL;
		}
		min::obj_vec_insptr vp = obj;
		value = sp[-1];
		if (    min::get ( value,
				   min::dot_initiator )
		     == immedD )
		{
		    SAVE;
		    min::obj_vec_ptr vp1 = value;
		    min::uns32 s = min::size_of ( vp1 );
		    for ( min::uns32 i = 0;
		          i < s; ++ i )
			min::attr_push ( vp ) = vp1[i];
		}
		else
		{
		    SAVE;
		    min::attr_push ( vp ) = value;
		}
		RESTORE;

		sp_change = -1;
		break;
	    }
	    case mex::VPOP:
	    {
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;
		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;

		obj = sp[-(int)immedA-1];
		if ( ! min::is_obj ( obj ) )
		    goto NOT_AN_OBJECT;

		if ( min::public_flag_of ( obj ) )
		{
		    message =
			"VPOP: trying to change"
			" read-only object";
		    goto INNER_FATAL;
		}
		min::obj_vec_insptr vp = obj;
		value = ( min::size_of ( vp ) == 0 ?
			  min::NONE() :
			  min::attr_pop ( vp ) );
		mex::process_push ( p, sp, value );
		sp_change = +1;

		break;
	    }
	    case mex::VSIZE:
	    {
	        if ( frame_length == 0 )
		    goto FRAME_TOO_SMALL;
		obj = sp[-1];
		min::obj_vec_ptr vp = obj;
		if ( vp == min::NULL_STUB )
		    goto NOT_AN_OBJECT;
		    
		value = min::new_direct_float_gen
			    ( min::size_of ( vp ) );
	        break;
	    }
	    case mex::GET:
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;
	        if ( 1 > frame_length )
		    goto FRAME_TOO_SMALL;

		sp_change = 0;

		obj = sp[-(int)immedA-1];
		label = sp[-1];
		if ( min::is_num ( label ) )
		{
		    min::obj_vec_ptr vp = obj;
		    if ( vp == min::NULL_STUB )
		        goto NOT_AN_OBJECT;
		    min::uns32 s = min::size_of ( vp );

		    min::float64 flabel = FG ( label );
		    if ( ! mex::isfinite ( flabel )
			 ||
			 flabel <= -1e9
			 ||
			 flabel >= +1e9
			 ||
		         floor ( flabel ) != flabel )
		    {
		        message = "GET: numeric label"
			          " is not integer in"
				  " range (-1e9, +1e9)";
			goto INNER_FATAL;
		    }
		    value =
		      ( ( flabel < 0 || flabel >= s ) ?
		        min::NONE() : vp[(int)flabel] );

		}
		else if ( min::is_name ( label ) )
		{
		    if ( ! min::is_obj ( obj ) )
			goto NOT_AN_OBJECT;
		    value = min::get ( obj, label );
		}
		else
		    goto NOT_A_LABEL;

		break;
	    case mex::GETI:
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;

		if ( sp >= spend )
		    goto STACK_LIMIT_STOP;

		label = immedD;
		obj = sp[-(int)immedA-1];
		if ( ! min::is_obj ( obj ) )
		    goto NOT_AN_OBJECT;
		if (    ! min::is_name ( label )
		     || min::is_num ( label ) )
		    goto NOT_A_LABEL;
		value = min::get ( obj, label );
		sp_change = +1;
		break;
	    case mex::SET:
	    {
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;
	        if ( 2 > frame_length )
		    goto FRAME_TOO_SMALL;

		sp_change = -2;

		value = sp[-1];
		obj = sp[-(int)immedA-1];
		label = sp[-2];
		min::obj_vec_updptr vp = obj;
		if ( vp == min::NULL_STUB )
		    goto NOT_AN_OBJECT;
		if ( min::public_flag_of ( vp ) )
		{
		    message = "SET: trying to change"
		              " read-only object";
		    goto INNER_FATAL;
		}
		if ( min::is_num ( label ) )
		{
		    min::uns32 s = min::size_of ( vp );

		    min::float64 flabel = FG ( label );
		    if ( ! mex::isfinite ( flabel )
			 ||
			 flabel <= -1e9
			 ||
			 flabel >= +1e9
			 ||
		         floor ( flabel ) != flabel )
		    {
		        message = "SET: numeric label"
			          " is not integer in"
				  " range (-1e9, +1e9)";
			goto INNER_FATAL;
		    }
		    if ( flabel < 0 || flabel >= s )
		    {
		        message = "SET: vector element"
			          " does not exist";
			goto INNER_FATAL;
		    }
		    vp[(int)flabel] = value;
		        // Executed before trace.
		}
		else if ( min::is_name ( label ) )
		{
		    vp = min::NULL_STUB;
		    SAVE;
		    min::set ( obj, label, value );
		        // Executed before trace.
		    RESTORE;
		}
		else
		    goto NOT_A_LABEL;

		break;
	    }
	    case mex::SETI:
	    {
	        if ( immedA >= frame_length )
		    goto FRAME_TOO_SMALL;

		sp_change = -1;

		value = sp[-1];
		label = immedD;
		obj = sp[-(int)immedA-1];
		if ( ! min::is_obj ( obj ) )
		    goto NOT_AN_OBJECT;
		if ( min::public_flag_of ( obj ) )
		{
		    message = "SETI: trying to change"
		              " read-only object";
		    goto INNER_FATAL;
		}
		if (    ! min::is_name ( label )
		     || min::is_num ( label ) )
		    goto NOT_A_LABEL;
		SAVE;
		min::set ( obj, label, value );
		    // Executed before trace.
	        RESTORE;
		break;
	    }
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
	        if ( immedA > frame_length )
		    goto FRAME_TOO_SMALL;
		sp_change = - (int) immedA;
		break;
	    }
	    case mex::BEGL:
	        if ( immedB > frame_length )
		    goto FRAME_TOO_SMALL;
		if ( sp + immedB > spend )
		    goto STACK_LIMIT_STOP;
		sp_change = immedB;
	        break;
	    case mex::ENDL:
	    case mex::CONT:
	    {
	        if ( immedA > frame_length )
		    goto FRAME_TOO_SMALL;
	        if (    2 * immedB < immedB
		     || immedA + 2 * immedB < immedA
		     ||   immedA + 2 * immedB
		        >   frame_length )
		    goto FRAME_TOO_SMALL;
		min::uns32 location = pc - pcbegin;
	        if ( immedC + 1 > location )
		{
		    message =
		        "ENDL/CONT: immedC too large;"
			" associated BEGL non-extant";
		    goto INNER_FATAL;
		}

		if ( pc->trace_depth > p->trace_depth )
		{
		    message = "CONT: trace depth would"
		              " become negative if"
			      " instruction was"
			      " executed";
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
	    case mex::TRACE_EXCEPTS:
	        break;
	    case mex::DUMP:
	        immedA = 0;
		// Fall through
	    case mex::TRACE:
	    case mex::WARN:
	    case mex::ERROR:
	        if ( immedA > frame_length )
		    goto FRAME_TOO_SMALL;
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
		min::uns32 nresults = ret->nresults;
		if ( nresults == mex::ALL_RESULTS )
		    nresults = immedC;
		else if ( immedC < nresults )
		{
		    message =
		        ( op_code == mex::ENDF ?
			  "ENDF: CALL expects return"
			  " values but function ends"
			  " returning no values:"
			  " 0 < return stack nresults" :
			  "RET: function returns fewer"
			  " values than CALL expects:"
			  " immedC < return stack"
		              " nresults" );
		    goto INNER_FATAL;
		}

	        min::gen * new_sp =
		    spbegin + p->ap[immedB];

	        if ( immedC > frame_length )
		    goto FRAME_TOO_SMALL;
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

		// RET/ENDF updates trace_depth and
		// stack before trace.
		//
		p->trace_depth = ret->trace_depth;
		min::gen * q = sp - (int) immedC;
		min::gen * qend = q + nresults;
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
		      immedD :
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
			<< "!!!!!!!!!!!!!!!!!!!!!!!!!"
		        << " FATAL ERROR INSTRUCTION:"
			<< min::eol;

		min::phrase_position pp =
		    m->position != min::NULL_STUB
		    &&
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
		    p->printer << " "
		               << min::pgen_quote
			              ( value );
		    break;
		}
		case mex::PUSHV:
		{
		    p->printer << ": ";
		    if ( tinfo != min::MISSING()  )
			p->printer
			    << ::pvar ( tinfo );
		    else
			p->printer << "*";
		    p->printer
		        << " <= stack[ap["
			<< immedB
			<< "]+"
			<< min::pgen ( arg )
			<< "] = "
			<< min::pgen_quote ( value );
		    break;
		}
		case mex::DEL:
		{
		    if ( immedA > 0 && immedC > 0 )
			p->printer << " sp[-"
				   << immedA + immedC
				   << "..-"
				   << immedC + 1
				   << "] <= sp[-"
				   << immedA
				   << "..-1]";
		    else if ( immedC > 0 )
			p->printer << " pop sp[-"
				   << immedC
				   << "..-1]";
		    else
			p->printer << " do nothing";
		    break;
		}
		case mex::PUSHOBJ:
		{
		    p->printer << ": "
			       << ::pvar ( tinfo )
			       << " <= NEW OBJ ( "
			       << immedA << ", "
			       << immedC << ")";
		    break;
		}
		case mex::COPY:
		case mex::COPYI:
		{
		    p->printer << ": "
			       << ::pvar ( tinfo )
			       << " <= "
		               << min::pgen_quote
			              ( value );
		    break;
		}
		case mex::VPUSH:
		{
		    p->printer << ":";
		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) == 2 )
		        p->printer << " PUSHED "
			           << ::pvar ( lp[1] )
				   << " <= "
				   << ::pvar ( lp[0] );
		    else
		        p->printer << " PUSHED * <= *";
		    p->printer << " = "
		               << min::pgen_quote
			              ( value );
		    break;
		}
		case mex::VPOP:
		case mex::VSIZE:
		{
		    const char * s =
		        ( op_code == mex::VPOP ?
			  "POPPED" : "SIZE OF" );
		    p->printer << ":";
		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) == 2 )
		        p->printer << " "
			           << ::pvar ( lp[1] )
				   << " <= " << s << " "
				   << ::pvar ( lp[0] );
		    else
		        p->printer << " * <= "
			           << s << " *";
		    p->printer << " = "
		               << min::pgen_quote
			              ( value );
		    break;
		}
		case mex::GET:
		case mex::GETI:
		{
		    p->printer << ":";
		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) == 2 )
		        p->printer << " "
			           << ::pvar ( lp[1] )
				   << " <= "
				   << ::pvar ( lp[0] );
		    else
		        p->printer << " * <= *";
		    p->printer << "["
		               << min::pgen_quote
			       	      ( label )
			       << "] = "
		               << min::pgen_quote
			              ( value );
		    break;
		}
		case mex::SET:
		case mex::SETI:
		{
		    p->printer << ":";
		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) == 2 )
		        p->printer << " "
			           << ::pvar ( lp[1] )
				   << "["
				   << min::pgen_quote
				          ( label )
				   << "] <= "
				   << ::pvar ( lp[0] );
		    else
		        p->printer << " *["
				   << min::pgen_quote
				          ( label )
				   << "] <= *";
		    p->printer << " = "
		               << min::pgen_quote
			              ( value );
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
		    p->printer << " "
		               << min::pgen_quote
			              ( value );
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
				  << min::pgen_quote
				   ( p[p->length-j] );
			    else
			        p->printer << "?";
			}
		    }
		    break;
		}

		}
		p->printer << min::eol;

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
	    case mex::PUSHI:
	    case mex::PUSHG:
	    case mex::PUSHL:
	    case mex::PUSHOBJ:
	    {
		sp = mex::process_push
		    ( p, sp, value );
		break;
	    }
	    case mex::PUSHV:
	        sp[-1] = value;
		break;
	    case mex::POPS:
	    {
		-- sp;
		sp[-(int)immedA] = value;
		MUP::acc_write_update ( p, value );
		break;
	    }
	    case mex::DEL:
	    {
	        min::gen * sp1 = sp - (int) immedA;
		min::gen * sp2 = sp1 - (int) immedC;
		while ( sp1 < sp ) * sp2 ++ = * sp1 ++;
		sp += sp_change;
		break;
	    }
	    case mex::VPOP:
	    case mex::VSIZE:
	    case mex::GET:
	    case mex::GETI:
	    case mex::COPY:
	    case mex::COPYI:
	    {
		sp = mex::process_push
		    ( p, sp + sp_change - 1, value );
		break;
	    }
	    case mex::VPUSH:
	    case mex::SET:
	    case mex::SETI:
	    {
	        sp += sp_change;
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
		p->trace_depth -= pc->trace_depth;
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
	    case mex::DUMP:
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
		p->ap[immedB] = ret->saved_ap;
		RW_UNS32 p->return_stack->length = rp;

		p->level = ret->saved_level;

		m = em;
		if ( m == min::NULL_STUB )
		{
		    RET_SAVE;
		    p->state = mex::CALL_END;
		    return true;
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
		      immedD :
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
		ret->saved_ap = p->ap[level];
		ret->trace_depth = p->trace_depth;
		p->level = level;
		min::uns32 k = ( sp - spbegin );
		p->fp[level] = k;
		p->ap[level] = k - (int) immedA;
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

	FRAME_TOO_SMALL:
	    message = "current frame too small"
		      " for instruction";
	    goto INNER_FATAL;

	NOT_AN_OBJECT:
	    message =
	        "object argument is not an object";
	    goto INNER_FATAL;

	NOT_A_LABEL:
	    message =
	        "label argument is not a label";
	    goto INNER_FATAL;

	// Fatal error discovered in loop.
	//
	INNER_FATAL:
	    SAVE;
	    goto FATAL;

#	undef FG

    } // end loop

// Come on error that does not allow test continuation.
//
PERMANENT_ERROR:
    p->state = mex::PERMANENT_ERROR;
    p->printer << min::bom
	       << "!!!!!!!!!!!!!!!!!!!!!!!!!"
               << " FATAL PROGRAM PERMANENT ERROR:"
	       << min::eol
	       << message
	       << min::eol
	       << "instruction counter = "
	       << p->counter
	       << min::eom;
    return false;

// Come here with fatal error `message'.  At this point
// the blame is on the current instruction.
//
FATAL:
    p->state = mex::FORMAT_ERROR;

    m = p->pc.module;
    i = p->pc.index;
    pc = ~ ( m + i );

    op_code = pc->op_code;
    trace_class = pc->trace_class;
    min::uns8 trace_depth = pc->trace_depth;
    min::uns32 immedA = pc->immedA;
    min::uns32 immedB = pc->immedB;
    min::uns32 immedC = pc->immedC;
    min::gen immedD = pc->immedD;

    const char * op_name =
        ( op_code >= mex::NUMBER_OF_OP_CODES ?
          "UNKNOWN-OP-CODE" :
           op_infos[op_code].name );
    const char * trace_class_name =
        ( trace_class >= mex::NUMBER_OF_TRACE_CLASSES ?
          "UNKNOWN-TRACE-CLASS" :
           trace_class_infos[trace_class].name );

    char trace_depth_buffer[100] = { 0 };
    if ( trace_depth != 0 )
        sprintf ( trace_depth_buffer,
	          " [%u]", trace_depth );

    min::phrase_position pp =
	m->position != min::NULL_STUB
	&&
	i < m->position->length ?
	m->position[i] :
	min::MISSING_PHRASE_POSITION;

    p->printer << min::bom
	       << min::place_indent ( 4 )
	       << "!!!!!!!!!!!!!!!!!!!!!!!!!"
               << " FATAL PROGRAM ERROR:";
    p->printer << min::indent << message;
    if ( pp )
    {
	min::file input_file = m->position->file;

        p->printer << min::indent
	           << min::pline_numbers
		          ( input_file, pp )
	           << ": ";
        p->printer << min::bol;
	min::print_phrase_lines
	    ( p->printer, input_file, pp );
    }

    if ( ! pp )
        p->printer << min::indent
	           << "PC NAME IS "
		   << p->pc.module->name;
    print_header ( p, pp, 0 )
	<< " " << op_name
	<< " " << trace_class_name
	<< trace_depth_buffer
	<< " " << immedA
	<< " " << immedB
	<< " " << immedC
	<< " " << min::pgen ( immedD )
	<< min::bol << min::place_indent ( 4 );

    if ( arg1 != ::VOID || arg2 != ::VOID )
    {
	p->printer << min::indent << "ARGS =";
	if ( arg1 != ::VOID )
	    p->printer
	        << " " << min::pgen_quote ( arg1 );
	if ( arg2 != ::VOID )
	    p->printer
	        << " " << min::pgen_quote ( arg2 );
    }
    p->printer << min::eom;
		        
    if ( p->test == 0 ) return false;
    p->printer << min::bol
               << "SKIPPING INSTRUCTION AND CONTINUING"
                  " BECAUSE PROCESS->TEST == "
	       << p->test
	       << " > 0"
	       << min::eol;	  
    -- p->test;
    RESTORE;
    ++ pc; -- limit;
    goto TEST_LOOP;


} // mex::run_process

// Create Functions
// ------ ---------

min::uns32 mex::module_length = 1 << 12;

mex::module mex::create_module ( min::file f )
{
    min::locatable_var<mex::module_ins> m =
        ( (mex::module_ins) ::module_vec_type.new_stub
	     ( mex::module_length ) );
    mex::interface_ref(m) = min::MISSING();
    mex::globals_ref(m) = min::NULL_STUB;
    mex::trace_info_ref(m) =
        min::gen_packed_vec_type.new_stub
	    ( mex::module_length );

    min::locatable_var
        <min::phrase_position_vec_insptr> position =
             (min::phrase_position_vec_insptr)
	     min::NULL_STUB;
    if ( f != min::NULL_STUB )
    {
	mex::name_ref(m) = f->file_name;
	min::init
	    ( min::ref<min::phrase_position_vec_insptr>
		  (position),
	      f, min::MISSING_PHRASE_POSITION,
	      mex::module_length );
    }
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
    p->test = mex::run_test;
    p->trace_flags = mex::run_trace_flags;
    p->excepts_mask = mex::run_excepts_mask;
    p->counter_limit = mex::run_counter_limit;

    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->ap[i] = 0;
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
        p->fp[i] = p->ap[i] = 0;
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
        p->fp[i] = p->ap[i] = 0;
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
	ret->saved_ap = 0;
	ret->nresults = 0;
	ret->trace_depth = 0;
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
