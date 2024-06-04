// MIN System Execution Engine Compiler Stack Support
//
// File:	mexstack.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Tue Jun  4 02:28:42 EDT 2024
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

extern min::uns32 var_stack_length;
extern min::uns32 func_stack_length;
extern min::uns32 func_var_stack_length;
void pop_stacks ( void );

extern min::uns32 lp[mex::max_lexical_level+1];
extern min::uns32 fp[mex::max_lexical_level+1];


// Block Stack
//
struct block_element
{
    min::uns8 begin_op_code;
    min::uns8 end_op_code;
    min::uns32 stack_limit;
    min::uns32 func_stack_length;
    min::uns32 func_var_stack_length;
    min::uns32 nvars;
    min::uns32 begin_location;
};
typedef min::packed_vec_insptr<mexstack::block_element>
    block_stack;
extern min::locatable_var<mexstack::block_stack>
    blocks;

extern min::uns32 stack_limit;

struct jump_element
{
    const min::gen target_name;
    min::uns32 jmp_location;
    min::uns8 lexical_level, depth, minimum_depth;
    min::uns32 var_stack_length, var_stack_minimum;
    min::uns32 next;
    jump_element
	    ( const jump_element & j ) :
	target_name ( j.target_name ),
	jmp_location ( j.jmp_location ),
	lexical_level ( j.lexical_level ),
        depth ( j.depth ),
        minimum_depth ( j.minimum_depth ),
        var_stack_length ( j.var_stack_length ),
        var_stack_minimum ( j.var_stack_minimum ),
	next ( j.next ) {}
    jump_element
	    ( min::gen target_name,
	      min::uns32 jmp_location,
	      min::uns32 lexical_level,
              min::uns32 depth,
              min::uns32 minimum_depth,
              min::uns32 var_stack_length,
              min::uns32 var_stack_minimum,
              min::uns32 next ) :
	target_name ( target_name ),
	jmp_location ( jmp_location ),
	lexical_level ( lexical_level ),
        depth ( depth ),
        minimum_depth ( minimum_depth ),
        var_stack_length ( var_stack_length ),
        var_stack_minimum ( var_stack_minimum ),
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
        ( lst, e.target_name );
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
	  min::uns32 index,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  bool no_source = false,
	  min::int32 stack_offset = 0 );

unsigned jump_list_delete
	( mexstack::jump_list jlist );
unsigned jump_list_update
	( mexstack::jump_list jlist );
unsigned jump_list_resolve
	( mexstack::jump_list jlist,
	  min::gen target_name );

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
void cont ( mex::instr & instr,
	    min::uns32 tvars,
	    min::gen trace_info = min::MISSING(),
            const min::phrase_position & pp =
	        min::MISSING_PHRASE_POSITION );

} // end mexstack namespace

# endif // MEXAS_H
