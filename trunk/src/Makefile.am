noinst_HEADERS=result.h result_queue.h queryrunner.h mock_queryrunner.h querywin.h resultlist.h catalog_queryrunner.h
bin_PROGRAMS=ocha ocha_indexer

noinst_PROGRAMS=result_queue_check catalog_check query_check indexer_various_check catalog_queryrunner_check

TESTS=result_queue_check catalog_check query_check indexer_various_check catalog_queryrunner_check

query_check_SOURCES=query_check.c query.c query.h
query_check_CFLAGS=$(TEST_CFLAGS) 
query_check_LDADD=$(TEST_LIBS) 

result_queue_check_SOURCES=result_queue_check.c result_queue.c result_queue_check.h result.h queryrunner.h
result_queue_check_CFLAGS=$(TEST_CFLAGS)
result_queue_check_LDADD=$(TEST_LIBS)

catalog_check_SOURCES=catalog_check.c catalog.c catalog.h catalog_result.c catalog_result.h result.h 
catalog_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)
catalog_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS)

catalog_queryrunner_check_SOURCES=catalog_queryrunner_check.c catalog_queryrunner.c catalog_queryrunner.h catalog.c catalog.h catalog_result.c catalog_result.h result.h result_queue.c result_queue.h indexer.h indexers.h mock_indexers.c 
catalog_queryrunner_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)  
catalog_queryrunner_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS) 

indexer_various_check_SOURCES=indexer_various_check.c indexer_applications.c indexer_applications.h indexer_files.c indexer.h indexer_files.h result.h indexer_utils.h indexer_utils.c mock_catalog.c mock_catalog.h desktop_file.h desktop_file.c indexer_mozilla.c indexer_mozilla.h
indexer_various_check_CFLAGS=$(TEST_CFLAGS) $(GNOME_CFLAGS)
indexer_various_check_LDADD=$(TEST_LIBS) 

ocha_indexer_SOURCES=indexer_main.c catalog.c catalog.h catalog_result.c catalog_result.h indexer_files.c indexer_utils.c  catalog_queryrunner.c result_queue.c indexer_applications.c indexer_applications.h desktop_file.h desktop_file.c indexer_mozilla.c indexer_mozilla.h indexers.h indexers.c ocha_init.h ocha_init.c
ocha_indexer_CFLAGS=$(GNOME_CFLAGS) $(SQLITE_CFLAGS)
ocha_indexer_LDADD=$(GNOME_LIBS) $(SQLITE_LIBS)

ocha_SOURCES=ocha_main.c result_queue.c result_queue.h querywin.h querywin.c \
querywin.h catalog_queryrunner.c catalog_queryrunner.h catalog.c catalog.h catalog_result.c catalog_result.h \
query.c query.h resultlist.h resultlist.c netwm.h netwm.c netwm_queryrunner.c \
netwm_queryrunner.h compound_queryrunner.h compound_queryrunner.c \
indexer_files.c indexer_utils.c  indexer_applications.c indexer_applications.h desktop_file.c desktop_file.h \
indexer_mozilla.c indexer_mozilla.h indexers.h indexers.c ocha_init.h ocha_init.c
ocha_CFLAGS=-g $(GTK_CFLAGS) $(SQLITE_CFLAGS) $(GNOME_CFLAGS)
ocha_LDADD=$(GTK_LIBS) $(SQLITE_LIBS) $(GNOME_LIBS)


xref:
	cd @top_srcdir@ && if [ -d @top_srcdir@/Xrefs ] ; then xref -update; else xref -create; fi

doxygen:
	doxygen Doxyfile

