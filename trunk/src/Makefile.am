noinst_HEADERS=mempool.h result.h result_queue.h queryrunner.h
bin_PROGRAMS=

noinst_PROGRAMS=mempool_check result_queue_check

TESTS=mempool_check result_queue_check

mempool_check_SOURCES=mempool_check.c mempool.c mempool.h
mempool_check_CFLAGS=$(TEST_CFLAGS) 
mempool_check_LDADD=$(TEST_LIBS) 

result_queue_check_SOURCES=result_queue_check.c result_queue_check.h result.h queryrunner.h
result_queue_check_CFLAGS=$(TEST_CFLAGS)
result_queue_check_LDADD=$(TEST_LIBS)

#lint:
#	flags="+posixlib -booltype bool" ;\
#	splint $$flags target.h hello.c

xref:
	cd @top_srcdir@ && if [ -d @top_srcdir@/Xrefs ] ; then xref -update; else xref -create; fi

doxygen:
	doxygen Doxyfile

