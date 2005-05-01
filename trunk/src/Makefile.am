noinst_HEADERS=result.h result_queue.h queryrunner.h querywin.h resultlist.h catalog_queryrunner.h
bin_PROGRAMS=ocha ocha_indexer ocha_preferences

noinst_PROGRAMS=result_queue_check catalog_check query_check indexer_various_check catalog_queryrunner_check string_utils_check contentlist_check

TESTS=result_queue_check catalog_check query_check indexer_various_check catalog_queryrunner_check contentlist_check

query_check_SOURCES=query_check.c query.c query.h
query_check_CFLAGS=$(TEST_CFLAGS) 
query_check_LDADD=$(TEST_LIBS) 

contentlist_check_SOURCES=contentlist_check.c contentlist.c contentlist.h
contentlist_check_CFLAGS=$(TEST_CFLAGS) 
contentlist_check_LDADD=$(TEST_LIBS)  $(GNOME_CFLAGS)

result_queue_check_SOURCES=result_queue_check.c result_queue.c result_queue_check.h result.h queryrunner.h
result_queue_check_CFLAGS=$(TEST_CFLAGS)
result_queue_check_LDADD=$(TEST_LIBS)

catalog_check_SOURCES=catalog_check.c catalog.c catalog.h catalog_result.c catalog_result.h result.h 
catalog_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)
catalog_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS)

string_utils_check_SOURCES=string_utils_check.c string_utils.h string_utils.c
string_utils_check_CFLAGS=$(TEST_CFLAGS)
string_utils_check_LDADD=$(TEST_LIBS)

catalog_queryrunner_check_SOURCES=catalog_queryrunner_check.c catalog_queryrunner.c catalog_queryrunner.h catalog.c catalog.h catalog_result.c catalog_result.h result.h result_queue.c result_queue.h launcher.h launchers.h mock_launchers.h mock_launchers.c string_utils.h string_utils.c
catalog_queryrunner_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)  
catalog_queryrunner_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS) 

indexer_various_check_SOURCES=indexer_various_check.c indexer_applications.c indexer_applications.h indexer_files.c indexer_files_view.c indexer_files.h indexer_files_view.h indexer.h indexer_files.h result.h indexer_utils.h indexer_utils.c mock_catalog.c mock_catalog.h desktop_file.h desktop_file.c indexer_mozilla.c indexer_mozilla.h launchers.c launcher_application.c launcher_open.c launcher_openurl.c string_set.h string_set.c
indexer_various_check_CFLAGS=$(TEST_CFLAGS) $(GNOME_CFLAGS) 
indexer_various_check_LDADD=$(TEST_LIBS) 

ocha_indexer_SOURCES=indexer_main.c catalog.c catalog.h catalog_result.c catalog_result.h indexer_files.c indexer_files_view.c indexer_files.h indexer_files_view.h  indexer_utils.c  catalog_queryrunner.c result_queue.c indexer_applications.c indexer_applications.h desktop_file.h desktop_file.c indexer_mozilla.c indexer_mozilla.h indexers.h indexers.c ocha_init.h ocha_init.c ocha_gconf.h ocha_gconf.c string_utils.h string_utils.c launchers.c launcher_application.c launcher_open.c launcher_openurl.c string_set.h string_set.c
ocha_indexer_CFLAGS=$(GNOME_CFLAGS) $(SQLITE_CFLAGS)
ocha_indexer_LDADD=$(GNOME_LIBS) $(SQLITE_LIBS)

ocha_SOURCES=ocha_main.c ocha_init.c result_queue.c result_queue.h querywin.h querywin.c \
querywin.h catalog_queryrunner.c catalog_queryrunner.h catalog.c catalog.h catalog_result.c catalog_result.h \
query.c query.h resultlist.h resultlist.c \
launchers.c launcher_open.c launcher_openurl.c launcher_application.c desktop_file.c \
string_utils.h string_utils.c ocha_gconf.c ocha_gconf.h schedule.h schedule.c
ocha_CFLAGS=-g $(GTK_CFLAGS) $(SQLITE_CFLAGS) $(GNOME_CFLAGS) -DBINDIR=\"$(bindir)\"
ocha_LDADD=$(GTK_LIBS) $(SQLITE_LIBS) $(GNOME_LIBS)



ocha_preferences_SOURCES=preferences_main.c catalog.c catalog.h catalog_result.c catalog_result.h indexer_files.c indexer_files_view.c indexer_files.h indexer_files_view.h indexer_utils.c  catalog_queryrunner.c result_queue.c indexer_applications.c indexer_applications.h desktop_file.h desktop_file.c indexer_mozilla.c indexer_mozilla.h indexers.h indexers.c ocha_init.h ocha_init.c preferences_catalog.h preferences_catalog.c indexer_view.h indexer_view.c ocha_gconf.h ocha_gconf.c indexer.c string_utils.h string_utils.c launchers.c launcher_application.c launcher_open.c launcher_openurl.c content_view.h content_view.c contentlist.h contentlist.c string_set.h string_set.c preferences_general.h preferences_general.c
ocha_preferences_CFLAGS=$(GNOME_CFLAGS) $(SQLITE_CFLAGS)
ocha_preferences_LDADD=$(GNOME_LIBS) $(SQLITE_LIBS)

xref:
	cd @top_srcdir@ && if [ -d @top_srcdir@/Xrefs ] ; then xref -update; else xref -create; fi

doxygen:
	doxygen Doxyfile

