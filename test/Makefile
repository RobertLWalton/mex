#  MIN System Execution Engine Interface Makefile
#
# File:		Makefile
# Author:	Bob Walton (walton@acm.org)
# Date:		Wed Aug 23 04:08:28 EDT 2023
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
	mexas_scanner_test_2 \
        mexas_compile_test_1 \
        mexas_compile_test_2 \
        mexas_compile_test_3 \
        mexas_compile_test_4 \
        mexas_compile_test_5 \
        mexas_compile_test_6


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
	    -e '/line [0-9][0-9]*:/s//line ####:/g' \
	    -e '/lines [0-9][0-9]*-[0-9][0-9]*:/s//lines ####-####/g' \
            < $*.out > $*.test

%.diff:	%.out
	@echo DIFFING $*.test $*.out
	-@sed \
	    -e '/line [0-9][0-9]*:/s//line ####:/g' \
	    -e '/lines [0-9][0-9]*-[0-9][0-9]*:/s//lines ####-####/g' \
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

mexas:	\
	${MEXAS_H} \
	${MEXAS_O} \
	../src/mexas_main.cc \
	Makefile
	g++ -o mexas -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    ../src/mexas_main.cc ${MEXAS_O}

mexas_scanner_test:	\
	${MEXAS_H} \
	${MEXAS_O} \
	mexas_scanner_test.cc \
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

TRACE = -tcl -t:ALL
mexas_compile_test_5.out: \
    TRACE = -t:BEG,BEGL,END,ENDL

mexas_compile_test_%.out:	\
		mexas \
		mexas_compile_test_%.mex \
		Makefile
	rm -f mexas_compile_test_$*.out
	-${RUNENV} mexas ${TRACE} \
	    mexas_compile_test_$*.mex \
	    > mexas_compile_test_$*.out


clean:
	rm -f ${TESTS:=.out} *.o mexas mexas_*_test
