noinst_HEADERS= \
	result.h \
	result_queue.h \
	queryrunner.h \
	querywin.h \
	resultlist.h \
	catalog_queryrunner.h
bin_PROGRAMS=ocha ochad

if RUN_TESTS
 noinst_PROGRAMS= \
	result_queue_check \
	catalog_check \
	query_check \
	indexer_various_check \
	catalog_queryrunner_check \
	string_utils_check \
	contentlist_check \
	parse_uri_list_next_check

  TESTS= \
	result_queue_check \
	catalog_check \
	query_check \
	indexer_various_check \
	catalog_queryrunner_check \
	contentlist_check \
	parse_uri_list_next_check

parse_uri_list_next_check_SOURCES=parse_uri_list_next_check.c \
	parse_uri_list_next.c parse_uri_list_next.h
parse_uri_list_next_check_CFLAGS=$(TEST_CFLAGS) $(GNOME_CFLAGS)
parse_uri_list_next_check_LDADD=$(TEST_LIBS)  $(GNOME_LIBS)

query_check_SOURCES=query_check.c \
	query.c query.h
query_check_CFLAGS=$(TEST_CFLAGS) 
query_check_LDADD=$(TEST_LIBS) 

contentlist_check_SOURCES=contentlist_check.c \
	contentlist.c contentlist.h
contentlist_check_CFLAGS=$(TEST_CFLAGS) 
contentlist_check_LDADD=$(TEST_LIBS)  $(GNOME_CFLAGS)

result_queue_check_SOURCES=result_queue_check.c \
	result_queue.c \
	result.h \
	queryrunner.h
result_queue_check_CFLAGS=$(TEST_CFLAGS)
result_queue_check_LDADD=$(TEST_LIBS)

catalog_check_SOURCES=catalog_check.c \
	catalog.c catalog.h \
	catalog_result.c catalog_result.h \
	result.h 
catalog_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)
catalog_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS)

string_utils_check_SOURCES=string_utils_check.c \
	string_utils.h string_utils.c
string_utils_check_CFLAGS=$(TEST_CFLAGS)
string_utils_check_LDADD=$(TEST_LIBS)

catalog_queryrunner_check_SOURCES= catalog_queryrunner_check.c \
        catalog.c catalog.h \
        catalog_queryrunner.c catalog_queryrunner.h \
        catalog_result.c catalog_result.h \
        launcher.h \
        launchers.h \
        mock_launchers.c mock_launchers.h \
        result.h \
        result_queue.c result_queue.h \
        string_utils.c string_utils.h 
        catalog_queryrunner_check_CFLAGS=$(TEST_CFLAGS) $(SQLITE_CFLAGS)  
catalog_queryrunner_check_LDADD=$(TEST_LIBS) $(SQLITE_LIBS) 

indexer_various_check_SOURCES=indexer_various_check.c \
        indexer_applications.c \
        indexer_applications.h \
        indexer_files.c \
        indexer_files_view.c \
        indexer_files.h \
        indexer_files_view.h \
        indexer.h \
        indexer_files.h \
        result.h \
        indexer_utils.h \
        indexer_utils.c \
        mock_catalog.c \
        mock_catalog.h \
        desktop_file.h \
        desktop_file.c \
        indexer_mozilla.c \
        indexer_mozilla.h \
        launchers.c \
        launcher_application.c \
        launcher_open.c \
        launcher_openurl.c \
        string_set.h \
        string_set.c 
indexer_various_check_CFLAGS=$(TEST_CFLAGS) $(GNOME_CFLAGS) 
indexer_various_check_LDADD=$(TEST_LIBS) 


else
  noinst_PROGRAMS= no_check
  TESTS= no_check

  no_check_SOURCES = no_check.c
endif


ochad_SOURCES= ochad_main.c \
	restart.c restart.h \
	ocha_init.c ocha_init.h \
	result_queue.c result_queue.h \
	querywin.h querywin.c \
	catalog_queryrunner.c catalog_queryrunner.h \
	catalog.c catalog.h \
	catalog_result.c catalog_result.h \
	query.c query.h \
	resultlist.h resultlist.c \
	launchers.c launchers.h \
	launcher_open.c launcher_open.h \
	launcher_openurl.c launcher_openurl.h \
	launcher_application.c launcher_application.h \
	desktop_file.c desktop_file.h \
	string_utils.h string_utils.c \
	ocha_gconf.c ocha_gconf.h \
	schedule.h schedule.c \
	accel_button.h accel_button.c
ochad_CFLAGS=-g $(GTK_CFLAGS) $(SQLITE_CFLAGS) $(GNOME_CFLAGS) -DBINDIR=\"$(bindir)\"
ochad_LDADD=$(GTK_LIBS) $(SQLITE_LIBS) $(GNOME_LIBS)


ocha_SOURCES= ocha_main.c \
	mode_install.c mode_install.h \
	mode_index.c mode_index.h \
	mode_preferences.c mode_preferences.h \
	mode_stop.c mode_stop.h \
        catalog.c catalog.h \
        catalog_queryrunner.c catalog_queryrunner.h \
        catalog_result.c catalog_result.h \
        content_view.c content_view.h \
        contentlist.c contentlist.h \
        desktop_file.c desktop_file.h \
        indexer.c indexer.h \
        indexer_applications.c indexer_applications.h \
        indexer_files.c indexer_files.h \
        indexer_files_view.c indexer_files_view.h \
        indexer_mozilla.c indexer_mozilla.h \
        indexer_utils.c indexer_utils.h \
        indexer_view.c indexer_view.h \
        indexer_views.c indexer_views.h \
        indexers.c indexers.h \
        launcher_application.c launcher_application.h \
        launcher_open.c launcher_open.h \
        launcher_openurl.c launcher_openurl.h \
        launchers.c launchers.h \
        ocha_gconf.c ocha_gconf.h \
        ocha_init.c ocha_init.h \
        preferences_catalog.c preferences_catalog.h \
        preferences_general.c preferences_general.h \
        preferences_stop.c preferences_stop.h \
        restart.c restart.h \
        result_queue.c result_queue.h \
        string_set.c string_set.h \
        string_utils.c string_utils.h \
	accel_button.h accel_button.c \
	parse_uri_list_next.h parse_uri_list_next.c

ocha_CFLAGS=$(GNOME_CFLAGS) $(SQLITE_CFLAGS) -DBINDIR=\"$(bindir)\"
ocha_LDADD=$(GNOME_LIBS) $(SQLITE_LIBS)

xref:
	cd @top_srcdir@ && if [ -d @top_srcdir@/Xrefs ] ; then xref -update; else xref -create; fi

doxygen:
	doxygen Doxyfile

