#  MIN System Execution Engine Interface Makefile
#
# File:		Makefile
# Author:	Bob Walton (walton@acm.org)
# Date:		Fri May 24 21:57:36 EDT 2024
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
WFLAGS = -Wall -Wextra -Wno-unused-parameter \
         -Wno-char-subscripts

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
        mexas_compile_test_6 \
        mexas_compile_test_7 \
        mexas_compile_test_8 \
        mexas_compile_test_9 \
        mexas_compile_test_10 \
        mexas_compile_test_11 \
        mexas_compile_test_12 \
        mexas_compile_test_13 \
        mexas_compile_test_14 \
        mexas_joint_test_1 \
        mexas_joint_test_2 \
        mexas_time_test_1 \
        mexas_time_test_1o \
        mexas_time_test_2 \
        mexas_time_test_2o


MIN_H =	${MIN}/include/min.h

MIN_O =	${MIN}/lib/libmin.a

MEXAS_H = ${MIN_H} \
		../include/mex.h \
		../include/mexcom.h \
		../include/mexstack.h \
		../include/mexas.h

MEXAS_O = \
		mex.o \
		mexcom.o \
		mexstack.o \
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
	    -e '/ [0-9][0-9.]* cpu/s// #.### cpu/g' \
            < $*.out > $*.test

%.diff:	%.out
	@echo DIFFING $*.test $*.out
	-@sed \
	    -e '/line [0-9][0-9]*:/s//line ####:/g' \
	    -e '/lines [0-9][0-9]*-[0-9][0-9]*:/s//lines ####-####/g' \
	    -e '/ [0-9][0-9.]* cpu/s// #.### cpu/g' \
            < $*.out | diff ${DIFFOPT} $*.test -

mex.o:	\
	${MIN_H} \
	../include/mex.h \
	../src/mex.cc \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -c ../src/mex.cc

mexcom.o:	\
	${MIN_H} \
	../include/mex.h \
	../include/mexcom.h \
	../src/mexcom.cc \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -c ../src/mexcom.cc

mexstack.o:	\
	${MIN_H} \
	../include/mex.h \
	../include/mexcom.h \
	../include/mexstack.h \
	../src/mexstack.cc \
	Makefile
	g++ -g ${GFLAGS} ${WFLAGS} \
	    -I ../include -I ${MIN}/include \
	    -c ../src/mexstack.cc

mexas.o:	\
	${MIN_H} \
	../include/mex.h \
	../include/mexcom.h \
	../include/mexstack.h \
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
	${RUNENV} ./mexas_scanner_test \
	    < mexas_scanner_test_$*.in \
	    > mexas_scanner_test_$*.out

TRACE = -pasource -tc:ALL
mexas_compile_test_5.out: \
    TRACE = -tc:BEG,BEGL,END,ENDL
mexas_compile_test_10.out: \
    TRACE = -ex:INVALID -pasource -tc:ALL
mexas_compile_test_11.out: \
    TRACE = -tc:FUNC
mexas_time_test_1.out: \
    RUN = -stack 20000000 -return-stack 1000002 \
          -counter 20000000
mexas_time_test_1o.out: \
    RUN = -stack 20000000 -return-stack 1000002 \
          -counter 20000000 -o:on
mexas_time_test_2.out: \
    RUN = -counter 20000000
mexas_time_test_2o.out: \
    RUN = -counter 20000000 -o:on

# mexas_compile_test_14.mex does not exist.
#
mexas_compile_test_14.out:
	rm -f mexas_compile_test_14.out
	-${RUNENV} ./mexas ${TRACE} \
	    mexas_compile_test_14.mex \
	    > mexas_compile_test_14.out

mexas_compile_test_%.out:	\
		mexas \
		mexas_compile_test_%.mex \
		Makefile
	rm -f mexas_compile_test_$*.out
	-${RUNENV} ./mexas ${TRACE} \
	    mexas_compile_test_$*.mex \
	    > mexas_compile_test_$*.out

mexas_joint_test_%.out:	\
		mexas \
		mexas_joint_test_%*.mex \
		Makefile
	rm -f mexas_joint_test_$*.out
	-${RUNENV} ./mexas ${TRACE} \
	    mexas_joint_test_$*[abc].mex \
	    > mexas_joint_test_$*.out

mexas_time_test_%.out:	\
		mexas \
		mexas_time_test_%.mex \
		Makefile
	rm -f mexas_time_test_$*.out
	${RUNENV} ./mexas -pasource \
	    mexas_time_test_$*.mex \
	    ${RUN} -r MAIN \
	    > mexas_time_test_$*.out

mexas_time_test_%o.out:	\
		mexas \
		mexas_time_test_%.mex \
		Makefile
	rm -f mexas_time_test_$*o.out
	${RUNENV} ./mexas -pasource \
	    mexas_time_test_$*.mex \
	    ${RUN} -r MAIN \
	    > mexas_time_test_$*o.out

clean:
	rm -f ${TESTS:=.out} *.o mexas mexas_*_test
