// MEX Test of Math Library Signalling
//
// File:	mex_test_math_signals.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul  6 05:31:03 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

// WARNING: DO NOT OPTIMIZE.
// If the compiler computes SNAN + 5.0 ===> nan at
// compile time, NO invalid exception will be raised
// at runtime.

# include <iostream>
# include <cfenv>
# include <cmath>
using std::cout;
using std::endl;
using std::hex;
using std::dec;

double arg1, arg2, result;
double sNAN = NAN;

void test ( const char * exp )
{
    cout << exp << " ===> " << result << endl;
    cout << "    ";

    int excepts = fetestexcept ( FE_ALL_EXCEPT );
    if ( excepts & FE_INVALID )
        cout << " invalid operand(s)";
    else if ( excepts & FE_DIVBYZERO )
        cout << " divide by zero";
    else if ( excepts & FE_OVERFLOW )
        cout << " overflow";
    else if ( excepts & FE_INEXACT )
        cout << " inexact";
    else if ( excepts & FE_UNDERFLOW )
        cout << " underflow";
    else if ( excepts == 0 )
        cout << " none";
    else
        cout << " some";
    cout << endl;

}

#define TEST1(op,a) \
    arg1 = (a); \
    feclearexcept ( FE_ALL_EXCEPT ); \
    result = op ( arg1 ); \
    test ( #op " ( " #a  " )" );

#define TEST2(a1,op,a2) \
    arg1 = (a1), arg2 = (a2); \
    feclearexcept ( FE_ALL_EXCEPT ); \
    result = arg1 op arg2; \
    test ( #a1 " " #op " " #a2 );

void print_as_hex ( const char * name, double value )
{
    long unsigned & v = * (long unsigned *) & value;
    cout << name << " = " << hex << v << dec << endl;
}

int main ( int argc, const char * argv[] )
{
    long unsigned & snan = * (long unsigned *) & sNAN;
    snan ^= 3ull << 50;

    print_as_hex ( "NAN", NAN );
    print_as_hex ( "SNAN", SNAN );
    print_as_hex ( "sNAN", sNAN );

    feclearexcept ( FE_ALL_EXCEPT );

    TEST2 ( 6.7, +, 5.0 );
    TEST2 ( SNAN, +, 5.0 );
    TEST2 ( (- INFINITY), +, (+ INFINITY) );
    TEST2 ( 1e308, +, 1e308 );
    TEST1 ( -, 5.0 );
    TEST1 ( -, SNAN );
    TEST1 ( fabs, 5.0 );
    TEST1 ( fabs, SNAN );
    TEST1 ( round, 5.5 );
    TEST1 ( round, SNAN );

    return 0;
}
