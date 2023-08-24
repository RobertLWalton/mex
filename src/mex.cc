// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Aug 24 14:46:10 EDT 2023
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
min::uns32 mex::module_length = 1 << 12;
min::uns32 mex::stack_limit = 1 << 14;
min::uns32 mex::return_stack_limit = 1 << 12;
min::uns32 mex::run_counter_limit = 1 << 20;
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

static void check_op_infos ( void );
static void check_trace_class_infos ( void );
static void initialize ( void )
{
    check_op_infos();
    check_trace_class_infos();
}
static min::initializer initializer ( ::initialize );

// Support Functions
//
static min::float64 powi ( min::float64 x, unsigned i )
{
    min::float64 r = 1, z = x;
    unsigned j = 1 << 0;
    while ( i != 0 )
    {
        if ( j & i )
	{
	    i -= j;
	    r *= z;
	}
	j <<= 1;
	z = z * z;
    }
    return r;
}

inline void print_indent ( mex::process p )
{
    unsigned i = p->trace_depth % 10;
    if ( p->optimize ) i = 0;
       // In optimize mode an instruction might
       // occassionally be executed in non-optimize
       // mode and change the depth.
    unsigned j = ( i + 1 ) * mex::trace_indent;
    while ( 1 < j )
    {
	p->printer << mex::trace_mark;
	-- j;
    }
    p->printer << ' ';
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

    min::uns32 limit = p->limit;
    if ( p->counter >= limit ) return false;
    limit -= p->counter;

    bool result = true;
    feclearexcept ( FE_ALL_EXCEPT );

#   define CHECK1 \
	    if ( sp < spbegin + 1 ) \
	        goto ERROR_EXIT

#   define CHECK2 \
	    if ( sp < spbegin + 2 ) \
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
	    sp[-2] = GF
	        ( FG ( sp[-2] ) + FG ( sp[-1] ) );
	    -- sp;
	    break;
	case mex::ADDI:
	    CHECK1;
	    sp[-1] = GF
	        ( FG ( sp[-1] ) + FG ( pc->immedD ) );
	    break;
	case mex::MUL:
	    CHECK2;
	    sp[-2] = GF
	        ( FG ( sp[-2] ) * FG ( sp[-1] ) );
	    -- sp;
	    break;
	case mex::MULI:
	    CHECK1;
	    sp[-1] = GF
	        ( FG ( sp[-1] ) * FG ( pc->immedD ) );
	    break;
	case mex::SUB:
	    CHECK2;
	    sp[-2] = GF
	        ( FG ( sp[-2] ) - FG ( sp[-1] ) );
	    -- sp;
	    break;
	case mex::SUBI:
	    CHECK1;
	    sp[-1] = GF
	        ( FG ( sp[-1] ) - FG ( pc->immedD ) );
	    break;
	case mex::SUBR:
	    CHECK2;
	    sp[-2] = GF
	        ( FG ( sp[-1] ) - FG ( sp[-2] ) );
	    -- sp;
	    break;
	case mex::SUBRI:
	    CHECK1;
	    sp[-1] = GF
	        ( FG ( pc->immedD ) - FG ( sp[-1] ) );
	    break;
	case mex::DIV:
	    CHECK2;
	    sp[-2] = GF
	        ( FG ( sp[-2] ) / FG ( sp[-1] ) );
	    -- sp;
	    break;
	case mex::DIVI:
	    CHECK1;
	    sp[-1] = GF
	        ( FG ( sp[-1] ) / FG ( pc->immedD ) );
	    break;
	case mex::DIVR:
	    CHECK2;
	    sp[-2] = GF
	        ( FG ( sp[-1] ) / FG ( sp[-2] ) );
	    -- sp;
	    break;
	case mex::DIVRI:
	    CHECK1;
	    sp[-1] = GF
	        ( FG ( pc->immedD ) / FG ( sp[-1] ) );
	    break;
	case mex::MOD:
	    CHECK2;
	    sp[-2] = GF
	        ( fmod ( FG ( sp[-2] ),
		         FG ( sp[-1] ) ) );
	    -- sp;
	    break;
	case mex::MODI:
	    CHECK1;
	    sp[-1] = GF
	        ( fmod ( FG ( sp[-1] ),
		         FG ( pc->immedD ) ) );
	    break;
	case mex::MODR:
	    CHECK2;
	    sp[-2] = GF
	        ( fmod ( FG ( sp[-1] ),
		         FG ( sp[-2] ) ) );
	    -- sp;
	    break;
	case mex::MODRI:
	    CHECK1;
	    sp[-1] = GF
	        ( fmod ( FG ( pc->immedD ),
		         FG ( sp[-1] ) ) );
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
	    sp[-1] = GF ( - FG ( sp[-1] ) );
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
	    sp[-2] = GF
	        ( atan2 ( FG ( sp[-2] ),
		          FG ( sp[-1] ) ) );
	    -- sp;
	    break;
	case mex::ATAN2R:
	    CHECK2;
	    sp[-2] = GF
	        ( atan2 ( FG ( sp[-1] ),
		          FG ( sp[-2] ) ) );
	    -- sp;
	    break;
	case mex::POWI:
	    CHECK1;
	    sp[-1] = GF
	        ( powi ( FG ( sp[-1] ), pc->immedA ) );
	    break;
	case mex::PUSHS:
	{
	    min::uns32 i = pc->immedA;
	    if ( sp >= spend || i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp = sp[-i-1];
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
	    sp[-i] = * sp;
	    break;
	}
	case mex::JMP:
	case mex::JMPEQ:
	case mex::JMPNE:
	case mex::JMPLT:
	case mex::JMPLEQ:
	case mex::JMPGT:
	case mex::JMPGEQ:
	{
	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedC = pc->immedC;
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
		case mex::JMPLT:
		    execute_jmp = ( arg1 < arg2 );
		    break;
		case mex::JMPLEQ:
		    execute_jmp = ( arg1 <= arg2 );
		    break;
		case mex::JMPGT:
		    execute_jmp = ( arg1 > arg2 );
		    break;
		case mex::JMPGEQ:
		    execute_jmp = ( arg1 >= arg2 );
		    break;
		}
	    }
	    if ( immedA > new_sp - spbegin
		 ||
		 immedC > pcend - pc
		 ||
		 immedC == 0 )
	        goto ERROR_EXIT;

	    if ( ! execute_jmp )
	    {
	        sp = new_sp;
		break;
	    }

	    sp = new_sp - (int) immedA;
	    pc += immedC;
	    -- pc;
	    break;
	}
	case mex::BEG:
	case mex::NOP:
	case mex::END:
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
	    if ( immedC > pc - pcbegin )
	        goto ERROR_EXIT;
	    sp -= immedA;
	    for ( min::uns32 i = immedB; 0 < i; -- i )
	        sp[-(int)immedB-i] = sp[-i];
	    pc -= immedC;
	    -- pc;
	    break;
	}
	case mex::SET_TRACE:
	case mex::TRACE:
	case mex::WARN:
	case mex::ERROR:
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
		        - p->nargs[immedB];
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
	    p->nargs[immedB] = ret->saved_nargs;
	    RW_UNS32 p->return_stack->length = rp;

	    if ( em == min::NULL_STUB )
	        goto RET_EXIT;

            m = em;
	    pcbegin = ~ min::begin_ptr_of ( m );
	    pc = pcbegin + new_pc;
	    pcend = pcbegin + m->length;
	    -- pc;
	    break;
	}
	case mex::RET:
	{
	    min::uns32 immedA = pc->immedA;
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
		        - p->nargs[immedB];
	    min::uns32 new_fp = ret->saved_fp;
	    if ( new_fp > new_sp - spbegin )
		goto ERROR_EXIT;
	    if ( immedA + immedC < immedA )
		// Check for overflow.
	        goto ERROR_EXIT;
	    if ( immedA + immedC > sp - new_sp )
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
	    p->nargs[immedB] = ret->saved_nargs;
	    RW_UNS32 p->return_stack->length = rp;

	    min::gen * qend = sp - (int) immedA;
	    min::gen * q = qend - (int) immedC;
	    while ( q < qend )
	        * ++ new_sp = * ++ q;
	    sp = new_sp;

	    if ( em == min::NULL_STUB )
	        goto RET_EXIT;

            m = em;
	    pcbegin = ~ min::begin_ptr_of ( m );
	    pc = pcbegin + new_pc;
	    pcend = pcbegin + m->length;
	    -- pc;
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
	        { m, (min::uns32)
		     ( pc - pcbegin + 1 ) };
	    mex::set_saved_pc ( p, ret, new_pc );
	    ret->saved_level = p->level;
	    ret->saved_fp = p->fp[level];
	    ret->saved_nargs = p->nargs[level];
	    p->level = level;
	    p->fp[level] = ( sp - spbegin );
	    p->nargs[level] = pc->immedA;
	    ret->nresults = pc->immedB;
	    RW_UNS32 p->return_stack->length = rp + 1;

	    new_pc = { cm, immedC + 1 };
	    mex::set_pc ( p, new_pc );
	    m = cm;
	    break;
	}

	} // end switch ( op_code )

	++ pc, -- limit;
    }

ERROR_EXIT:
    result = false;
EXIT:
    RW_UNS32 p->pc.index = pc - pcbegin;
RET_EXIT:
    RW_UNS32 p->length = sp - spbegin;
    p->excepts_accumulator |= 
	fetestexcept ( FE_ALL_EXCEPT );
    p->counter = p->limit - limit;
    return result;

#   undef CHECK1
#   undef CHECK2
#   undef FG
#   undef GF
#   undef A1F
}


// Run Process
// --- -------

// Operation Information Table.
//
mex::op_info mex::op_infos [ mex::NUMBER_OF_OP_CODES ] =
{   { 0, 0, T_ALWAYS, "NONE", "" },
    { mex::ADD, A2, T_AOP, "ADD", "+" },
    { mex::ADDI, A2I, T_AOP, "ADDI", "+" },
    { mex::MUL, A2, T_AOP, "MUL", "*" },
    { mex::MULI, A2I, T_AOP, "MULI", "*" },
    { mex::SUB, A2, T_AOP, "SUB", "-" },
    { mex::SUBR, A2R, T_AOP, "SUBR", "-" },
    { mex::SUBI, A2I, T_AOP, "SUBI", "-" },
    { mex::SUBRI, A2RI, T_AOP, "SUBRI", "-" },
    { mex::DIV, A2, T_AOP, "DIV", "/" },
    { mex::DIVR, A2R, T_AOP, "DIVR", "/" },
    { mex::DIVI, A2I, T_AOP, "DIVI", "/" },
    { mex::DIVRI, A2RI, T_AOP, "DIVRI", "/" },
    { mex::MOD, A2, T_AOP, "MOD", "fmod" },
    { mex::MODR, A2R, T_AOP, "MODR", "fmod" },
    { mex::MODI, A2I, T_AOP, "MODI", "fmod" },
    { mex::MODRI, A2RI, T_AOP, "MODRI", "fmod" },
    { mex::FLOOR, A1, T_AOP, "FLOOR", "floor" },
    { mex::CEIL, A1, T_AOP, "CEIL", "ceil" },
    { mex::TRUNC, A1, T_AOP, "TRUNC", "trunc" },
    { mex::ROUND, A1, T_AOP, "ROUND", "round" },
    { mex::NEG, A1, T_AOP, "NEG", "-" },
    { mex::ABS, A1, T_AOP, "ABS", "abs" },
    { mex::LOG, A1, T_AOP, "LOG", "log" },
    { mex::LOG10, A1, T_AOP, "LOG10", "log10" },
    { mex::EXP, A1, T_AOP, "EXP", "exp" },
    { mex::EXP10, A1, T_AOP, "EXP10", "exp10" },
    { mex::SIN, A1, T_AOP, "SIN", "sin" },
    { mex::ASIN, A1, T_AOP, "ASIN", "asin" },
    { mex::COS, A1, T_AOP, "COS", "cos" },
    { mex::ACOS, A1, T_AOP, "ACOS", "acos" },
    { mex::TAN, A1, T_AOP, "TAN", "tan" },
    { mex::ATAN, A1, T_AOP, "ATAN", "atan" },
    { mex::ATAN2, A2, T_AOP, "ATAN2", "atan2" },
    { mex::ATAN2R, A2R, T_AOP, "ATAN2R", "atan2" },
    { mex::POWI, A1, T_AOP, "POWI", "pow" },
    { mex::PUSHS, NONA, T_PUSH, "PUSHS", NULL },
    { mex::PUSHL, NONA, T_PUSH, "PUSHL", NULL },
    { mex::PUSHI, NONA, T_PUSH, "PUSHI", NULL },
    { mex::PUSHG, NONA, T_PUSH, "PUSHG", NULL },
    { mex::POPS, NONA, T_POP, "POPS", NULL },
    { mex::JMP, J, T_JMP, "JMP", NULL },
    { mex::JMPEQ, J2, T_JMPS, "JMPEQ", "==" },
    { mex::JMPNE, J2, T_JMPS, "JMPNE", "!=" },
    { mex::JMPLT, J2, T_JMPS, "JMPLT", "<" },
    { mex::JMPLEQ, J2, T_JMPS, "JMPLEQ", "<=" },
    { mex::JMPGT, J2, T_JMPS, "JMPGT", ">" },
    { mex::JMPGEQ, J2, T_JMPS, "JMPGEQ", ">=" },
    { mex::BEG, NONA, T_BEG, "BEG", NULL },
    { mex::NOP, NONA, T_NOP, "NOP", NULL },
    { mex::END, NONA, T_END, "END", NULL },
    { mex::BEGL, NONA, T_BEGL, "BEGL", NULL },
    { mex::ENDL, NONA, T_ENDL, "ENDL", NULL },
    { mex::CONT, NONA, T_CONT, "CONT", NULL },
    { mex::SET_TRACE, NONA, T_ALWAYS, "SET_TRACE",
                      NULL },
    { mex::TRACE, NONA, T_ALWAYS, "TRACE", NULL },
    { mex::WARN, NONA, T_ALWAYS, "WARN", NULL },
    { mex::ERROR, NONA, T_ALWAYS, "ERROR", NULL },
    { mex::SET_EXCEPTS, NONA, T_SET_EXCEPTS,
                          "SET_EXCEPTS", NULL },
    { mex::TRACE_EXCEPTS, NONA, T_ALWAYS,
                          "TRACE_EXCEPTS", NULL },
    { mex::BEGF, NONA, T_BEGF, "BEGF", NULL },
    { mex::ENDF, NONA, T_ENDF, "ENDF", NULL },
    { mex::CALLM, NONA, T_CALLM, "CALLM", NULL },
    { mex::CALLG, NONA, T_CALLG, "CALLG", NULL },
    { mex::RET, NONA, T_RET, "RET", NULL },
    { mex::PUSHA, NONA, T_PUSH, "PUSHA", NULL },
    { mex::PUSHNARGS, NONA, T_PUSH, "PUSHNARGS", NULL },
    { mex::PUSHV, A1, T_PUSH, "PUSHV", "pushv" },
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
    { T_NOP, "NOP" },
    { T_SET_EXCEPTS, "SET_EXCEPTS" },
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

inline min::printer print_message_header
	( mex::process p, min::phrase_position pp )
{
    print_indent ( p );
    p->printer << "{";

    if ( pp )
	p->printer
	    << pp.end.line - 1
	    << ":";

    p->printer << p->pc.index
	       << ","
	       << p->length
	       << ","
	       << p->counter
	       << "}";
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
    pcbegin = ~ min::begin_ptr_of ( m );
    pc = pcbegin + i;
    pcend = pcbegin + m->length;

    i = p->length;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large";
	goto FATAL;
    }
    spbegin = ~ min::begin_ptr_of ( p );
    sp = spbegin + i;
    spend = spbegin + p->max_length;

    limit = p->limit;
    if ( p->counter >= limit )
    {
        p->state = mex::LIMIT_STOP;
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
	p->counter = p->limit - limit;

#   define RET_SAVE \
	RW_UNS32 p->length = sp - spbegin; \
	p->counter = p->limit - limit;

#   define RESTORE \
	pcbegin = ~ min::begin_ptr_of ( m ); \
	pc = pcbegin + p->pc.index; \
	pcend = pcbegin + m->length; \
	spbegin = ~ min::begin_ptr_of ( p ); \
	sp = spbegin + p->length; \
	spend = spbegin + p->max_length; \
        limit = p->limit - p->counter;

    p->state = mex::RUNNING;
    while ( true ) // Inner loop.
    {
        if ( pc == pcend )
	{
	    SAVE;
	    p->state = mex::MODULE_END;
	    return true;
	}
	if ( limit == 0 )
	{
	    SAVE;
	    p->state = mex::LIMIT_STOP;
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
		          " index > 0";
		goto FATAL;
	    }
	    if ( oi >= m->length )
	    {
		if ( oi == m->length )
		{
		    p->state = mex::MODULE_END;
		    return true;
		}
		message = "Illegal PC: index too large";
		goto FATAL;
	    }

	    if ( p->counter >= p->limit )
	    {
		p->state = mex::LIMIT_STOP;
		return false;
	    }

	    RESTORE;
	}

        min::uns8 op_code = pc->op_code;
	min::uns8 trace_class = pc->trace_class;
	op_info * op_info;
	min::float64 arg1, arg2;
	min::gen * new_sp = sp;
	min::uns32 trace_flags = p->trace_flags; 

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
	case A2RI:
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
	    new_sp -= 1;
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
	    case mex::MUL:
	    case mex::MULI:
	        result = arg1 * arg2;
		break;
	    case mex::SUB:
	    case mex::SUBI:
	    case mex::SUBR:
	    case mex::SUBRI:
	        result = arg1 - arg2;
		break;
	    case mex::DIV:
	    case mex::DIVI:
	    case mex::DIVR:
	    case mex::DIVRI:
	        result = arg1 / arg2;
		break;
	    case mex::MOD:
	    case mex::MODI:
	    case mex::MODR:
	    case mex::MODRI:
	        result = fmod ( arg1, arg2 );
		break;
	    case mex::FLOOR:
	        result = floor ( arg1 );
		break;
	    case mex::CEIL:
	        result = ceil ( arg1 );
		break;
	    case mex::ROUND:
	        result = rint ( arg1 );
		break;
	    case mex::TRUNC:
	        result = trunc ( arg1 );
		break;
	    case mex::NEG:
	        result = - arg1;
		break;
	    case mex::ABS:
	        result = fabs ( arg1 );
		break;
	    case mex::LOG:
	        result = log ( arg1 );
		break;
	    case mex::LOG10:
	        result = log10 ( arg1 );
		break;
	    case mex::EXP:
	        result = exp ( arg1 );
		break;
	    case mex::EXP10:
	        result = exp10 ( arg1 );
		break;
	    case mex::SIN:
	        result = sin ( arg1 );
		break;
	    case mex::COS:
	        result = cos ( arg1 );
		break;
	    case mex::TAN:
	        result = tan ( arg1 );
		break;
	    case mex::ASIN:
	        result = asin ( arg1 );
		break;
	    case mex::ACOS:
	        result = acos ( arg1 );
		break;
	    case mex::ATAN:
	        result = atan ( arg1 );
		break;
	    case mex::ATAN2:
	    case mex::ATAN2R:
	        result = atan2 ( arg1, arg2 );
		break;
	    case mex::POWI:
	        result = powi ( arg1, pc->immedA );
		break;
	    case mex::PUSHV:
	    {
		min::uns32 j = pc->immedB;
		if ( j < 1 || j > p->level )
		{
		    message = "invalid immedB";
		    goto INNER_FATAL;
		}
		min::float64 ff = floor ( arg1 );
		if ( std::isnan ( ff )
		     ||
		     arg1 != ff
		     ||
		     ff < 1 || ff > p->nargs[j] )
		{
		    result = NAN;
		    feraiseexcept ( FE_INVALID );
		}
		else
		{
		    min::uns32 i = (min::uns32) ff;
		    min::uns32 k =
		        p->fp[j] - p->nargs[j];
		    result = MUP::direct_float_of
		                ( spbegin[k + i - 1] );
		}
		break;
	    }
	    } // end switch ( op_code )

	    int excepts =
	        fetestexcept ( FE_ALL_EXCEPT );
	    p->excepts_accumulator |= excepts;
	    excepts &= p->excepts;

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

		print_message_header ( p, pp )
		    << " " << min::bom
		    << op_info->name << ": ";

		if ( m->trace_info != min::NULL_STUB  )
		{
		    min::gen trace_info =
		        m->trace_info[p->pc.index];
		    if ( min::is_str ( trace_info ) )
		        p->printer << trace_info;
		    else
		        p->printer << "*";
		}
		else
		    p->printer << "*";

		char buffer[200];
		if ( op_code == mex::POWI )
		    sprintf
			( buffer,
			  " = %.15g <= %s %.15g %u",
			  result,
			  op_info->oper, arg1,
			  pc->immedA );

		else if ( op_code == mex::PUSHV )
		    sprintf
			( buffer,
			  " = %.15g"
			  " <= fp[%u]"
			  "[-nargs[%u]+%.15g-1]",
			  result, pc->immedB,
			  pc->immedB, arg1 );

		else if ( op_info->op_type == mex::A1 )
		    sprintf
			( buffer,
			  " = %.15g <= %s %.15g",
			  result, op_info->oper, arg1 );

		else
		    sprintf
			( buffer,
			  " = %.15g <= %.15g %s %.15g",
			  result,
			  arg1, op_info->oper, arg2 );

		p->printer << buffer << min::eom;

	        RESTORE;
	    }

	    * new_sp ++ = min::new_num_gen ( result );
	    sp = new_sp;

	    goto LOOP;
	}

	JUMP:
	{
	    // Process JMP.

	    min::uns32 immedA = pc->immedA;
	    min::uns32 immedC = pc->immedC;

	    if ( immedA > new_sp - spbegin )
	    {
	        message = "immedA too large";
	        goto INNER_FATAL;
	    }
	    if ( pc + immedC > pcend )
	    {
	        message = "immedC too large";
	        goto INNER_FATAL;
	    }
	    if ( immedC == 0 )
	    {
	        message = "immedC == 0";
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
		case mex::JMPLT:
		    execute_jmp = ( arg1 < arg2 );
		    break;
		case mex::JMPLEQ:
		    execute_jmp = ( arg1 <= arg2 );
		    break;
		case mex::JMPGT:
		    execute_jmp = ( arg1 > arg2 );
		    break;
		case mex::JMPGEQ:
		    execute_jmp = ( arg1 >= arg2 );
		    break;
		}
	    }

	    min::uns8 jmp_trace_class = trace_class;
	    if ( ! execute_jmp
	         &&
		 trace_class == mex::T_JMPS )
	        ++ jmp_trace_class;

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

		min::phrase_position pp =
		    p->pc.index < m->position->length ?
		    m->position[p->pc.index] :
		    min::MISSING_PHRASE_POSITION;

		print_message_header ( p, pp )
		    << " " << min::bom
		    << op_info->name << ": ";

		if ( m->trace_info != min::NULL_STUB
		     &&
		       p->pc.index
		     < m->trace_info->length )
		{
		    min::gen trace_info =
		        m->trace_info[p->pc.index];
		    if ( min::is_str ( trace_info ) )
		        p->printer << " " << trace_info;
		}

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

		    char buffer[200];
		    sprintf
		        ( buffer, " <= %.15g %s %.15g",
			  arg1, op_info->oper, arg2 );
		    p->printer << buffer;
		}

		p->printer << min::eom;

		RESTORE;
	    }

	    if ( ! execute_jmp )
		sp = new_sp;
	    else
	    {
		sp = new_sp - (int) immedA;
		p->trace_depth -= pc->trace_depth;
		pc += immedC;
		-- pc;
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

	    min::gen value;
	        // Value to be pushed or popped.
		// Computed early for tracing.
	     

	    // Pre-trace check for fatal errors.
	    // Also compute value for PUSH... and
	    // POP... .
	    //
	    switch ( op_code )
	    {
	    case mex::PUSHS:
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
		value = sp[-(int)immedA-1];
		break;
	    case mex::PUSHI:
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		value = immedD;
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
		value = mg->globals[immedA];
		break;
	    }
	    case mex::PUSHL:
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		if (    immedB == 0
		     && m->globals != min::NULL_STUB )
		{
		    if ( immedA >= m->globals->length )
		    {
			message = "immedA too large";
			goto INNER_FATAL;
		    }
		    break;
		}
		if ( immedB > p->level )
		{
		    message = "immedB too large";
		    goto INNER_FATAL;
		}
		if (    p->fp[immedB] + immedA
		     >= sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
	        if ( immedB == 0
		     &&
		     m->globals != min::NULL_STUB )
		    value = m->globals[immedA];
		else
		    value =
			spbegin[p->fp[immedB] + immedA];
		break;
	    case mex::PUSHA:
		if ( immedB < 1 || immedB > p->level )
		{
		    message = "invalid immedB";
		    goto INNER_FATAL;
		}
		if (    immedA < 1
		     || immedA > p->nargs[immedB] )
		{
		    message = "immedA out of range";
		    goto INNER_FATAL;
		}
		value = spbegin
		    [p->fp[immedB] - (int) immedA];
		break;
	    case mex::PUSHNARGS:
		if ( immedB < 1 || immedB > p->level )
		{
		    message = "invalid immedB";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		value = min::new_num_gen
		    ( p->nargs[immedB] );
		break;
	    case mex::POPS:
		if ( sp <= spbegin )
		{
		    message =
		        "stack empty for pop";
		    goto INNER_FATAL;
		}
	        if ( immedA >= sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
		value = sp[-1];
		break;
	    case mex::BEG:
	    case mex::NOP:
	    case mex::END:
	    {
		if ( immedA > sp - spbegin )
		    goto INNER_FATAL;
		break;
	    }
	    case mex::BEGL:
	        if ( immedA + immedB < immedA )
		{
		    message = "immedA+immedB too large";
		    goto INNER_FATAL;
		}
	        if ( immedA + immedB > sp - spbegin )
		{
		    message = "immedA+immedB too large";
		    goto INNER_FATAL;
		}
		if ( sp + immedB > spend + immedA )
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
	    case mex::TRACE:
	    case mex::WARN:
	    case mex::ERROR:
		if ( immedA > sp - spbegin )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
	        break;
	    case mex::BEGF:
	        if ( immedC > pcend - pc )
		{
		    message = "immedC too large";
		    goto INNER_FATAL;
		}
		if ( immedB > p->level + 1 )
		{
		    message = "immedB too large";
		    goto INNER_FATAL;
		}
	        if ( immedB > mex::max_lexical_level )
		{
		    message = "immedB too large";
		    goto INNER_FATAL;
		}
		break;
	    case mex::ENDF:
		immedA = immedC = 0;
		// Fall through
	    case mex::RET:
	    {
		if ( immedB != p->level )
		{
		    message = "immedB != current"
		              " lexical level";
		    goto INNER_FATAL;
		}
		min::uns32 rp = p->return_stack->length;
		if ( rp == 0 )
		{
		    message = "return stack is empty";
		    goto INNER_FATAL;
		}
		-- rp;
		const mex::ret * ret =
		   ~ ( p->return_stack + rp );
		if ( immedC != ret->nresults )
		{
		    message =
		        ( op_code == mex::ENDF ?
			  "return stack nresults is"
			  " not zero" :
			  "immedC != return stack"
		              " nresults" );
		    goto INNER_FATAL;
		}

	        min::gen * new_sp =
		    spbegin + p->fp[immedB]
		            - p->nargs[immedB];

		if ( immedA + immedC < immedA
		     ||
		     immedA + immedC > sp - new_sp )
		{
		    // Not possible for ENDF.
		    message =
		        "immedA + immedC is too large";
		    goto INNER_FATAL;
		}
		mex::module em = ret->saved_pc.module;
		min::uns32 new_pc = ret->saved_pc.index;
		if ( em == min::NULL_STUB )
		{
		    if ( new_pc != 0 )
		    {
			message = "bad saved_pc value";
			goto INNER_FATAL;
		    }
		}
		else
		{
		    if ( new_pc > em->length )
		    {
			message = "bad saved_pc value";
			goto INNER_FATAL;
		    }
		}

		min::uns32 new_fp = ret->saved_fp;
		if ( new_fp > new_sp - spbegin )
		{
		    message = "bad saved_fp value";
		    goto INNER_FATAL;
		}


		// RET/ENDF is executed before tracing,
		// so what is traced is the CALL
		// instruction, but with op_code ==
		// RET/ENDF.
		//
		mex::set_pc ( p, ret->saved_pc );
		p->fp[immedB] = new_fp;
		p->nargs[immedB] = ret->saved_nargs;
		RW_UNS32 p->return_stack->length = rp;

		min::gen * qend = sp - (int) immedA;
		min::gen * q = qend - (int) immedC;
		while ( q < qend )
		    * ++ new_sp = * ++ q;
		sp = new_sp;

		p->level = ret->saved_level;
		-- p->trace_depth;

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
		      pc->immedD :
		      m );
		if ( cm == min::NULL_STUB )
		{
		    message = "immedD is not a module";
		    goto INNER_FATAL;
		}
		if ( immedC >= cm->length )
		{
		    message = "immedC is too large";
		    goto INNER_FATAL;
		}
		const mex::instr * target =
		    ~ ( cm + immedC );
		if ( target->op_code != mex::BEGF )
		{
		    message =
		        "transfer target is not a BEGF";
		    goto INNER_FATAL;
		}
		min::uns32 level = target->immedB;
		if ( level > p->level + 1 )
		{
		    message =
		        "BEGF immedB is too large";
		    goto INNER_FATAL;
		}
		if ( immedA < target->immedA )
		{
		    message =
		        "immedA < BEGF immedA";
		    goto INNER_FATAL;
		}
		min::uns32 rp = p->return_stack->length;
		if ( rp >= p->return_stack->max_length )
		{
		    message = "return stack is full";
		    goto INNER_FATAL;
		}

		// CALL... is executed before tracing,
		// so what is traced is the BEGF
		// instruction, but with op_code ==
		// CALL....
		//
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

		print_message_header ( p, pp )
		    << " " << op_infos[op_code].name
		    << ": " << min::bom;

		min::gen tinfo  = min::MISSING();
		if (   p->pc.index
		     < m->trace_info->length )
		    tinfo = m->trace_info[p->pc.index];

		switch ( op_code )
		{
		case mex::PUSHS:
		case mex::PUSHL:
		case mex::PUSHG:
		case mex::PUSHA:
		case mex::POPS:
		{
		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) == 2 )
		    {
		        p->printer << " "
			           << lp[1]
				   << " <=== "
				   << lp[0]
				   << " =";
		    }
		    p->printer << " " << value;
		    break;
		}
		case mex::PUSHI:
		case mex::PUSHNARGS:
		{
		    if ( min::is_str ( tinfo ) )
		        p->printer << " "
			           << tinfo
				   << " <===";
		    if ( op_code == mex::PUSHNARGS )
		        p->printer << " nargs["
			           << immedB
				   << "] =";
		    p->printer << " " << value;
		    break;
		}
		case mex::SET_TRACE:
		{
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
		case mex::BEGF:
		{
		    // trace_info only used by CALL...
		    //
		    break;
		}

		default:
		{
		    min::lab_ptr lp ( tinfo );
		    if ( lp != min::NULL_STUB
		         &&
			 min::lablen ( lp ) > 0 )
		    {
		        min::uns32 len =
			    min::lablen ( lp );
		        p->printer << lp[0];
			for ( min::uns32 i = 1;
			      i < len; ++ i )
			{
			    min::uns32 j = len - i;
			    p->printer
			        << ", " << lp[i]
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
		sp -= immedA;
		for ( int i = immedB; 0 < i; -- i )
		    sp[-(int)immedB-i] = sp[-i];
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
	    case mex::TRACE:
	    case mex::WARN:
	    case mex::ERROR:
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
		// RET/ENDF was executed before tracing
		// so the CALL instruction would be
		// traced.
		//
		break;
	    }
	    case mex::CALLM:
	    case mex::CALLG:
	    {
		// CALL.. was executed before tracing
		// to the BEGF instruction would be
		// traced.
		//
		break;
	    }

	    } // end switch ( op_code )

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
    p->state = mex::FORM_ERROR;
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
        const mex::instr * instr =
	    ~ ( p->pc.module + p->pc.index );
	min::uns8 op_code = instr->op_code;
	op_info * op_info =
	    ( op_code < NUMBER_OF_OP_CODES ?
	      op_infos + op_code : NULL );
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

    p->printer << min::bol << "FATAL ERROR: "
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
	       << instr_buffer
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
    mex::process p =
        (mex::process) ::process_vec_type.new_stub
	    ( mex::stack_limit );
    mex::return_stack_ref(p) =
        (mex::return_stack)
	::return_stack_vec_type.new_stub
	    ( mex::return_stack_limit );
    mex::printer_ref(p) = printer;
    mex::pc pc = { min::NULL_STUB, 0 };
    mex::set_pc ( p, pc );
    p->optimize = false;
    p->level = 0;
    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->nargs[i] = 0;
    p->trace_depth = 0;
    p->trace_flags = 0;
    p->excepts = 0;
    p->excepts_accumulator = 0;
    p->state = mex::NEVER_STARTED;
    p->counter = 0;
    p->limit = 0;

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
    p->level = 0;
    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->nargs[i] = 0;
    p->trace_depth = 0;
    p->excepts_accumulator = 0;
    p->state = mex::NEVER_STARTED;
    p->counter = 0;
    p->limit = 0;

    return p;
}

mex::process mex::init_process
	( mex::pc pc, mex::process p )
{
    if ( p == min::NULL_STUB )
        p = mex::create_process();
    RW_UNS32 p->length = 0;
    RW_UNS32 p->return_stack->length = 0;
    p->level = 0;
    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = p->nargs[i] = 0;
    p->trace_depth = 0;
    p->excepts_accumulator = 0;
    p->state = mex::NEVER_STARTED;
    p->counter = 0;
    p->limit = 0;

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
