noinst_HEADERS=target.h mempool.h
bin_PROGRAMS=

noinst_PROGRAMS=mempool_check target_check target_pool_check

TESTS=mempool_check target_check target_pool_check

mempool_check_SOURCES=mempool_check.c mempool.c mempool.h
mempool_check_CFLAGS=$(TEST_CFLAGS) 
mempool_check_LDADD=$(TEST_LIBS) 

target_check_SOURCES=target_check.c target.c target.h mempool.c mempool.h
target_check_CFLAGS=$(TEST_CFLAGS) 
target_check_LDADD=$(TEST_LIBS) 

target_pool_check_SOURCES=target_pool_check.c target_pool.c target.h 
target_pool_check_CFLAGS=$(TEST_CFLAGS) 
target_pool_check_LDADD=$(TEST_LIBS) 


#lint:
#	flags="+posixlib -booltype bool" ;\
#	splint $$flags target.h hello.c

xref:
	if [ -d @top_srcdir@/Xrefs ] ; then xref -update @top_srcdir@; else xref -create @top_srcdir@; fi

doxygen:
	doxygen Doxyfile


SUBDIRS=catalog
