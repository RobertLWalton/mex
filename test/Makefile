#  MIN System Execution Engine Interface Makefile
#
# File:		Makefile
# Author:	Bob Walton (walton@acm.org)
# Date:		Sun Jul 16 08:07:46 EDT 2023
#
# The authors have placed this program in the public
# domain; they make no warranty and accept no liability
# for this program.

# The following locates the MIN source code.
#
MIN = ~/min

# The following turns on MIN debugging.
#
GFLAGS = -DMIN_PROTECT=1
WFLAGS = -Wall -Wno-char-subscripts

.SUFFIXES:
.SUFFIXES: .out .test .diff

# O3 is needed to inline functions.
#
OPTIMIZE = -O3

DIFFOPT ?= -u

TESTS = mexas_scanner_test

MIN_H =	${MIN}/lib/libmin.a

MIN_O =	${MIN}/lib/libmin.a

MEX_H =	${MIN_H} \
		../include/mex.h \
		../include/mexas.h

MEX_O =	${MIN_O} \
		mex.o \
		mexas.o

.SECONDARY:	${TESTS:=.out}

all:		diff

diff:		${TESTS:=.diff}

out:		${TESTS:=.out}

test:		${TESTS:=.test}


%.test:	%.out Makefile
	rm -f $*.test
	sed \
	    -e '/\.cc:[0-9]* assert/s//.cc:XXXX assert/' \
	    -e '/\.h:[0-9]* assert/s//.h:XXXX assert/' \
	    -e '/\.cc:[0-9]* desire/s//.cc:XXXX desire/' \
	    -e '/\.L/s/\(\.L[A-Z][A-Z]*\)[0-9][0-9]*/\1XXX/g' \
	    -e '/line [0-9]/s//line #/g' \
	    -e '/\(lines [0-9][0-9]*-\)[0-9]/s//\1#/g' \
	    -e '/lines [0-9]\([0-9]*-#\)/s//lines #\1/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
            < $*.out > $*.test

%.diff:	%.out
	@echo DIFFING $*.test $*.out
	-@sed \
	    -e '/\.cc:[0-9]* assert/s//.cc:XXXX assert/' \
	    -e '/\.h:[0-9]* assert/s//.h:XXXX assert/' \
	    -e '/\.cc:[0-9]* desire/s//.cc:XXXX desire/' \
	    -e '/\.L/s/\(\.L[A-Z][A-Z]*\)[0-9][0-9]*/\1XXX/g' \
	    -e '/line [0-9]/s//line #/g' \
	    -e '/\(lines [0-9][0-9]*-\)[0-9]/s//\1#/g' \
	    -e '/lines [0-9]\([0-9]*-#\)/s//lines #\1/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/#[0-9]/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/[0-9]#/s//##/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
	    -e '/@[0-9]/s//@@/g' \
            < $*.out | diff ${DIFFOPT} $*.test -

%.out:	%
	rm -f $*.out
	${RUNENV} $* > $*.out

mex.o:	\
	${MIN_H} \
	../include/mex.h \
	../src/mex.cc \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -c ../src/mex.cc

mexas:	\
	${MIN_H} \
	../include/mex.h \
	../include/mexas.h \
	../src/mexas.cc \
	mex.o \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -o mexas ../src/mexas.cc mex.o

mex_scanner_test.out:
		mexas \
		mex_scanner_test.in \
		Makefile
	rm -f $*.out
	${RUNENV} $* -p < mex_scanner_test.in \
	             > $*.out


clean:
	rm -f mexas ${TESTS:=.out} *.o
