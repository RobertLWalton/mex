// MIN System Execution Engine Compiler Stack Support
//
// File:	mexstack.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jan 23 02:23:41 AM EST 2025
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup
//	Data
//	Support Functions


// Setup
// -----

# ifndef MEXSTACK_H
# define MEXSTACK_H

# include <mexcom.h>


// Data
// ----

namespace mexstack {


extern min::uns8 lexical_level;
extern min::uns8 depth[mex::max_lexical_level+1];

extern min::uns32 stack_length;
extern min::uns32 stack_limit;

extern min::uns32 ap[mex::max_lexical_level+1];
extern min::uns32 fp[mex::max_lexical_level+1];

struct compile_save_area
{
    min::uns32 error_count;
    min::uns32 warning_count;
    min::uns32 stack_length;
    min::uns32 stack_limit;
    min::uns32 code_length;
};

void pop_stacks ( void );
void save ( mexstack::compile_save_area & area );
bool restore ( mexstack::compile_save_area & area );

// Block Stack
//
struct block_element
{
    min::uns8 begin_op_code;
    min::uns8 end_op_code;
    min::uns32 stack_limit;
    min::uns32 nvars;
    min::uns32 begin_location;
};
typedef min::packed_vec_insptr<mexstack::block_element>
    block_stack;
extern min::locatable_var<mexstack::block_stack>
    blocks;


struct jump_element
{
    const min::gen target;
    min::uns32 jmp_location;
    min::uns8 lexical_level, depth, minimum_depth;
    min::uns32 stack_length, stack_minimum;
    min::uns32 next;
    jump_element
	    ( const jump_element & j ) :
	target ( j.target ),
	jmp_location ( j.jmp_location ),
	lexical_level ( j.lexical_level ),
        depth ( j.depth ),
        minimum_depth ( j.minimum_depth ),
        stack_length ( j.stack_length ),
        stack_minimum ( j.stack_minimum ),
	next ( j.next ) {}
    jump_element
	    ( min::gen target,
	      min::uns32 jmp_location,
	      min::uns32 lexical_level,
              min::uns32 depth,
              min::uns32 minimum_depth,
              min::uns32 stack_length,
              min::uns32 stack_minimum,
              min::uns32 next ) :
	target ( target ),
	jmp_location ( jmp_location ),
	lexical_level ( lexical_level ),
        depth ( depth ),
        minimum_depth ( minimum_depth ),
        stack_length ( stack_length ),
        stack_minimum ( stack_minimum ),
	next ( next ) {}
    jump_element & operator =
	    ( const jump_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
	new ( this ) jump_element ( e );
	return * this;
    }
};
typedef min::packed_vec_insptr<mexstack::jump_element>
    jump_list;
extern min::locatable_var<mexstack::jump_list>
    jumps;

inline void push_jump
	( mexstack::jump_list lst,
	  const mexstack::jump_element & e )
{
    mexstack::jump_element * free = ~ ( lst + 0 );
    mexstack::jump_element * active = free + 1;
    min::uns32 next = free->next;
    if ( next == 0 )
    {
        next = lst->length;
	min::push(lst) = e;
    }
    else
    {
	mexstack::jump_element * ep = ~ ( lst + next );
	free->next = ep->next;
        * ep = e;
    }
    (lst + next)->next = active->next;
    active->next = next;
    min::unprotected::acc_write_update
        ( lst, e.target );
}

// Support Functions
// ------- ---------

enum print
{
    NO_PRINT,
    PRINT,
    PRINT_WITH_SOURCE
};
extern print print_switch;
void print_instr ( min::uns32 location,
                   bool no_source = false,
		   min::int32 stack_offset = 0 );

extern bool trace_never;
inline void push_instr
        ( mex::instr & instr,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  min::gen trace_info = min::MISSING(),
	  bool no_source = false,
	  min::int32 stack_offset = 0 )
{
    MIN_ASSERT
        ( instr.op_code < mex::NUMBER_OF_OP_CODES,
	  "illegal op_code in call to mexstack::"
	  "push_instr" );
    if ( instr.trace_class == 0 )
        instr.trace_class =
	    mex::op_infos[instr.op_code].trace_class;
    if ( mexstack::trace_never
         &&
	 instr.trace_class != mex::T_ALWAYS )
        instr.trace_class = mex::T_NEVER;

    mex::module_ins m = mexcom::output_module;
    mex::push_instr ( m, instr );
    mex::push_position ( m, pp );
    mex::push_trace_info ( m, trace_info );
    if ( mexstack::print_switch != mexstack::NO_PRINT )
        mexstack::print_instr
	    ( m->length - 1, no_source, stack_offset );
}

void push_push_instr
        ( min::gen new_name, min::gen old_name,
	  min::uns32 location,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  bool no_source = false,
	  min::int32 stack_offset = 0 );

inline void push_jmp_instr
        ( mex::instr & instr,
	  min::gen target,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  bool no_source = false,
	  min::int32 success_stack_offset = 0 )
{
    mexstack::push_instr
        ( instr, pp, target, no_source, 0 );
    min::uns8 L = mexstack::lexical_level,
              D = mexstack::depth[L];
    min::uns32 S = mexstack::stack_length
                 + success_stack_offset;
    mexstack::jump_element je = {
        target,
	mexcom::output_module->length - 1,
	L, D, D, S, S, 0 };
    mexstack::push_jump ( mexstack::jumps, je );

}

void init ( void );

void begx ( mex::instr & instr,
	    min::uns32 nvars, min::uns32 tvars,
	    min::gen trace_info = min::MISSING(),
            const min::phrase_position & pp =
	       min::MISSING_PHRASE_POSITION );
unsigned endx ( mex::instr & instr,
	        min::uns32 tvars,
	        min::gen trace_info = min::MISSING(),
                const min::phrase_position & pp =
	            min::MISSING_PHRASE_POSITION );
bool cont ( mex::instr & instr,
	    min::uns32 loop_depth,
	    min::uns32 tvars,
	    min::gen trace_info = min::MISSING(),
            const min::phrase_position & pp =
	        min::MISSING_PHRASE_POSITION );

unsigned jmp_clear ( void );
unsigned jmp_update ( void );
unsigned jmp_target ( min::gen target );
void print_label ( min::gen name,
	           const min::phrase_position & pp =
	               min::MISSING_PHRASE_POSITION,
	           bool no_source = false,
		   min::int32 stack_offset = 0 );

} // end mexstack namespace

# endif // MEXAS_H
