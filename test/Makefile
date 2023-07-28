#  MIN System Execution Engine Interface Makefile
#
# File:		Makefile
# Author:	Bob Walton (walton@acm.org)
# Date:		Fri Jul 28 15:36:08 EDT 2023
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

TESTS = mexas_scanner_test_1 \
        mexas_compile_test_1

MIN_H =	${MIN}/include/min.h

MIN_O =	${MIN}/lib/libmin.a

MEXAS_H = ${MIN_H} \
		../include/mex.h \
		../include/mexas.h

MEXAS_O = \
		mex.o \
		mexas.o \
		${MIN_O}

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

mexas.o:	\
	${MIN_H} \
	../include/mex.h \
	../include/mexas.h \
	../src/mexas.cc \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -c ../src/mexas.cc

mexas_%_test:	\
	${MEXAS_H} \
	${MEXAS_O} \
	mexas_%_test.cc \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -o $@ $@.cc ${MEXAS_O}

mexas_scanner_test_%.out:	\
		mexas_scanner_test \
		mexas_scanner_test_%.in \
		Makefile
	rm -f mexas_scanner_test_$*.out
	${RUNENV} mexas_scanner_test \
	    < mexas_scanner_test_$*.in \
	    > mexas_scanner_test_$*.out

mexas_compile_test_%.out:	\
		mexas_compile_test \
		mexas_compile_test_%.in \
		Makefile
	rm -f mexas_compile_test_$*.out
	${RUNENV} mexas_compile_test \
	    < mexas_compile_test_$*.in \
	    > mexas_compile_test_$*.out


clean:
	rm -f ${TESTS:=.out} *.o
