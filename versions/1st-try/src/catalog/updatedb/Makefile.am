
noinst_PROGRAMS=updatedb_check locate_check
TESTS=updatedb_check locate_check

AM_CPPFLAGS=-I../../
updatedb_check_SOURCES=updatedb_check.c
updatedb_check_INCLUDES=updatedb.h
updatedb_check_CFLAGS=$(TEST_CFLAGS)
updatedb_check_LDADD=$(TEST_LIBS)

locate_check_SOURCES=locate_check.c locate.c
locate_check_INCLUDES=locate.h
locate_check_CFLAGS=$(TEST_CFLAGS)
locate_check_LDADD=$(TEST_LIBS)
