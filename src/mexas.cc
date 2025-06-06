// MIN System Execution Engine Assembler
//
// File:	mexas.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Mon May 26 11:27:57 PM EDT 2025
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// Table of Contents:
//
//	Setup and Data
//	Support Functions
//	Scanner Function
//	Compile Function


// Setup and Data
// ----- --- ----

# include <mexas.h>
# include <mexcom.h>
# include <cmath>
# include <cfenv>

# define L mexstack::lexical_level
# define SP mexstack::stack_length
# define LEVEL_AND_DEPTH \
    (   ( (min::uns32) L << 16 ) \
      + (min::uns32) mexstack::depth[L] )

# define MUP min::unprotected

void mexstack::pop_stacks ( void )
{
    min::uns32 level_and_depth = LEVEL_AND_DEPTH;
    min::uns32 length = mexas::variables->length;
    while ( length > 0 )
    {
        mexas::variable_element * ve =
	    ~( mexas::variables + ( length - 1 ) );
	if ( ve->level_and_depth <= level_and_depth )
	    break;
	-- length;
    }
    min::pop ( mexas::variables,
	       mexas::variables->length - length );

    length = mexas::functions->length;
    while ( length > 0 )
    {
        mexas::function_element * fe =
	    ~( mexas::functions + ( length - 1 ) );
	if ( fe->level_and_depth <= level_and_depth )
	    break;
	-- length;
    }
    min::pop ( mexas::functions,
    	       mexas::functions->length - length );
}

min::locatable_gen mexas::star;
min::locatable_gen mexas::V;
min::locatable_gen mexas::F;

enum op_code
    // Extends mex::op_code to include pseudo-ops and
    // declarations.
{
    PUSHM = mex::NUMBER_OF_OP_CODES,
    PUSH,
    POP,
    CALL,
    LABEL,
    TEST_INSTRUCTION,
    STACKS,
    NUMBER_OF_OP_CODES
};

static struct extended_op
  { const char * name; ::op_code op_code; }
    extended_ops[] = {
    { "PUSHM", ::PUSHM },
    { "PUSH", ::PUSH },
    { "POP", ::POP },
    { "CALL", ::CALL },
    { "LABEL", ::LABEL },
    { "TEST_INSTRUCTION", ::TEST_INSTRUCTION },
    { "STACKS", ::STACKS }
    };

static void init_op_code_table ( void )
{
    mexcom::init_op_code_table();

    min::obj_vec_insptr vp ( mexcom::op_code_table );
    min::attr_insptr ap ( vp );

    extended_op * p = ::extended_ops;
    extended_op * endp =
        p + (   ::NUMBER_OF_OP_CODES
	      - mex::NUMBER_OF_OP_CODES );
    min::locatable_gen tmp;
    while  ( p < endp )
    {
        tmp = min::new_str_gen ( p->name );
        min::attr_push(vp) = tmp;
	min::locate ( ap, tmp );
	tmp = min::new_num_gen ( p->op_code );
	min::set ( ap, tmp );
	++ p;
    }
}

static min::uns32 variable_element_gen_disp[] =
{
    min::DISP ( & mexas::variable_element::name ),
    min::DISP_END
};

static min::packed_vec<mexas::variable_element>
     variable_stack_vec_type
         ( "variable_stack_vec_type",
	   ::variable_element_gen_disp );

min::locatable_var<mexas::variable_stack>
    mexas::variables;

static min::uns32 function_element_gen_disp[] =
{
    min::DISP ( & mexas::function_element::name ),
    min::DISP_END
};

static min::packed_vec<mexas::function_element>
     function_stack_vec_type
         ( "function_stack_vec_type",
	   ::function_element_gen_disp );

min::locatable_var<mexas::function_stack>
    mexas::functions;

min::locatable_var<mexas::statement_lexemes>
    mexas::statement;
min::uns32 mexas::first_line_number,
           mexas::last_line_number;

min::locatable_gen mexas::single_quote;
min::locatable_gen mexas::double_quote;
min::locatable_gen mexas::left_bracket;
min::locatable_gen mexas::right_bracket;
static min::locatable_gen comma;
static min::locatable_gen backslash;
static min::locatable_gen on;
static min::locatable_gen off;
static min::locatable_gen loop;
static min::locatable_gen TRUE;
static min::locatable_gen FALSE;
static min::locatable_gen NONE;

static void initialize ( void )
{
    mexas::star = min::new_str_gen ( "*" );
    mexas::V = min::new_str_gen ( "V" );
    mexas::F = min::new_str_gen ( "F" );
    mexas::single_quote = min::new_str_gen ( "'" );
    mexas::double_quote = min::new_str_gen ( "\"" );
    mexas::left_bracket = min::new_str_gen ( "[" );
    mexas::right_bracket = min::new_str_gen ( "]" );
    ::comma = min::new_str_gen ( "," );
    ::backslash = min::new_str_gen ( "\\" );
    ::on = min::new_str_gen ( "ON" );
    ::off = min::new_str_gen ( "OFF" );
    ::loop = min::new_str_gen ( "LOOP" );
    ::TRUE = min::new_str_gen ( "TRUE" );
    ::FALSE = min::new_str_gen ( "FALSE" );
    ::NONE = min::new_str_gen ( "NONE" );

    ::init_op_code_table();

    mexas::variables =
	::variable_stack_vec_type.new_stub ( 1000 );
    mexas::functions =
	::function_stack_vec_type.new_stub ( 100 );
    mexas::statement =
	min::gen_packed_vec_type.new_stub ( 500 );
}
static min::initializer initializer ( ::initialize );


// Support Functions
// ------- ---------

bool mexas::check_new_name
	( min::gen name, min::phrase_position pp )
{
    if ( name == mexas::star ) return true;

    if (    mexas::search
                ( name, mexstack::stack_limit )
         == mexas::NOT_FOUND )
        return true;

    mexcom::compile_error
	( pp,
	  "new variable with name ",
	  min::pgen ( name ),
	  " improperly hides previous variable" );
    return false;
}

min::uns32 mexas::local_search
    ( min::gen name,
      min::phrase_position pp )
{
    min::uns32 j = search ( name, SP );
    if ( j != mexas::NOT_FOUND
         &&
	 j >= mexstack::fp[mexstack::lexical_level] )
        return j;

    mexcom::compile_error
	( pp, "variable named ",
	      min::pgen ( name ),
	      " not defined within current function"
	      " frame; instruction ignored" );
    return mexas::NOT_FOUND;
}

min::uns32 mexas::global_search
	( mex::module & m, min::gen module_name,
	                   min::gen type,
			   min::gen name )
{
    min::gen labbuf[2] = { type, name };
    min::locatable_gen label
        ( min::new_lab_gen ( labbuf, 2 ) );

    for ( min::uns32 i = mexcom::modules->length;
          0 < i; )
    {
	-- i;
        m = mexcom::modules[i];
	if ( m->name != module_name
	     &&
	     mexas::star != module_name )
	    continue;
	if ( ! min::is_obj ( m->interface ) )
	    continue;
	min::gen result =
	    min::get ( m->interface, label );
	if ( result == min::NONE() )
	    continue;
	return (min::uns32) min::int_of ( result );
    }
    return mexas::NOT_FOUND;
}

// Find a module given an module_name.  Searches most
// recently compiled first.  Returns the module, or
// NULL_STUB if none found.
//
static mex::module module_search
	( min::gen module_name )
{
    for ( min::uns32 i = mexcom::modules->length;
          0 < i; )
    {
	-- i;
        if ( mexcom::modules[i]->name == module_name )
	    return mexcom::modules[i];
    }
    return min::NULL_STUB;
}

void mexas::make_module_interface ( void )
{
    min::locatable_gen interface
        ( min::new_obj_gen ( 16000, 4000 ) );

    for ( min::uns32 i = 0;
          i < mexas::variables->length; ++ i )
    {
	min::gen name = (mexas::variables + i)->name;
	if ( name == mexas::star ) continue;

	// Overrides previous setting.
	//
	min::gen labbuf[2] = { mexas::V, name };
	min::locatable_gen label
	    ( min::new_lab_gen ( labbuf, 2 ) );
	min::locatable_gen index
	    ( min::new_num_gen ( i ) );
	min::set ( interface, label, index );
    }

    for ( min::uns32 i = 0;
          i < mexas::functions->length; ++ i )
    {
	min::ptr<mexas::function_element> p =
	    mexas::functions + i;

	min::gen labbuf[2] = { mexas::F, p->name };
	min::locatable_gen label
	    ( min::new_lab_gen ( labbuf, 2 ) );
	min::locatable_gen index
	    ( min::new_num_gen ( p->index ) );
	min::set ( interface, label, index );
    }

    mex::interface_ref ( mexcom::output_module ) =
        interface;
}

void mexas::push_push_instr
        ( min::gen new_name, min::gen name,
	  const min::phrase_position & pp,
	  bool no_source,
	  min::int32 stack_offset )
{
    min::uns32 i = search ( name, SP );
    if ( i != mexas::NOT_FOUND )
    {
        if ( (mexas::variables+i)->is_write_only )
	    mexcom::compile_error
		(    mexstack::print_switch
		  == mexstack::PRINT_WITH_SOURCE ?
		      min::MISSING_PHRASE_POSITION : pp,
		  "reading write-only variable named ",
		  min::pgen ( name ) );
    	mexstack::push_push_instr
	    ( new_name, name, i, pp,
	      no_source, stack_offset );
	return;
    }

    mex::instr instr =
	{ 0, 0, 0, 0, 0, 0, 0, min::MISSING() };
    min::gen labbuf[3] = { name };
    min::locatable_gen trace_info;

    mex::module gm;
    i = mexas::global_search
	( gm, mexas::star, mexas::V, name );
    if ( i != mexas::NOT_FOUND )
    {
	instr.immedA = i;
	instr.immedD = min::new_stub_gen ( gm );
	instr.op_code = mex::PUSHG;
	instr.trace_class = mex::T_PUSH;

	labbuf[1] = gm->name;
	labbuf[2] = new_name;
	trace_info = new_lab_gen ( labbuf, 3 );
    }
    else
    {
	mexcom::compile_error
	    (    mexstack::print_switch
	      == mexstack::PRINT_WITH_SOURCE ?
	          min::MISSING_PHRASE_POSITION : pp,
	      "variable named ",
	      min::pgen ( name ),
	      " not defined; instruction"
	      " changed to PUSHI missing"
	      " value" );
	instr.op_code = mex::PUSHI;
	instr.trace_class = mex::T_PUSH;
	trace_info = min::MISSING();
    }
    mexstack::push_instr
        ( instr, pp, trace_info,
	  no_source, stack_offset );
}

min::uns32 mexas::get_trace_info
    ( min::ref<min::gen> trace_info,
      min::uns32 & i,
      const min::phrase_position & pp )
{
    trace_info = mexas::get_trace_info ( i );
    if ( trace_info == min::MISSING() )
        return 0;
    min::lab_ptr labp ( trace_info );
    MIN_ASSERT ( labp != min::NULL_STUB,
                 "bad trace_info" );
    min::uns32 len = min::lablen ( labp );
    for ( min::uns32 j = 1; j < len; ++ j )
    {
        min::gen n = labp[j];
	if ( mexas::is_name ( n ) )
	{
	    mexas::push_push_instr
	        ( mexas::star, n, pp, true, j );
	}
	else
	{
	    mexcom::compile_error
		( pp, "not a name: ",
		      min::pgen ( n ),
		      "; PUSHI missing value output" );
	    mex::instr instr =
		{ mex::PUSHI, 0, 0, 0, 0, 0, 0,
		              min::MISSING() };
	    mexstack::push_instr
	        ( instr, pp, min::MISSING(),
		  true, j - 1 );
	}
    }
    return len - 1;
}

bool mexas::check_parameter
	( min::uns32 & param, min::gen n,
	  const min::phrase_position & pp,
	  const char * pname, bool is_level )
{
    min::float64 nf = MUP::direct_float_of ( n );

    if ( std::isnan ( nf )
	 ||
	 nf != (min::int64) nf )
    {
	mexcom::compile_error
	    ( pp, "bad ", min::pnop, pname, min::pnop,
	          " parameter; statement ignored" );
	return false;
    }

    if ( is_level )
    {
        if ( - L < nf && nf <= 0 )
	{
	    param = L + nf;
	    return true;
	}
	else if ( 1 <= nf && nf <= L )
	{
	    param = nf;
	    return true;
	}
    }
    else if ( 0 <= nf && nf < (1ul << 32) )
    {
	param = nf;
	return true;
    }

    mexcom::compile_error
	( pp, pname, min::pnop,
	      " parameter out of range;"
	      " statement ignored" );
    return false;
}

static bool check_pop
	( min::uns32 nvars, min::phrase_position pp )
{
    if ( SP < mexstack::stack_limit + nvars )
    {
	mexcom::compile_error
	    ( pp, "stack locations"
		  " to be popped or moved"
		  " are arguments"
		  " or below"
		  " current lexical"
		  " level or depth;"
		  " instruction"
		  " ignored" );
	return false;
    }
    else
        return true;
}


// Scanner Function
// ------- --------

static void scan_error
        ( const char * message,
	  const char * header = "ERROR" )
{
    mexcom::printer
        << min::bol << min::bom << header << ": "
	<< min::place_indent ( 0 )
	<< "line " << mexas::last_line_number + 1
	<< ": " << message << min::eom;
    print_line ( mexcom::printer,
                 mexcom::input_file,
		 mexas::last_line_number );
}

static void scan_warning
        ( const char * message )
{
    ::scan_error ( message, "WARNING" );
}

bool mexas::next_statement ( void )
{
    min::pop ( mexas::statement,
               mexas::statement->length );
    min::packed_vec_ptr<char> buffer =
        mexcom::input_file->buffer;

    bool statement_started = false;
        // A statement is started by any non-blank line.
    bool continuation_mark_found = false;
        // I.e., ::backslash.

    while ( true )
    {
        // Process next line.

	if ( ! statement_started )
	    mexas::first_line_number =
	        mexcom::input_file->next_line_number;
	mexas::last_line_number =
	    mexcom::input_file->next_line_number;
	min::uns32 begin_offset =
	    min::next_line ( mexcom::input_file );
	min::uns32 end_offset =
	    mexcom::input_file->next_offset - 1;
	    // Do not include NUL character.
	if ( begin_offset == min::NO_LINE )
	{
	    begin_offset = min::remaining_offset
		( mexcom::input_file );
	    end_offset = begin_offset
	               + min::remaining_length
			     ( mexcom::input_file );
	    if ( begin_offset == end_offset )	
	    {
	        // EOF
		//
		if ( continuation_mark_found )
		    ::scan_warning
			( "file ended while seeking"
			  " statement continuation;"
			  " statement terminated" );
		return statement->length != 0;
	    }
	    else min::skip_remaining
	             ( mexcom::input_file );
	}

	statement_started = true;

	char work[end_offset - begin_offset + 10];

	const char * p = ~ min::begin_ptr_of ( buffer );
	const char * endp = p + end_offset;
	p += begin_offset;
	    // buffer + XX is illegal in case of partial
	    // line at end of file and XX == end_offset.
	bool illegal_character_found = false;
	bool lexeme_found = false;

#	define SAVE \
	    begin_offset = end_offset - ( endp - p );
#	define RESTORE \
	    p = ~ min::begin_ptr_of ( buffer ); \
	    endp = p + end_offset; \
	    p += begin_offset;

	while ( true )
	{
	    // Loop through lexemes in a line.

	    // Skip whitespace.
	    //
	    while ( p < endp && std::isspace ( * p ) )
	        ++ p;

	    if ( p >= endp ) goto END_LINE;

	    // Scan lexeme into work.
	    //
	    char * q = work;
	    char type = * p;
	    if ( type == '\'' || type == '"' )
	    {
	        ++ p;
		while ( p < endp && * p != type )
		{
		    char c = * p ++;
		    if ( ( c < ' ' || c >= 0177 )
		         &&
			 ! isspace ( c ) )
		    {
		        c = '#';
			illegal_character_found = true;
		    }
		    * q ++ = c;
		}
		if ( p == endp )
		{
		    SAVE;
		    ::scan_warning
		        ( "string does not have"
			  " string-ending quote;"
			  " quote added" );
		    RESTORE;
		}
		else ++ p;
	    }
	    else
	    {
	        type = 0;
		while ( p < endp && ! isspace ( * p ) )
		{
		    char c = * p ++;
		    if ( ( c < ' ' || c >= 0177 )
		         &&
			 ! isspace ( c ) )
		    {
		        c = '#';
			illegal_character_found = true;
		    }
		    * q ++ = c;
		}
	    }
	    * q = 0;
	   
	    if ( type == 0
		 &&
		 work[0] == '/'
		 &&
		 work[1] == '/' )
	    {
	        // Comment
		//
		while ( p < endp )
		{
		    char c = * p ++;
		    if ( ( c < ' ' || c >= 0177 )
		         &&
			 ! isspace ( c ) )
			illegal_character_found = true;
		}
		goto END_LINE;
	    }

	    lexeme_found = true;

	    // Put lexeme in statement.
	    //
	    SAVE;
	    if ( type != 0 )
	    {
	        min::push ( statement ) =
		    ( type == '\'' ?
		      mexas::single_quote :
		      mexas::double_quote );
		min::push ( statement ) =
		    min::new_str_gen ( work );
	    }
	    else
	    {
	        char * endptr;
		double val = std::strtod
		    ( work, & endptr );
		if ( * endptr == 0 )
		    min::push ( statement ) =
			min::new_num_gen ( val );
		else
		    min::push ( statement ) =
			min::new_str_gen ( work );
	    }
	    RESTORE;
	}

	END_LINE:

	if ( illegal_character_found )
	    ::scan_error
		( "illegal character found in line;"
		  " changed to `#'" );

	if ( lexeme_found )
	{
	    if (    statement[statement->length - 1]
	         == ::backslash )
	    {
		continuation_mark_found = true;
		min::pop ( statement );
	    }
	    else
	        break; // End of statement.
	}
    }

    return true;
}

// Compile Function
// ------- --------

mex::module mexas::compile ( min::file file )
{
    mexcom::error_count = 0;
    mexcom::warning_count = 0;

    mexstack::init();

    min::pop ( mexas::variables,
               mexas::variables->length );
    min::pop ( mexas::functions,
               mexas::functions->length );

    mexcom::input_file = file;

    mexcom::output_module = (mex::module_ins)
        mex::create_module ( file );
    mex::module_ins m = mexcom::output_module;

    while ( next_statement() )
    {
	mex::instr instr =
	    { 0, 0, 0, 0, 0, 0, 0, min::MISSING() };
        min::phrase_position pp =
	    { { mexas::first_line_number, 0 },
	      { mexas::last_line_number + 1, 0 } };
	min::uns32 index = 1;
	    // statement[index] is next lexeme

        min::gen v = min::get
	    ( mexcom::op_code_table,
	      mexas::statement[0] );
	if ( v == min::NONE() )
	{
	    mexcom::compile_error
	        ( pp, "undefined operation code"
		      " or declaration name;"
		      " statement ignored" );
	    continue;
	}
	min::uns32 op_code =
	    (min::uns32) min::int_of ( v );

        min::uns8 op_type = mex::NONA;
	if ( op_code < mex::NUMBER_OF_OP_CODES )
	{
	    op_type = mex::op_infos[op_code].op_type;
	    instr.op_code = op_code;
	}

	min::gen top_name = min::NONE();
	    // If not NONE, JUMP does extra push at end.
	min::int32 success_stack_offset = 0;
	    // For JUMP.

	switch ( op_type )
	{
	case mex::NONA:
	    goto NON_ARITHMETIC;
	case mex::A2:
	case mex::A2R:
	    if ( SP < mexstack::stack_limit + 2 )
	        goto STACK_TOO_SHORT;
	    min::pop ( variables, 2 );
	    mexstack::stack_length -= 2;
	    goto ARITHMETIC;

	case mex::A2I:
	case mex::A2RI:
	    instr.immedD = mexas::get_num ( index );
	    if ( instr.immedD == min::NONE() )
	    {
		mexcom::compile_error
		    ( pp, "no immediate value given;"
		          " zero assumed" );
		instr.immedD == min::new_num_gen ( 0 );
	    }
	    /* FALLTHRU */
	case mex::A1:
	    if ( SP < mexstack::stack_limit + 1 )
	        goto STACK_TOO_SHORT;
	    min::pop ( variables );
	    mexstack::stack_length -= 1;
	    goto ARITHMETIC;

	case mex::J1:
	    if ( SP < mexstack::stack_limit + 1 )
	        goto STACK_TOO_SHORT;
	    if ( mexas::get_star ( index )
	         ==
		 mexas::star )
		instr.immedB = 1;
	    min::pop ( variables, 1 );
	    mexstack::stack_length -= 1;
	    goto JUMP;

	case mex::JS:
	    {
		min::gen name =
		    mexas::get_name ( index );
		if ( name == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no variable name:"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 j =
		    mexas::local_search ( name, pp );
		if ( j == mexas::NOT_FOUND )
		    continue;
		instr.immedB = SP - j - 1;

		instr.immedD = mexas::get_num ( index );
		if ( instr.immedD == min::NONE() )
		    instr.immedD ==
		        min::new_num_gen ( 1 );
	    }
	    goto JUMP;

	case mex::J2:
	    if ( SP < mexstack::stack_limit + 2 )
	        goto STACK_TOO_SHORT;
	    if ( mexas::get_star ( index )
	         ==
		 mexas::star )
	    {
		top_name =
		    (~ (   variables
			 + ( variables->length - 1 ) ) )
		    ->name;
		instr.immedB = 1;
		success_stack_offset = -1;
		mexstack::stack_length -= 1;
	    }
	    else
		mexstack::stack_length -= 2;
	    min::pop ( variables, 2 );
	    // Fall through.
	case mex::J:
	    goto JUMP;
	}

	STACK_TOO_SHORT:
	{
	    mexcom::compile_error
	        ( pp, "portion of stack in the current"
		      " block is too little to pop"
		      " required arguments; instruction"
		      " ignored" );
	    continue;
	}
	ARITHMETIC:
	{
	    min::gen name = mexas::get_name ( index );
	    if ( name == min::NONE() )
	    {
	        mexas::get_star ( index );
	        name = mexas::star;
	    }
	    else
		check_new_name ( name, pp );
	    mexas::push_variable
	        ( mexas::variables, name,
		  LEVEL_AND_DEPTH );
	    mexstack::push_instr ( instr, pp, name );
	    goto EXTRA_STUFF_CHECK;
	}
	JUMP:
	{
	    min::gen target = mexas::get_name ( index );
	    if ( target == min::NONE() )
	    {
		mexcom::compile_error
		    ( pp, "jmp... does not have a "
		          " jmp-target that is a name"
			  " instruction will fail" );
		    // instr fails because immedC = 0
		target = min::MISSING();
	    }
	    mexstack::push_jmp_instr
		( instr, target, pp,
		  false, success_stack_offset );

	    if ( top_name != min::NONE() )
	    {
	        -- mexstack::stack_length;
		mexas::push_variable
		    ( mexas::variables, top_name,
		      LEVEL_AND_DEPTH );
	    }
	    goto EXTRA_STUFF_CHECK;
	}
	NON_ARITHMETIC:
	{
	    switch ( op_code )
	    {
	    case ::PUSH:
	    case ::PUSHM:
	    case mex::PUSHG:
	    {
	        min::gen mod_name;
		if ( op_code == mex::PUSHG )
		{
		    mod_name =
			mexas::get_name ( index );
		    if ( mod_name == min::NONE() )
		        mod_name = mexas::get_star
			    ( index );
		    if ( mod_name == min::NONE() )
		    {
			mexcom::compile_error
			    ( pp, "no module name:"
				  " instruction"
				  " ignored" );
			continue;
		    }
		}
		min::gen name =
		    mexas::get_name ( index );
		if ( name == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no variable name:"
			      " instruction ignored" );
		    continue;
		}
		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		switch ( op_code )
		{
		case ::PUSH:
		{
		    mexas::push_variable
			( mexas::variables, new_name,
			  LEVEL_AND_DEPTH );
		    mexas::push_push_instr
		        ( new_name, name, pp );
		    break;
		}
		case ::PUSHM:
		{
		    min::uns32 limit =
			( L == 0 ?
			  mexstack::stack_limit :
			  mexstack::ap[1] );
		    min::uns32 j =
		        search ( name, limit );
		    if ( j == mexas::NOT_FOUND )
		    {
			mexcom::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " not globally"
				  " defined;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.op_code = mex::PUSHL;
		    instr.trace_class = mex::T_PUSH;
		    instr.immedA = j;
		    instr.immedB = 0;

		    min::gen labbuf[2] =
		        { new_name, name };
		    min::locatable_gen trace_info
			( min::new_lab_gen
			      ( labbuf, 2 ) );
		    mexas::push_variable
			( mexas::variables, new_name,
			  LEVEL_AND_DEPTH );
		    mexstack::push_instr
			( instr, pp, trace_info );
		    break;
		}
		case mex::PUSHG:
		{
		    mex::module gm;
		    min::uns32 index =
		        mexas::global_search
			    ( gm, mod_name,
			      mexas::V, name );
		    if ( index == mexas::NOT_FOUND )
		    {
			mexcom::compile_error
			    ( pp, "variable named ",
				  min::pgen ( name ),
				  " in module named ",
				  min::pgen
				      ( mod_name ),
				  " not defined;"
				  " instruction"
				  " ignored" );
			continue;
		    }

		    instr.immedA = index;
		    instr.immedD =
		        min::new_stub_gen ( gm );

		    min::gen labbuf[3] =
		        { new_name, gm->name, name };
		    min::locatable_gen trace_info
			( min::new_lab_gen
			      ( labbuf, 3 ) );
		    mexas::push_variable
			( mexas::variables, new_name,
			  LEVEL_AND_DEPTH );
		    mexstack::push_instr
			( instr, pp, trace_info );

		    break;
		}
		}
		break;
	    }
	    case mex::PUSHI:
	    {
	        min::locatable_gen D;
		D = mexas::get_num ( index );
		if ( D == min::NONE() )
		    D = mexas::get_label ( index );
		if ( D == min::NONE() )
		{
		    D = mexas::get_name ( index );
		    if ( D == ::TRUE )
		        D = mex::TRUE;
		    else if ( D == ::FALSE )
		        D = mex::FALSE;
		    else if ( D == ::NONE )
		        D = min::NONE();
		    else
		    {
			mexcom::compile_error
			    ( pp, "cannot understand"
				  " first argument to"
				  " PUSHI: instruction"
				  " ignored" );
			continue;
		    }
		}
		instr.immedD = D;

		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );
		mexstack::push_instr
		    ( instr, pp, new_name );
		break;
	    }
	    case ::POP:
	    {
	        if ( ! check_pop ( 1, pp ) )
		    continue;
		min::gen name =
		    mexas::get_name ( index );
		if ( name == min::NONE() )
		    name = mexas::get_star ( index );
		if ( name == min::NONE() )
		    name = mexas::star;
		if ( name != mexas::star )
		{
		    min::uns32 j =
		        local_search ( name, pp );
		    if ( j == mexas::NOT_FOUND )
		        continue;

		    mexas::variable_element * ve =
		        ~ ( mexas::variables + j );
		    if ( ! ve->is_write_only
		         &&
			    ve->depth()
		         == mexstack::depth[L] )
		    {
			mexcom::compile_error
			    ( pp, "non write-only"
			          " variable named ",
				  min::pgen ( name ),
				  " has the same depth"
				  " as the POP"
				  " instruction, and"
				  " as such cannot"
				  " legally be written;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.immedA = SP - j - 1;
		}
		else
		    instr.immedA = 0;
		instr.op_code = mex::POPS;
		instr.trace_class = mex::T_POP;

		min::gen old_name =
		    ( mexas::variables
		      +
		      ( mexstack::stack_length
		        - 1 ) )
		    ->name;
		min::gen labbuf[2] = { old_name, name };
		min::locatable_gen trace_info
		    ( min::new_lab_gen ( labbuf, 2 ) );

		min::pop ( mexas::variables );
	        mexstack::stack_length -= 1;
		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::DEL:
	    {
		min::gen A = mexas::get_num ( index );
		min::gen C = mexas::get_num ( index );
		if ( ! mexas::check_parameter
			    ( instr.immedA, A,
			      pp, "immedA" ) )
		    continue;
		if ( ! mexas::check_parameter
			    ( instr.immedC, C,
			      pp, "immedC" ) )
		    continue;
		if ( ! check_pop
			   (   instr.immedA
			     + instr.immedC, pp ) )
		    continue;

		mexstack::stack_length -=
		   (int) instr.immedC;
		mexstack::push_instr ( instr, pp );

		min::uns32 sp = variables->length;
		min::uns32 sp1 = sp - instr.immedA;
		min::uns32 sp2 = sp1 - instr.immedC;
		while ( sp1 < sp )
		    variables[sp2++] = variables[sp1++];
		min::pop ( variables, instr.immedC );

	        break;
	    }
	    case mex::PUSHOBJ:
	    {
		min::gen A = mexas::get_num ( index );
		if ( A == min::NONE() )
		    instr.immedA = 32;
		else if ( ! mexas::check_parameter
		                ( instr.immedA, A,
				  pp, "unused_size" ) )
		    continue;
		min::gen C = mexas::get_num ( index );
		if ( C == min::NONE() )
		    instr.immedC = 8;
		else if ( ! mexas::check_parameter
		                ( instr.immedC, C,
				  pp, "hash_size" ) )
		    continue;
		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );
		mexstack::push_instr
		    ( instr, pp, new_name );
		break;
	    }
	    case mex::COPY:
	    {
		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		min::pop ( mexas::variables );
		mexstack::stack_length -= 1;
		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );
		mexstack::push_instr
		    ( instr, pp, new_name );
		break;
	    }
	    case mex::COPYI:
	    {
	        min::locatable_gen obj
		    ( min::new_obj_gen ( 40, 5 ) );
		min::obj_vec_insptr vp = obj;
		min::locatable_gen element;
		while ( true )
		{
		    element = mexas::get_num ( index );
		    if ( element == min::NONE() )
			element = mexas::get_str
			    ( index );
		    if ( element == min::NONE() )
		        break;
		    min::attr_push(vp) = element;
		}
		min::attr_insptr ap = vp;
		min::locate ( ap, min::dot_initiator );
		min::set ( ap, mexas::left_bracket );
		min::locate ( ap, min::dot_terminator );
		min::set ( ap, mexas::right_bracket );
		min::locate ( ap, min::dot_separator );
		min::set ( ap, ::comma );
		min::set_public_flag_of ( vp );
		vp = min::NULL_STUB;
		instr.immedD = obj;

		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );
		mexstack::push_instr
		    ( instr, pp, new_name );
		break;
	    }
	    case mex::VPUSH:
	    {
	        if ( ! check_pop ( 1, pp ) )
		    continue;

	        min::gen obj_var_name =
		    mexas::get_name ( index );
		if ( obj_var_name == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no object-variable-name;"
			      " instruction ignored" );
		    continue;
		}

		min::uns32 j =
		    local_search ( obj_var_name, pp );
		if ( j == mexas::NOT_FOUND )
		    continue;
		instr.immedA = SP - j - 1;

		min::locatable_gen attr_label
		    ( mexas::get_label ( index ) );
		if ( attr_label == min::NONE() )
		{
		    min::uns32 original_index = index;
		    attr_label =
		        mexas::get_name ( index );
		    if ( attr_label == ::NONE )
		        attr_label = min::NONE();
		    else
		    {
			attr_label = min::MISSING();
		        index = original_index;
		    }
		}
		instr.immedD = attr_label;

		min::gen old_name =
		    ( mexas::variables
		      +
		      ( mexstack::stack_length
			- 1 ) )
		    ->name;
		min::gen labbuf[2] =
		    { old_name, obj_var_name };
		min::locatable_gen trace_info
		    ( min::new_lab_gen ( labbuf, 2 ) );

		min::pop ( mexas::variables );
		mexstack::stack_length -= 1;

		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::VPOP:
	    {

	        min::gen obj_var_name =
		    mexas::get_name ( index );
		if ( obj_var_name == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "VPOP: no object-"
			      "variable-name;"
			      " instruction ignored" );
		    continue;
		}

		min::uns32 j =
		    local_search ( obj_var_name, pp );
		if ( j == mexas::NOT_FOUND )
		    continue;
		instr.immedA = SP - j - 1;

		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
			mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		min::gen labbuf[2] =
		    { obj_var_name, new_name };
		min::locatable_gen trace_info
		    ( min::new_lab_gen ( labbuf, 2 ) );

		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );

		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::VSIZE:
	    {
	 	if (    mexstack::fp
		          [mexstack::lexical_level]
		     >= mexstack::stack_length )
		{
		    mexcom::compile_error
			( pp, "VSIZE: function frame"
			      " too small;"
			      " instruction ignored" );
		    continue;
		}

		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
			mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		min::gen old_name =
		    ( mexas::variables
		      +
		      ( mexstack::stack_length
			- 1 ) )
		    ->name;
		min::gen labbuf[2] =
		    { old_name, new_name };
		min::locatable_gen trace_info
		    ( min::new_lab_gen ( labbuf, 2 ) );

		min::pop ( mexas::variables );
		mexstack::stack_length -= 1;
		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );

		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::GETI:
	    case mex::GET:
	    case mex::SETI:
	    case mex::SET:
	    {
	        min::gen obj_var_name =
		    mexas::get_name ( index );
		if ( obj_var_name == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no object-variable-name;"
			      " instruction ignored" );
		    continue;
		}

		min::uns32 j =
		    local_search ( obj_var_name, pp );
		if ( j == mexas::NOT_FOUND )
		    continue;
		instr.immedA = SP - j - 1;

		min::locatable_gen attr_label;

		if (    op_code == mex::GETI
		     || op_code == mex::SETI )
		{
		    attr_label =
		        mexas::get_label ( index );
		    if ( attr_label == min::NONE() )
		    {
			mexcom::compile_error
			    ( pp, "no attribute-label;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.immedD = attr_label;
		}

		min::locatable_gen trace_info;

		if ( op_code == mex::GET
		     ||
		     op_code == mex::GETI )
		{
		    min::gen new_name =
			mexas::get_name ( index );
		    if ( new_name != min::NONE() )
			check_new_name ( new_name, pp );
		    else
			new_name =
			    mexas::get_star ( index );
		    if ( new_name == min::NONE() )
			new_name = mexas::star;

		    min::gen labbuf[2] =
			{ obj_var_name, new_name };
		    trace_info =
			min::new_lab_gen ( labbuf, 2 );

		    if ( op_code == mex::GET )
		    {
			min::pop ( mexas::variables );
			mexstack::stack_length -= 1;
		    }

		    mexas::push_variable
			( mexas::variables, new_name,
			  LEVEL_AND_DEPTH );
		}
		else //    op_code == mex::SET
		     // || op_code == mex::SETI
		{
		    min::gen old_name =
			( mexas::variables
			  +
			  ( mexstack::stack_length
			    - 1 ) )
			->name;
		    min::gen labbuf[2] =
		        { old_name, obj_var_name };
		    trace_info =
			min::new_lab_gen ( labbuf, 2 );

		    unsigned pops =
		        op_code == mex::SET ? 2 : 1;
		    if ( ! check_pop ( pops, pp ) )
		        continue;
		    min::pop ( mexas::variables, pops );
		    mexstack::stack_length -= pops;
		}
		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case ::LABEL:
	    {

		min::gen target =
		    mexas::get_name ( index );
		if ( target == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "label does not have a "
			      " jmp-target that is a"
			      " name; declaraton"
			      " ignored" );
		    continue;
		}

		mexstack::print_label
		    ( target, pp );
		mexstack::jmp_target ( target );
		continue;
	    }
	    case ::STACKS:
	    {
		if ( mexstack::print_switch
		     ==
		     mexstack::NO_PRINT )
		    continue;

		min::printer printer = mexcom::printer;

		if ( mexstack::print_switch
		     ==
		     mexstack::PRINT_WITH_SOURCE )
		{
		    min::phrase_position spp = pp;
		    -- spp.end.line;
		    if (   spp.end.line
		         > spp.begin.line )
			min::print_phrase_lines
			    ( printer,
			      mexcom::input_file,
			      spp );
		}

		printer << min::bom
		        << "    STACKS: "
		        << min::place_indent ( 0 );

		printer << min::save_indent
                        << "VARIABLES: "
		        << min::place_indent ( 0 );
		for ( min::uns32 i =
		          mexstack::stack_length;
		      0 < i; )
		{
		    -- i;
		    mexas::variable_element v =
		        variables[i];
		    printer
		        << min::indent
		        << v.level()
			<< "."
			<< v.depth()
			<< " "
			<< v.name;
		    if ( v.is_write_only )
		        printer << " [WO]";
		}
		printer << min::restore_indent;

		if ( functions->length == 0 )
		{
		    printer << min::eom;
		    continue;
		}

		printer << min::indent
                        << min::save_indent
		        << "FUNCTIONS: "
		        << min::place_indent ( 0 );
		for ( min::uns32 i = functions->length;
		      0 < i; )
		{
		    -- i;
		    mexas::function_element f =
		        functions[i];
		    printer
		        << min::indent
		        << f.level()
			<< "."
			<< f.depth()
			<< " "
			<< f.name;
		}
		printer << min::restore_indent;

		printer << min::eom;
		continue;
	    }
	    case ::TEST_INSTRUCTION:
	    {
	        min::gen op_code =
		    mexas::get_name ( index );
		if ( op_code == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no op_code;"
			      " statement ignored" );
		    continue;
		}

		op_code = min::get
		    ( mexcom::op_code_table, op_code );
		if ( op_code == min::NONE()
		     ||
		        min::direct_float_of ( op_code )
		     >= mex::NUMBER_OF_OP_CODES )
		{
		    mexcom::compile_error
			( pp, "undefined operation"
			      " code; statement"
			      " ignored" );
		    continue;
		}
		instr.op_code =
		    (min::uns8)
		    min::int_of ( op_code );

	        min::gen trace_class =
		    mexas::get_name ( index );
		if ( trace_class != min::NONE() )
		{
		    trace_class = min::get
			( mexcom::trace_class_table,
			  trace_class );
		    if ( trace_class != min::NONE() )
		    {
			min::float64 f =
			    min::direct_float_of
				( trace_class );
			instr.trace_class =
			    (min::uns8) f;
		    }
		    else
		    {
			mexcom::compile_error
			    ( pp, "unrecognized trace"
				  " class; statement"
				  " ignored" );
			continue;
		    }
		}

	        min::gen trace_depth =
		    mexas::get_trace_depth ( index );
		if ( trace_depth != min::NONE() )
		{
		    min::uns32 depth;
		    if ( !  mexas::check_parameter
				( depth, trace_depth,
				  pp, "trace_depth" ) )
			continue;
		    if ( depth >= 255 )
		    {
			mexcom::compile_error
			    ( pp, "trace_depth too"
			          " large; statement"
				  " ignored" );
			continue;
		    }
		    instr.trace_depth =
		        (min::uns8) depth;
		}

	        min::gen immedA =
		    mexas::get_num ( index );
		if ( immedA == min::NONE() )
		{
		    mexstack::push_instr ( instr, pp );
		    break;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedA, immedA,
			      pp, "immedA" ) )
		    continue;

	        min::gen immedB =
		    mexas::get_num ( index );
		if ( immedB == min::NONE() )
		{
		    mexstack::push_instr ( instr, pp );
		    break;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedB, immedB,
			      pp, "immedB" ) )
		    continue;

	        min::gen immedC =
		    mexas::get_num ( index );
		if ( immedC == min::NONE() )
		{
		    mexstack::push_instr ( instr, pp );
		    break;
		}
		if ( !  mexas::check_parameter
		            ( instr.immedC, immedC,
			      pp, "immedC" ) )
		    continue;

	        min::locatable_gen immedD 
		    ( mexas::get_num ( index ) );
		if ( immedD == min::NONE() )
		    immedD = mexas::get_label ( index );
		if ( immedD == min::NONE() )
		{
		    immedD = mexas::get_name ( index );
		    if ( immedD == min::NONE() )
		        immedD = min::MISSING();
		    else if ( immedD == ::TRUE )
		        immedD = mex::TRUE;
		    else if ( immedD == ::FALSE )
		        immedD = mex::FALSE;
		    else if ( immedD == ::NONE )
		        immedD = min::NONE();
		    else
		    {
			mex::module m = ::module_search
			    ( immedD );
			if ( m == min::NULL_STUB )
			{
			    mexcom::compile_error
				( pp, "",
				      min::pgen
					  ( immedD ),
				      " does not name a"
				      " module or"
				      " special value;"
				      " statement"
				      "ignored" );
			    continue;
			}
			immedD =
			    min::new_stub_gen ( m );
		    }
		}

		instr.immedD = immedD;
		mexstack::push_instr ( instr, pp );
		break;
	    }
	    case mex::BEG:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexstack::begx
		    ( instr, 0, tvars, trace_info, pp );
		break;
	    }
	    case mex::END:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		mexstack::endx
		    ( instr, tvars, trace_info, pp );
		break;
	    }
	    case mex::BEGL:
	    {
	        min::gen nn = mexas::get_num ( index );
		if ( nn == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no nnext parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 nnext;
		if ( !  mexas::check_parameter
		            ( nnext, nn,
			      pp, "nnext" ) )
		    continue;

		if (   nnext
		     > SP - mexstack::stack_limit )
		{
		    mexcom::compile_error
			( pp, "portion of stack in the"
			      " containing block is"
			      " smaller than the number"
			      " of next-variables" );
		    nnext =
		        SP - mexstack::stack_limit;
			    // To protect against
			    // excessively large nvars
			    // values.
		}

		min::gen message =
		    mexas::get_str ( index );
		if ( message == min::NONE() )
		    message = ::loop;

		for ( min::uns32 i = 1;
		      i <= nnext; ++ i )
		{
		    mexas::variable_element * ve =
		        ~ (   mexas::variables
		            + ( SP - i ) );
		    if (    ve->level_and_depth
		         != LEVEL_AND_DEPTH
			 ||
			 ve->name == mexas::star )
		    {
		        nnext = i - 1;
			mexcom::compile_error
			    ( pp, "BEGL: not enough"
			          " named variables"
				  " of current lexical"
				  " level and depth"
				  " in the stack;"
				  " nnext reduced to",
				  min::puns
				      ( nnext, "%u" ) );
			break;
		    }
		}

		min::gen labbuf[nnext+1];
		labbuf[0] = message;
		for ( min::uns32 i = 1;
		      i <= nnext; ++ i )
		    labbuf[nnext+1-i] =
			(   mexas::variables
			  + ( SP - i ) )->name;

		min::locatable_gen trace_info
		    ( min::new_lab_gen
		          ( labbuf, nnext + 1 ) );
		min::uns32 pre_stack_limit =
		    mexstack::stack_limit;
		mexstack::begx
		    ( instr, nnext, 0, trace_info, pp );

		for ( min::uns32 i = 0;
		      i < nnext; ++ i )
		{
		    MIN_ASSERT
			(    nnext
			  <= SP - pre_stack_limit,
			  "BEGL: mexstack::"
			  "stack_limit violation"
			);
		    min::locatable_gen name
			( (   mexas::variables
			    + ( SP - nnext ) )->name );
		    MIN_ASSERT
			( name != mexas::star,
			  "BEGL: variable name is *" );

		    min::str_ptr sp ( name );
		    min::unsptr len =
		        min::strlen ( sp );
		    char buffer[len+10];
		    std::strcpy ( buffer, "next-" );
		    min::strcpy ( buffer + 5, sp );
		    name = min::new_str_gen ( buffer );

		    mexas::push_variable
			( mexas::variables, name,
			  LEVEL_AND_DEPTH, true );
		}
		break;
	    }
	    case mex::ENDL:
	    {
		mexstack::endx
		    ( instr, 0, min::MISSING(), pp );
		break;
	    }
	    case mex::CONT:
	    {
	        min::gen nn =
		    mexas::get_num ( index );
		min::uns32 loop_depth;
		if ( nn == min::NONE() )
		    loop_depth = 1;
		else if ( ! mexas::check_parameter
		                ( loop_depth, nn,
			          pp, "loop_depth" ) )
		    continue;
		if ( ! mexstack::cont
		           ( instr, loop_depth, 0,
		             min::MISSING(), pp ) )
		    mexcom::compile_error
			( pp, "CONT is not inside ",
			      min::puns
			          ( loop_depth, "%d" ),
			      " nested BEGL ... ENDL"
			      " loops; instruction"
			      " ignored" );
		break;
	    }
	    case mex::BEGF:
	    {
		min::gen function_name =
		    mexas::get_name ( index );
		if ( function_name == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "BEGF has no function"
			      " name; instruction"
			      " ignored" );
		    continue;
		}
		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nargs = index - first;
		min::gen message =
		    mexas::get_str ( index );
		if ( message == min::NONE() )
		    message = function_name;

		min::gen labbuf[nargs + 1];
		labbuf[0] = message;
		for ( min::uns32 i = 0; i < nargs;
		                        ++ i )
		    labbuf[i+1] = statement[first+i];
		min::locatable_gen trace_info
		    ( min::new_lab_gen
		          ( labbuf, nargs+1 ) );

		mexas::push_function
		    ( mexas::functions,
		      function_name,
		      LEVEL_AND_DEPTH,
		      m->length );

		mexstack::begx
		    ( instr, nargs, 0,
		      trace_info, pp );
		for ( min::uns32 i = 0; i < nargs;
		                        ++ i )
		    mexas::push_variable
			( mexas::variables,
			  statement[first+i],
			  LEVEL_AND_DEPTH );
		break;
	    }
	    case mex::ENDF:
	    {
		mexstack::endx
		    ( instr, 0, min::MISSING(), pp );
		continue;
	    }
	    case mex::RET:
	    {
	        if ( L == 0 )
		{
		    mexcom::compile_error
			( pp, "RET not in a function;"
			      " instruction ignored" );
		    continue;
		}
		min::gen nn = mexas::get_num ( index );
		if ( nn == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no nresults parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 nresults;
		if ( !  mexas::check_parameter
		            ( nresults, nn,
			      pp, "nresults" ) )
		    continue;
		if ( ! check_pop ( nresults, pp ) )
		    continue;
		instr.immedB = L;
		instr.immedC = nresults;
		min::pop ( mexas::variables, nresults );
	        mexstack::stack_length -= nresults;
		mexstack::push_instr ( instr, pp );
		break;
	    }
	    case ::CALL:
	    case mex::CALLM:
	    case mex::CALLG:
	    {
		mex::module cm = m;
		min::uns32 begf_location =
		    mexas::NOT_FOUND;
		min::gen function_name;

		if ( op_code == mex::CALLG )
		{
		    min::gen mod_name =
		        mexas::get_name ( index );
		    if ( mod_name == min::NONE() )
		        mod_name =
			    mexas::get_star ( index );
		    if ( mod_name == min::NONE() )
		    {
			mexcom::compile_error
			    ( pp, "no module name for"
			          " CALLG;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    function_name =
		        mexas::get_name ( index );
		    if ( function_name == min::NONE() )
		    {
			mexcom::compile_error
			    ( pp, "no function name for"
			          " CALLG;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    begf_location = mexas::global_search
		        ( cm, mod_name, mexas::F,
			      function_name );
		    if (    begf_location
		         == mexas::NOT_FOUND )
		    {
			mexcom::compile_error
			    ( pp, "function ",
			          min::pgen
				      ( function_name ),
				  " in module ",
			          min::pgen
				      ( mod_name ),
			          " not found;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    instr.immedD =
		        min::new_stub_gen ( cm );
		}
		else
		{
		    function_name =
		        mexas::get_name ( index );
		    if ( function_name == min::NONE() )
		    {
			mexcom::compile_error
			    ( pp, "no function name for"
			          " CALL/CALLM;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		    min::uns32 i =
		        mexas::functions->length;
		    while ( 0 < i )
		    {
		        -- i;
			min::ptr
			    <mexas::function_element>
			    fptr =
			    (mexas::functions + i );
			if (    fptr->name
			     == function_name )
			{
			    begf_location =
			        fptr->index;
			    break;
			}
		    }
		    if (    begf_location
			 != mexas::NOT_FOUND )
		    {
		        instr.op_code = mex::CALLM;
			instr.trace_class =
			    mex::T_CALLM;
		    }
		    else if ( op_code == ::CALL )
		    {
		        instr.op_code = mex::CALLG;
			instr.trace_class =
			    mex::T_CALLG;
			begf_location =
			    mexas::global_search
				( cm, mexas::star,
				      mexas::F,
				      function_name );
			instr.immedD =
			    min::new_stub_gen ( cm );
		    }
		    if (    begf_location
		         == mexas::NOT_FOUND )
		    {
			mexcom::compile_error
			    ( pp, "function ",
			          min::pgen
				      ( function_name ),
			          " not found;"
				  " instruction"
				  " ignored" );
			continue;
		    }
		}

		if ( begf_location >= cm->length )
		{
		    mexcom::compile_error
			( pp, "bad function_element;"
			      " begf_location >="
			      " cm->length;"
			      " instruction ignored" );
		    continue;
		}
		min::ptr<mex::instr> ip =
		    cm + begf_location;
		if ( ip->op_code != mex::BEGF )
		{
		    mexcom::compile_error
			( pp, "bad function_element;"
			      " does not point at"
			      " BEGF;"
			      " instruction ignored" );
		    continue;
		}

	        min::gen na = mexas::get_num ( index );
		if ( na == min::NONE() )
		{
		    mexcom::compile_error
			( pp, "no nargs parameter;"
			      " instruction ignored" );
		    continue;
		}
		min::uns32 nargs;
		if ( !  mexas::check_parameter
		            ( nargs, na,
			      pp, "nargs" ) )
		    continue;
		if ( ! check_pop ( nargs, pp ) )
		    continue;

		if ( ip->immedA > nargs )
		{
		    mexcom::compile_error
			( pp, "BEGF expects ",
			      min::puns
			         ( ip->immedA,
				   "%u" ),
			      " arguments but CALL..."
			      " provides only ",
			      min::puns
			         ( nargs, "%u" ),
			      ";"
			      " instruction ignored" );
		    continue;
		}

		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE()
			||
			mexas::get_star ( index )
			!=
			min::NONE() );
		min::uns32 nresults = index - first;
		min::gen message =
		    mexas::get_str ( index );
		if ( message == min::NONE() )
		    message = function_name;

		min::gen labbuf[nresults + 1];
		labbuf[0] = message;
		for ( min::uns32 i = 0; i < nresults;
		                        ++ i )
		    labbuf[i+1] = statement[first+i];
		min::locatable_gen trace_info
		    ( min::new_lab_gen
		          ( labbuf, nresults+1 ) );
		
		instr.immedA = nargs;
		instr.immedB = nresults;
		instr.immedC = begf_location;

		min::pop ( mexas::variables, nargs );
	        mexstack::stack_length -= nargs;
		for ( min::uns32 i = 0; i < nresults;
		                        ++ i )
		    mexas::push_variable
			( mexas::variables,
			  statement[first+i],
			  LEVEL_AND_DEPTH );
		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::SET_TRACE:
	    {
		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nflags = index - first;

		min::uns32 flags = 0;
		for ( min::uns32 i = 0; i < nflags;
		                        ++ i )
		{
		    min::gen fname = statement[first+i];
		    min::gen fbit = min::get
		        ( mexcom::trace_flag_table,
			  fname );
		    if ( fbit != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( fbit );
			flags |= (min::uns32) f;
		    }
		    else
			mexcom::compile_error
			    ( pp, "Unrecognized trace"
			          " flag: ",
				  min::pgen ( fname ),
				  "; ignored" );
		}
		instr.immedA = flags;

		mexstack::push_instr ( instr, pp );
		break;
	    }
	    case mex::SET_EXCEPTS:
	    {
		min::uns32 first = index;
		while ( mexas::get_name ( index )
		        !=
			min::NONE() );
		min::uns32 nflags = index - first;

		min::uns32 flags = 0;
		for ( min::uns32 i = 0; i < nflags;
		                        ++ i )
		{
		    min::gen fname = statement[first+i];
		    min::gen fbit = min::get
		        ( mexcom::except_mask_table,
			  fname );
		    if ( fbit != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( fbit );
			flags |= (min::uns32) f;
		    }
		    else
			mexcom::compile_error
			    ( pp, "Unrecognized except"
			          " flag: ",
				  min::pgen ( fname ),
				  "; ignored" );
		}
		instr.immedA = flags;

		mexstack::push_instr ( instr, pp );
		break;
	    }
	    case mex::SET_OPTIMIZE:
	    {
	        min::gen param =
		    mexas::get_name ( index );
		if ( param == ::on )
		    instr.immedA = 1;
		else if ( param == ::off )
		    instr.immedA = 0;
		else
		    mexcom::compile_error
		        ( pp, "SET_OPTIMIZE requires"
			      " `ON' or `OFF'"
			      " parameter; `OFF'"
			      " assumed" );

		mexstack::push_instr ( instr, pp );
		break;
	    }
	    case mex::NOP:
	    case mex::TRACE:
	    case mex::WARN:
	    case mex::ERROR:
	    {
		min::locatable_gen trace_info;
	        min::uns32 tvars =
		    mexas::get_trace_info
			( trace_info, index, pp );
		instr.immedA = tvars;
		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::DUMP:
	    {
		min::locatable_gen trace_info
		    ( mexas::get_trace_info ( index ) );
		mexstack::push_instr
		    ( instr, pp, trace_info );
		break;
	    }
	    case mex::PUSHNARGS:
	    case mex::PUSHV:
	    {
		min::uns32 level;
	        min::gen nl = mexas::get_num ( index );
		if ( nl == min::NONE() )
		    level = L;
		else if ( !  mexas::check_parameter
		                 ( level, nl,
			           pp, "level", true ) )
		    continue;
		instr.immedB = level;

		min::gen new_name =
		    mexas::get_name ( index );
		if ( new_name != min::NONE() )
		    check_new_name ( new_name, pp );
		else
		    new_name =
		        mexas::get_star ( index );
		if ( new_name == min::NONE() )
		    new_name = mexas::star;

		mexas::push_variable
		    ( mexas::variables, new_name,
		      LEVEL_AND_DEPTH );
		mexstack::push_instr
		    ( instr, pp, new_name );
		break;
	    }

	    default:
		mexstack::push_instr ( instr, pp );
	    }
	}
	EXTRA_STUFF_CHECK:
	{
	    if ( index < mexas::statement->length )
	    {
		min::phrase_position pp =
		    m->position[m->length - 1];
		min::gen item = mexas::statement[index];
		char quote[3] = "\0\0";
		if ( ( item == mexas::single_quote
		       ||
		       item == mexas::double_quote )
		     &&
		       index + 1
		     < mexas::statement->length )
		{
		    min::strncpy ( quote, item, 1 );
		    item = mexas::statement[++index];
		}
		mexcom::compile_error
		    ( pp,
		      "extra stuff at end of"
		      " instruction: ",
		      min::pnop,
		      quote,
		      min::pgen_never_quote ( item ),
		      quote,
		      min::pnop,
		      " ..." );
	    }
	    continue;
	}

    }

    mexstack::jmp_clear();

    if ( mexcom::error_count > 0 )
        return min::NULL_STUB;

    if (    mexcom::input_file->file_name
         == min::MISSING() )
        mex::name_ref(m) = min::MISSING();
    else
    {
        min::str_ptr sp
	    ( mexcom::input_file->file_name );
	const char * p =
	    min::unprotected::str_of ( sp );
	min::uns32 len = std::strlen ( p );
	if ( len > 4
	     &&
	     std::strcmp ( ".mex", p + len - 4 ) == 0 )
	    mex::name_ref(m) =
	        min::new_str_gen ( p, len - 4 );
	else
	    mex::name_ref(m) =
	        mexcom::input_file->file_name;

    }

    mexas::make_module_interface();

    min::locatable_var<mex::process> process
        ( mex::init_process ( m ) );
    mex::run_process ( process );
    if ( process->state != mex::MODULE_END )
    {
        process->printer
	    << min::bom << "ERROR: "
	    << min::place_indent ( 0 )
	    << "module initialization process did not"
	       " terminate normally at module end: "
	    << mex::state_infos[process->state]
	    	    .description
	    << min::eom;
        return min::NULL_STUB;
    }
    mex::excepts_check ( process );

    min::packed_vec_insptr<min::gen> g =
        (min::packed_vec_insptr<min::gen>)
	min::gen_packed_vec_type.new_stub
	    ( process->length );
    mex::globals_ref ( m ) = g;
    for ( min::uns32 i = 0; i < process->length; ++ i )
        min::push(g) = process[i];

    min::push ( mexcom::modules ) = m;

    return m;
}
