SUBDIRS=C

if HAS_XSLTPROC

man_MANS = ocha.1 ochad.1

ocha.1: ocha.xml
	-@($(XSLTPROC) --nonet ocha.xml)

ochad.1: ochad.xml
	-@($(XSLTPROC) --nonet ochad.xml)

ocha-manual.html: C/ocha-manual.xml
	-@($(XSLTPROC) --nonet -o ocha-manual.html ocha-manual.xml)

docsdir = $(datadir)/doc/@PACKAGE_NAME@
docs_DATA = ocha-manual.html

EXTRA_DIST = ocha-manual.html ocha.1 ochad.1 

endif