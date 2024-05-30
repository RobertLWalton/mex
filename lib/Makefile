# MEX Lib Makefile
#
# File:		Makefile
# Author:	Bob Walton (walton@acm.org)
# Date:		Thu May 30 09:43:21 EDT 2024
#
# The authors have placed this program in the public
# domain; they make no warranty and accept no liability
# for this program.

# The following locates MIN.
#
MIN = ~/min

MAKE = make --no-print-directory

mex-all:
	@${MAKE} -C ${MIN}/lib
	@${MAKE} libmex.a

D = $$

# The following turns on MIN debugging.
#
GFLAGS = -DMIN_PROTECT=1

# O3 is needed to inline functions.
#
OPTIMIZE = -O3

WFLAGS = -Wall -Wextra \
         -Wno-unused-parameter \
         -Wno-char-subscripts

.SUFFIXES:


MIN_H =	${MIN}/lib/libmin.a

MEX_H =	${MIN_H} \
		../include/mex.h \
		../include/mexas.h

MEX_O =	mex.o mexcom.o mexstack.o mexas.o

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


libmex.a:       ${MEX_O}
	rm -f libmex.a
	ar rs libmex.a ${MEX_O}

clean:
	rm -f *.cc *.o *.a
