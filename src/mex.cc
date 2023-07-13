// MIN System Execution Engine Interface
//
// File:	mex.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul 13 02:28:57 EDT 2023
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

static min::uns32 instr_gen_disp[] =
{
    min::DISP ( & mex::instr::immedD ),
    min::DISP_END
};

static min::uns32 module_gen_disp[] =
{
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

static min::packed_vec<min::gen,mex::module_header>
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
    unsigned i = ( p->trace_depth + 1 )
	       * mex::trace_indent;
    while ( 1 < i )
    {
	p->printer << mex::trace_mark;
	-- i;
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
    const mex::instr * pcbegin = ~ ( m + 0 );
    const mex::instr * pc = pcbegin + i;
    const mex::instr * pcend = pcbegin + m->length;

    i = p->length;
    if ( i > p->max_length ) return false;
    min::gen * spbegin = ~ ( p + 0 );
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
	    int i = pc->immedA;
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
	    sp = mex::process_push
	        ( p, sp, globals[i] );
	    break;
	}
	case mex::PUSHL:
	{
	    int i = pc->immedA;
	    int j = pc->immedB;
	    if ( j > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    i += p->fp[j];
	    if ( i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp ++ = spbegin[i];
	    break;
	}
	case mex::PUSHA:
	{
	    int i = pc->immedA;
	    int j = pc->immedB;
	    if ( j > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    int k = p->fp[j];
	    if ( i > k )
	        goto ERROR_EXIT;
	    k -= i;
	    if ( i >= sp - spbegin )
	        goto ERROR_EXIT;
	    * sp ++ = spbegin[k];
	    break;
	}
	case mex::PUSHNARGS:
	{
	    int rp = p->return_stack->length;
	    if ( rp == 0 )
	        goto ERROR_EXIT;
	    if ( i >= sp - spbegin )
	        goto ERROR_EXIT;
	    -- rp;
	    mex::ret * ret = ~ ( p->return_stack + rp );
	    * sp ++ = GF ( ret->nargs );
	    break;
	}
	case mex::PUSHV:
	{
	    int j = pc->immedB;
	    if ( j < 1 || j > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    if ( sp <= spbegin )
	        goto ERROR_EXIT;
	    int k = p->fp[j];
	    min::float64 f = FG ( sp[-1] );
	    min::float64 ff = floor ( f );
	    if ( std::isnan ( ff )
	         ||
	         f != ff
		 ||
	         ff <= 0 || ff > k - p->fp[j-1] )
	    {
	        sp[-1] = GF ( NAN );
		feraiseexcept ( FE_INVALID );
	    }
	    else
	    {
		int i = (int) ff;
		k -= i;
		sp[-1] = spbegin[k];
	    }
	    break;
	}
	case mex::POPS:
	{
	    int i = pc->immedA;
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
	case mex::BEGF:
	{
	    int immedB = pc->immedB;
	    int immedC = pc->immedC;
	    if ( immedC > pc - pcbegin )
	        goto ERROR_EXIT;
	    if ( immedB > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    pc += immedC;
	    -- pc;
	}
	case mex::ENDF:
	case mex::RET:
	{
	    int immedA = pc->immedA;
	    int immedB = pc->immedB;
	    int immedC = pc->immedC;
	    if ( op_code == mex::ENDF ) immedC = 0;
	    min::uns32 rp = p->return_stack->length;
	    if ( immedA > sp - spbegin )
	        goto ERROR_EXIT;
	    if ( immedB > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    if ( immedC > immedA )
	        goto ERROR_EXIT;
	    if ( rp == 0 )
	        goto ERROR_EXIT;
	    -- rp;
	    const mex::ret * ret =
	       ~ ( p->return_stack + rp );
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
	    min::gen * new_sp = sp - immedA;
	    min::uns32 new_fp = ret->saved_fp;
	    if ( new_fp > new_sp - spbegin )
		goto ERROR_EXIT;

	    mex::set_pc ( p, ret->saved_pc );
	    p->fp[immedB] = new_fp;
	    RW_UNS32 p->return_stack->length = rp;

	    min::gen * q = new_sp + immedC;
	    while ( q > new_sp )
	        * -- q = * -- sp;
	    sp = new_sp;

	    if ( em == min::NULL_STUB )
	        goto RET_EXIT;

            m = em;
	    pcbegin = ~ ( m + 0 );
	    pc = pcbegin + new_pc;
	    pcend = pcbegin + m->length;
	    -- pc;
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
	    int level = target->immedB;
	    if ( level > mex::max_lexical_level )
	        goto ERROR_EXIT;
	    min::uns32 rp = p->return_stack->length;
	    if ( rp >= p->return_stack->max_length )
	        goto ERROR_EXIT;

	    mex::ret * ret = ~ ( p->return_stack + rp );
	    mex::pc new_pc =
	        { m, (min::uns32)
		     ( pc - pcbegin + 1 ) };
	    mex::set_saved_pc ( p, ret, new_pc );
	    ret->saved_fp = p->fp[level];
	    ret->nargs = pc->immedA;
	    ret->nresults = pc->immedB;
	    RW_UNS32 p->return_stack->length = rp + 1;

	    new_pc = { cm, immedC + 1 };
	    mex::set_pc ( p, new_pc );
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
enum
{
    NONA = 1,	// No arithmetic operands, not a JMP.
    A2 =   2,	// sp[0] and sp[-1] are the arithmetic
                // operands in that order.
    A2R =  3,	// sp[-1] and sp[0] are the arithmetic
                // operands in that order.
    A2I =  4,	// sp[0] and immedD are the arithmetic
                // operands in that order.
    A2RI = 5,	// immedD and sp[0] are the arithmetic
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
    { mex::ADDI, A2I, "ADDI", "+" },
    { mex::MUL, A2, "MUL", "*" },
    { mex::MULI, A2I, "MULI", "*" },
    { mex::SUB, A2, "SUB", "-" },
    { mex::SUBR, A2R, "SUBR", "-" },
    { mex::SUBI, A2I, "SUBI", "-" },
    { mex::SUBRI, A2RI, "SUBRI", "-" },
    { mex::DIV, A2, "DIV", "/" },
    { mex::DIVR, A2R, "DIVR", "/" },
    { mex::DIVI, A2I, "DIVI", "/" },
    { mex::DIVRI, A2RI, "DIVRI", "/" },
    { mex::MOD, A2, "MOD", "fmod" },
    { mex::MODR, A2R, "MODR", "fmod" },
    { mex::MODI, A2I, "MODI", "fmod" },
    { mex::MODRI, A2RI, "MODRI", "fmod" },
    { mex::FLOOR, A1, "FLOOR", "floor" },
    { mex::CEIL, A1, "CEIL", "ceil" },
    { mex::TRUNC, A1, "TRUNC", "trunc" },
    { mex::ROUND, A1, "ROUND", "round" },
    { mex::NEG, A1, "NEG", "-" },
    { mex::ABS, A1, "ABS", "abs" },
    { mex::LOG, A1, "LOG", "log" },
    { mex::LOG10, A1, "LOG10", "log10" },
    { mex::EXP, A1, "EXP", "exp" },
    { mex::EXP10, A1, "EXP10", "exp10" },
    { mex::SIN, A1, "SIN", "sin" },
    { mex::ASIN, A1, "ASIN", "asin" },
    { mex::COS, A1, "COS", "cos" },
    { mex::ACOS, A1, "ACOS", "acos" },
    { mex::TAN, A1, "TAN", "tan" },
    { mex::ATAN, A1, "ATAN", "atan" },
    { mex::ATAN2, A2, "ATAN2", "atan2" },
    { mex::ATAN2R, A2R, "ATAN2R", "atan2" },
    { mex::POWI, A1, "POWI", "pow" },
    { mex::PUSHS, NONA, "PUSHS", NULL },
    { mex::PUSHL, NONA, "PUSHL", NULL },
    { mex::PUSHI, NONA, "PUSHI", NULL },
    { mex::PUSHG, NONA, "PUSHG", NULL },
    { mex::POPS, NONA, "POPS", NULL },
    { mex::JMP, J, "JMP", NULL },
    { mex::JMPEQ, J2, "JMPEQ", "==" },
    { mex::JMPNE, J2, "JMPNE", "!=" },
    { mex::JMPLT, J2, "JMPLT", "<" },
    { mex::JMPLEQ, J2, "JMPLEQ", "<=" },
    { mex::JMPGT, J2, "JMPGT", ">" },
    { mex::JMPGEQ, J2, "JMPGEQ", ">=" },
    { mex::BEG, NONA, "BEG", NULL },
    { mex::NOP, NONA, "NOP", NULL },
    { mex::END, NONA, "END", NULL },
    { mex::BEGL, NONA, "BEGL", NULL },
    { mex::ENDL, NONA, "ENDL", NULL },
    { mex::CONT, NONA, "CONT", NULL },
    { mex::SET_TRACE, NONA, "SET_TRACE", NULL },
    { mex::ERROR, NONA, "ERROR", NULL },
    { mex::BEGF, NONA, "BEGF", NULL },
    { mex::ENDF, NONA, "ENDF", NULL },
    { mex::CALLM, NONA, "CALLM", NULL },
    { mex::CALLG, NONA, "CALLG", NULL },
    { mex::RET, NONA, "RET", NULL },
    { mex::PUSHA, NONA, "PUSHA", NULL },
    { mex::PUSHNARGS, NONA, "PUSHNARGS", NULL },
    { mex::PUSHV, A1, "PUSHV", "pushv" },
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
    pcbegin = ~ ( m + 0 );
    pc = pcbegin + i;
    pcend = pcbegin + m->length;

    i = p->length;
    if ( i > p->max_length )
    {
	message = "Illegal SP: too large";
	goto FATAL;
    }
    spbegin = ~ ( p + 0 );
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
	pcbegin = ~ ( m + 0 ); \
	pc = pcbegin + p->pc.index; \
	pcend = pcbegin + m->length; \
	spbegin = ~ ( p + 0 ); \
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
		int j = pc->immedB;
		if (    j < 1
		     || j > mex::max_lexical_level )
		{
		    message = "invalid immedB";
		    goto INNER_FATAL;
		}
		int k = p->fp[j];
		min::float64 ff = floor ( arg1 );
		if ( std::isnan ( ff )
		     ||
		     arg1 != ff
		     ||
		     ff <= 0 || ff > k - p->fp[j-1] )
		{
		    result = NAN;
		    feraiseexcept ( FE_INVALID );
		}
		else
		{
		    int i = (int) ff;
		    k -= i;
		    result = MUP::direct_float_of
		                ( spbegin[k] );
		}
		break;
	    }
	    } // end switch ( op_code )

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

		if ( pp
		     &&
		     (   trace_flags
		       & mex::TRACE_PHRASE ) )
		    min::print_phrase_lines
		        ( p->printer,
			  m->position->file,
			  pp );

		print_indent ( p );
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
		     (   trace_flags
		       & mex::TRACE_PHRASE ) )
		    min::print_phrase_lines
		        ( p->printer,
			  m->position->file,
			  pp );
		print_indent ( p );
		p->printer << min::bom;

		if ( pp )
		    p->printer
		        << min::pline_numbers
			    ( m->position->file, pp )
			<< ": ";

		min::gen tinfo  = min::MISSING();
		if (   p->pc.index
		     < m->trace_info->length)
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
		    (* p->trace_function) ( p, tinfo );

		p->printer << min::eom;

		if ( bad_jmp )
		{
		    p->state = mex::JMP_ERROR;
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
	    case mex::PUSHL:
		if ( immedB > mex::max_lexical_level )
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
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		break;
	    case mex::PUSHA:
		if ( immedB > mex::max_lexical_level )
		{
		    message = "immedB too large";
		    goto INNER_FATAL;
		}
		if ( immedA > p->fp[i] )
		{
		    message = "immedA too large";
		    goto INNER_FATAL;
		}
		break;
	    case mex::PUSHNARGS:
		if ( p->return_stack->length == 0 )
		{
		    message = "return stack empty";
		    goto INNER_FATAL;
		}
		if ( sp >= spend )
		{
		    message =
		        "stack too large for push";
		    goto INNER_FATAL;
		}
		break;
	    case mex::POPS:
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
	    case mex::BEGF:
	        if ( immedC > pcend - pc )
		{
		    message = "immedC too large";
		    goto INNER_FATAL;
		}
	        if ( immedB > mex::max_lexical_level )
		{
		    message = "immedB too large";
		    goto INNER_FATAL;
		}
	    case mex::RET:
	    case mex::ENDF:
	    {
		int immedA = pc->immedA;
		int immedB = pc->immedB;
		int immedC = pc->immedC;
		if ( op_code == mex::ENDF ) immedC = 0;
		min::uns32 rp = p->return_stack->length;
		if ( immedA > sp - spbegin )
		{
		    message = "immedA is too large";
		    goto INNER_FATAL;
		}
		if ( immedB > mex::max_lexical_level )
		{
		    message = "immedA is too large";
		    goto INNER_FATAL;
		}
		if ( immedC > immedA )
		{
		    message =
		        "immedC is larger than immedA";
		    goto INNER_FATAL;
		}
		if ( rp == 0 )
		{
		    message = "return stack is empty";
		    goto INNER_FATAL;
		}
		-- rp;
		const mex::ret * ret =
		   ~ ( p->return_stack + rp );
		mex::module em = ret->saved_pc.module;
		min::uns32 new_pc = ret->saved_pc.index;
		if ( em == min::NULL_STUB )
		{
		    if ( new_pc != 0 )
			goto INNER_FATAL;
		    {
			message = "bad saved_pc value";
			goto INNER_FATAL;
		    }
		}
		else
		{
		    if ( new_pc > em->length )
			goto INNER_FATAL;
		    {
			message = "bad saved_pc value";
			goto INNER_FATAL;
		    }
		}
		min::gen * new_sp = sp - immedA;
		min::uns32 new_fp = ret->saved_fp;
		if ( new_fp > new_sp - spbegin )
		{
		    message = "bad saved_fp value";
		    goto INNER_FATAL;
		}
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
		int level = target->immedB;
		if ( level > mex::max_lexical_level )
		{
		    message =
		        "BEGF immedB is too large";
		    goto INNER_FATAL;
		}
		min::uns32 rp = p->return_stack->length;
		if ( rp >= p->return_stack->max_length )
		{
		    message = "return stack is full";
		    goto INNER_FATAL;
		}
	    }

	    } // end switch ( op_code )

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
		     (   trace_flags
		       & mex::TRACE_PHRASE ) )
		    min::print_phrase_lines
		        ( p->printer,
			  m->position->file,
			  pp );
		print_indent ( p );
		p->printer << min::bom;

		if ( pp )
		    p->printer
		        << min::pline_numbers
			    ( m->position->file, pp )
			<< ": ";

		min::gen tinfo  = min::MISSING();
		if (   p->pc.index
		     < m->trace_info->length )
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
		    (* p->trace_function)
		        ( p, tinfo );     

		p->printer << min::eom;

		if ( fatal_error )
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
	    {
		int i = pc->immedA;
		* sp = sp[-i-1];
		++ sp;
		break;
	    }
	    case mex::PUSHI:
	    {
		sp = mex::process_push
		    ( p, sp, pc->immedD );
		break;
	    }
	    case mex::PUSHG:
	    {
	        mex::module mg =
		    (mex::module) pc->immedD;
		sp = mex::process_push
		    ( p, sp, mg->globals[pc->immedA] );
		break;
	    }
	    case mex::PUSHL:
	        * sp ++ =
		    spbegin[p->fp[pc->immedB] + immedA];
		break;
	    case mex::PUSHA:
	    {
		* sp ++ =
		    spbegin[p->fp[pc->immedB] - immedA];
		break;
	    }
	    case mex::PUSHNARGS:
	    {
		int rp = p->return_stack->length;
		-- rp;
		mex::ret * ret =
		    ~ ( p->return_stack + rp );
		* sp ++ = min::new_direct_float_gen
		               ( ret->nargs );
		break;
	    }
	    case mex::POPS:
	    {
		int i = pc->immedA;
		-- sp;
		sp[-i] = * sp;
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
	    {
		int immedA = pc->immedA;
	        sp -= immedA;
	        break;
	    }
	    case mex::BEGF:
	    {
		int immedC = pc->immedC;
		pc += immedC;
		-- pc;
		break;
	    }
	    case mex::RET:
	    case mex::ENDF:
	    {
		int immedA = pc->immedA;
		int immedB = pc->immedB;
		int immedC = pc->immedC;
		if ( op_code == mex::ENDF ) immedC = 0;
		min::uns32 rp = p->return_stack->length;
		-- rp;
		const mex::ret * ret =
		    ~ ( p->return_stack + rp );
		mex::module em = ret->saved_pc.module;
		min::uns32 new_pc = ret->saved_pc.index;
		min::uns32 new_fp = ret->saved_fp;

		mex::set_pc ( p, ret->saved_pc );
		p->fp[immedB] = new_fp;
		RW_UNS32 p->return_stack->length = rp;

		min::gen * new_sp = sp - immedA;
		min::gen * q = new_sp + immedC;
		while ( q > new_sp )
		    * -- q = * -- sp;
		sp = new_sp;

		if ( em == min::NULL_STUB )
		{
		    RET_SAVE;
		    p->state = mex::CALL_END;
		    return true;
		}

		m = em;
		pcbegin = ~ ( m + 0 );
		pc = pcbegin + new_pc;
		pcend = pcbegin + m->length;
		-- pc;
	    }
	    case mex::CALLM:
	    case mex::CALLG:
	    {
		min::uns32 immedC = pc->immedC;
		mex::module cm =
		    ( op_code == mex::CALLG ?
		      pc->immedD :
		      m );
		const mex::instr * target =
		    ~ ( cm + immedC );
		int level = target->immedB;
		min::uns32 rp = p->return_stack->length;

		mex::ret * ret =
		    ~ ( p->return_stack + rp );
		mex::pc new_pc =
		    { m, (min::uns32)
			 ( pc - pcbegin + 1 ) };
		mex::set_saved_pc ( p, ret, new_pc );
		ret->saved_fp = p->fp[level];
		ret->nargs = pc->immedA;
		ret->nresults = pc->immedB;
		RW_UNS32 p->return_stack->length =
		    rp + 1;

		new_pc = { cm, immedC + 1 };
		mex::set_pc ( p, new_pc );
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

mex::process mex_create_process ( min::printer printer )
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
    p->trace_function = NULL;
    for ( unsigned i = 0;
          i <= mex::max_lexical_level; ++ i )
        p->fp[i] = 0;
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

