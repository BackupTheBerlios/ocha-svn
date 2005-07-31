docname=ocha-manual
omffile=ocha-manual.omf
entities=ocha-manual.xml legal.xml
lang=C
include $(top_srcdir)/doc/xmldocs.make
dist-hook: app-dist-hook

if HAS_XSLTPROC

man_MANS = ocha.1 ochad.1


ocha.1: ocha.xml
	-@($(XSLTPROC) --nonet ocha.xml)

ochad.1: ochad.xml
	-@($(XSLTPROC) --nonet ochad.xml)

ocha-manual.html: ocha-manual.xml
	-@($(XSLTPROC) --nonet -o ocha-manual.html ocha-manual.xml)

endif