// MEX Test of Math Library Signalling
//
// File:	mex_test_math_signals.cc
// Author:	Bob Walton (walton@acm.org)
// Date:	Thu Jul  6 03:13:02 EDT 2023
//
// The authors have placed this program in the public
// domain; they make no warranty and accept no liability
// for this program.

# include <iostream>
# include <cfenv>
# include <cmath>
using std::cout;
using std::endl;
using std::hex;
using std::dec;

double result;
double sNAN = NAN;

void test ( const char * expression )
{
    cout << expression << " ===> " << result << endl;
    cout << "    ";

    int excepts = fetestexcept ( FE_ALL_EXCEPT );
    if ( excepts & FE_INVALID )
        cout << " invalid operand(s)";
    else if ( excepts & FE_DIVBYZERO )
        cout << " divide by zero";
    else if ( excepts & FE_OVERFLOW )
        cout << " overflow";
    else if ( excepts & FE_INEXACT )
        cout << " invalid";
    else if ( excepts & FE_UNDERFLOW )
        cout << " invalid";
    else if ( excepts == 0 )
        cout << " none";
    else
        cout << " some";
    cout << endl;

    feclearexcept ( FE_ALL_EXCEPT );
}

#define TEST(expression) \
    result = (expression); \
    test (#expression);

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

    TEST ( 6.7 + 5.0 );
    TEST ( NAN + 5.0 );
    TEST ( sNAN + 5.0 );
    TEST ( SNAN + 5.0 );
    TEST ( ( + INFINITY) + ( - INFINITY ) );
    TEST ( ( + INFINITY) + ( + INFINITY ) );
    TEST ( SNAN );
    TEST ( sNAN );
    TEST ( 1.0 / 0.0 );
    TEST ( SNAN - SNAN );
    TEST ( fabs ( SNAN ) );
    TEST ( SNAN + SNAN );
    TEST ( 1e308 + 1e308 );

    return 0;
}
