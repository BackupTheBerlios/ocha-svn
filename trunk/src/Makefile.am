noinst_HEADERS=mempool.h result.h result_queue.h queryrunner.h mock_queryrunner.h querywin.h
bin_PROGRAMS=ocha

noinst_PROGRAMS=mempool_check result_queue_check catalog_check

TESTS=mempool_check result_queue_check catalog_check

mempool_check_SOURCES=mempool_check.c mempool.c mempool.h
mempool_check_CFLAGS=$(TEST_CFLAGS) 
mempool_check_LDADD=$(TEST_LIBS) 

result_queue_check_SOURCES=result_queue_check.c result_queue.c result_queue_check.h result.h queryrunner.h
result_queue_check_CFLAGS=$(TEST_CFLAGS)
result_queue_check_LDADD=$(TEST_LIBS)

catalog_check_SOURCES=catalog_check.c catalog.c catalog.h result.h 
catalog_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)
catalog_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS)

ocha_SOURCES=main.c result_queue.c result_queue.h querywin.h querywin.c querywin.h catalog_queryrunner.c catalog_queryrunner.h catalog.c catalog.h
ocha_CFLAGS=$(GTK_CFLAGS) $(SQLITE_CFLAGS)
ocha_LDADD=$(GTK_LIBS) $(SQLITE_LIBS)

#lint:
#	flags="+posixlib -booltype bool" ;\
#	splint $$flags target.h hello.c

xref:
	cd @top_srcdir@ && if [ -d @top_srcdir@/Xrefs ] ; then xref -update; else xref -create; fi

doxygen:
	doxygen Doxyfile

