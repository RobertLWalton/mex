// MIN System Execution Engine Assembler Main Program
//
// File:	mexas_main.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Sat May 25 22:00:41 EDT 2024
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

# include <mexas.h>
# include <sys/times.h>
# include <unistd.h>

int main ( int argc, char * argv[] )
{
    min::initialize();

    min::init_ostream
        ( mex::default_printer, std::cout );
    mex::default_printer << min::ascii;

    min::printer printer = mex::default_printer;

    min::locatable_gen tmp1, tmp2;

    int i = 1;
    while ( i < argc )
    {
        const char * arg = argv[i++];
	const char * num;
	char * q;
	if ( strcmp ( "-pa", arg ) == 0 )
	    mexcom::print_switch = mexcom::PRINT;
	else if ( strcmp ( "-pasource", arg ) == 0 )
	    mexcom::print_switch =
	        mexcom::PRINT_WITH_SOURCE;
	else if ( strcmp ( "-paoff", arg ) == 0 )
	    mexcom::print_switch = mexcom::NO_PRINT;

	else if ( strcmp ( "-tcnormal", arg ) == 0 )
	    mexcom::trace_never = false;
	else if ( strcmp ( "-tcnever", arg ) == 0 )
	    mexcom::trace_never = true;

	else if ( strcmp ( "-o:on", arg ) == 0 )
	    mex::run_optimize = true;
	else if ( strcmp ( "-o:off", arg ) == 0 )
	    mex::run_optimize = false;
	else if ( strcmp ( "-counter", arg ) == 0 )
	{
	    num = argv[i++];
	    long L = std::strtol ( num, & q, 10 );
	    if (    * q != 0
	         || L < 0 || L >= (1l << 32 ) )
	        goto BAD_NUMBER;
	    mex::run_counter_limit = (min::uns32) L;

	}
	else if ( strcmp ( "-stack", arg ) == 0 )
	{
	    num = argv[i++];
	    long L = std::strtol ( num, & q, 10 );
	    if (    * q != 0
	         || L < 0 || L >= (1l << 32 ) )
	        goto BAD_NUMBER;
	    mex::run_stack_limit = (min::uns32) L;

	}
	else if ( strcmp ( "-return-stack", arg ) == 0 )
	{
	    num = argv[i++];
	    long L = std::strtol ( num, & q, 10 );
	    if (    * q != 0
	         || L < 0 || L >= (1l << 32 ) )
	        goto BAD_NUMBER;
	    mex::run_return_stack_limit =
	        (min::uns32) L;

	}

	else if ( strncmp ( "-tc:", arg, 4 ) == 0 )
	{
	    min::uns32 flags = (1 << mex::T_ALWAYS );
	    const char * p = arg + 4;
	    while ( * p )
	    {
	        const char * q = p;
		while ( * q && * q != ',' ) ++ q;
		if ( q > p )
		{
		    tmp1 = min::new_str_gen
		        ( p, q - p );
		    tmp2 = min::get
		        ( mexcom::trace_flag_table,
			  tmp1 );
		    if ( tmp2 != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( tmp2 );
			flags |= (min::uns32) f;
		    }
		    else
		        printer
			    << min::bol
			    << "trace class/group "
			    << min::pgen ( tmp1 )
			    << " unrecognized;"
			       " ignored"
			    << min::eol;
		}
		if ( * q ) ++ q;
		p = q;
	    }
	    mex::run_trace_flags = flags;
	}

	else if ( strncmp ( "-ex:", arg, 4 ) == 0 )
	{
	    int excepts = 0;
	    const char * p = arg + 4;
	    while ( * p )
	    {
	        const char * q = p;
		while ( * q && * q != ',' ) ++ q;
		if ( q > p )
		{
		    tmp1 = min::new_str_gen
		        ( p, q - p );
		    tmp2 = min::get
		        ( mexcom::except_mask_table,
			  tmp1 );
		    if ( tmp2 != min::NONE() )
		    {
		        min::float64 f =
			    min::direct_float_of
			        ( tmp2 );
			excepts |= (int) f;
		    }
		    else
		        printer
			    << min::bol
			    << "except name "
			    << min::pgen ( tmp1 )
			    << " unrecognized;"
			       " ignored"
			    << min::eol;
		}
		if ( * q ) ++ q;
		p = q;
	    }
	    mex::run_excepts_mask = excepts;
	}

	else if ( strcmp ( "-r", arg ) == 0 )
	{
	    min::locatable_gen name
	        ( min::new_str_gen ( argv[i++] ) );
	    mex::module m;
	    min::uns32 location =
	        mexas::global_search
		    ( m, mexas::star, mexas::F, name );
	    if ( location == mexas::NOT_FOUND )
	        printer << min::bol
		        << "function "
			<< name
			<< " not found;"
			<< " run command ignored"
			<< min::eol;
	    else
	    {
	        mex::pc pc = { m, location };
		mex::process process =
		    mex::init_process ( pc );
		struct tms start, stop;
		times ( & start );
		mex::run_process ( process );
		times ( & stop );
		if ( process->state != mex::CALL_END )
		{
		    printer
			<< min::bom << "ERROR: "
			<< min::place_indent ( 0 )
			<< "process did not terminate"
			   " normally with return to"
			   " call of "
			<< name
			<< ": "
			<< mex::state_infos
			       [process->state]
			       .description;
		    if (    process->state
		         == mex::EXCEPTS_ERROR )
		    {
			printer << "; exceptions are: ";
			mex::print_excepts
			    ( process->printer,
			      process->
			          excepts_accumulator,
			      process->excepts_mask );
		    }
		    printer << min::eom;
		}
		else
		{
		    long ticks_per_second =
		        sysconf ( _SC_CLK_TCK );
		    double cpu_time =
		        (double) (   stop.tms_utime
			           - start.tms_utime )
			/ ticks_per_second;
		    printer << min::bom
		            << "Call to " << name << " "
			    << min::place_indent ( 0 )
			    << "succeeded and"
			       " executed "
			    << (double) process->counter
			       * 1e-6
			    << " million instructions"
			    << min::indent
			    << "("
			    << (double)
			         process->
				     optimized_counter
			       * 1e-6
			    << " million optimized)"
			    << " in "
			    << min::pfloat
			        ( cpu_time, "%.3f" )
			    << " cpu seconds."
			    << min::eom;
		}
	    }
	}
	else if ( strcmp ( "-", arg ) == 0 )
	{
	    min::locatable_var<min::file> file;
	    min::init_printer
	        ( file, mex::default_printer );
	    min::init_input_stream ( file, std::cin );
	    bool result = mexas::compile ( file );
	    if ( ! result )
	    {
	        printer << min::bol
		        << "exiting due to compile"
		           " errors"
			<< min::eol;
		return 1;
	    }
	    else
	        printer << min::bol
		        << "standard input successfully"
		           " compiled"
			<< min::eol;
	}
	else if ( arg[0] == '-' )
	{
	    printer << min::bol
	            << "unrecognized option: " << arg
		    << min::eol;
	    return 1;
	}
	else
	{
	    min::locatable_gen file_name
	        ( min::new_str_gen ( arg ) );
	    min::locatable_var<min::file> file;
	    min::init_printer
	        ( file, mex::default_printer );
	    if ( ! min::init_input_named_file
	               ( file, file_name ) )
	    {
	        mex::default_printer
		    << min::error_message;
		return 1;
	    }
	    bool result = mexas::compile ( file );
	    if ( ! result )
	    {
	        printer << min::bol
		        << "exiting due to compile"
		           " errors"
			<< min::eol;
		return 1;
	    }
	    else
	        printer << min::bol
			<< arg
		        << " successfully compiled"
			<< min::eol;
	}

	continue;

    BAD_NUMBER:
	printer << min::bol << "bad " << arg
		<< " parameter " << num
		<< "; option ignored" << min::eol;
	continue;
    }
    return 0;
}
