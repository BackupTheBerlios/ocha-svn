
bin_PROGRAMS=

noinst_PROGRAMS=hello_check

TESTS=hello_check

hello_check_SOURCES=hello.c
hello_check_INCLUDES=@CHECK_CFLAGS@
hello_check_LDADD=@CHECK_LIBS@

#lint:
#	flags="+posixlib -booltype bool" ;\
#	lclint $$flags vpsdecoder.c stationcode.c vpsdecode.c vpsdata.c  ;\
#	lclint $$flags vpsdecode_stub.c

xref:
	if [ -d @top_srcdir@/Xrefs ] ; then xref -update @top_srcdir@; else xref -create @top_srcdir@; fi

doxygen:
	doxygen Doxyfile
