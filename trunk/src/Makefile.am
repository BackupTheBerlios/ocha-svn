noinst_HEADERS=mempool.h
bin_PROGRAMS=

noinst_PROGRAMS=mempool_check 

TESTS=mempool_check 

mempool_check_SOURCES=mempool_check.c mempool.c mempool.h
mempool_check_CFLAGS=$(TEST_CFLAGS) 
mempool_check_LDADD=$(TEST_LIBS) 

#lint:
#	flags="+posixlib -booltype bool" ;\
#	splint $$flags target.h hello.c

xref:
	if [ -d @top_srcdir@/Xrefs ] ; then xref -update @top_srcdir@; else xref -create @top_srcdir@; fi

doxygen:
	doxygen Doxyfile


