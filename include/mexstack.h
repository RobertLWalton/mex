// MIN System Execution Engine Compiler Stack Support
//
// File:	mexstack.h
// Author:	Bob Walton (walton@acm.org)
// Date:	Sun May 19 12:53:18 EDT 2024
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

# include <mex.h>


// Data
// ----

namespace mexstack {


void push_push_instr
        ( min::gen new_name, min::gen old_name,
	  min::uns32 index,
	  const min::phrase_position & pp =
	      min::MISSING_PHRASE_POSITION,
	  min::uns32 offset = 0 );

extern min::uns8 lexical_level;
extern min::uns8 depth[mex::max_lexical_level+1];

extern min::uns8 var_stack_length;
extern min::uns8 func_stack_length;
extern min::uns8 func_var_stack_length;

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

typedef min::packed_vec_insptr<mex::module>
    module_stack;
extern min::locatable_var<mexstack::module_stack>
    modules;

struct jump_element
{
    const min::gen target_name;
    min::uns32 jmp_location;
    min::uns8 lexical_level, depth, maximum_depth;
    min::uns32 var_stack_length, var_stack_minimum;
    min::uns32 next;
    jump_element
	    ( const jump_element & j ) :
	target_name ( j.target_name ),
	jmp_location ( j.jmp_location ),
	lexical_level ( j.lexical_level ),
        depth ( j.depth ),
        maximum_depth ( j.maximum_depth ),
        var_stack_length ( j.var_stack_length ),
        var_stack_minimum ( j.var_stack_minimum ),
	next ( j.next ) {}
    jump_element
	    ( min::gen target_name,
	      min::uns32 jmp_location,
	      min::uns32 lexical_level,
              min::uns32 depth,
              min::uns32 maximum_depth,
              min::uns32 var_stack_length,
              min::uns32 var_stack_minimum,
              min::uns32 next ) :
	target_name ( target_name ),
	jmp_location ( jmp_location ),
	lexical_level ( lexical_level ),
        depth ( depth ),
        maximum_depth ( maximum_depth ),
        stack_length ( var_stack_length ),
        stack_minimum ( var_stack_minimum ),
	next ( next ) {}
    jump_element & operator =
	    ( const jump_element & e )
    {
        // Implicit operator = not defined because
	// of const members.
	//
	new jump_element ( this ) ( e );
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