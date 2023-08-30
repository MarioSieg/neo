## Process this file with automake to produce Makefile.in

SUBDIRS = demo

pkgtcldir = $(pkglibdir)/tcl

man = tkspline.3tk

# If all else fails there is a local copy of tkInt.h in tkstubs/
AM_CPPFLAGS = \
	-I$(TK_SRC_DIR)/generic \
	-I$(TCL_SRC_DIR)/generic \
	$(TCL_INCLUDES) $(TCLINT_INCLUDES) \
	$(TK_INCLUDES) $(TKINT_INCLUDES) -I$(top_srcdir)/tclpkg/tkstubs

AM_CFLAGS = $(X_CFLAGS) $(TCL_CFLAGS) $(TK_CFLAGS) $(TK_DEFS) \
	-UPACKAGE_NAME -UPACKAGE_STRING -UPACKAGE_TARNAME \
	-UPACKAGE_VERSION -UPACKAGE_BUGREPORT

if WITH_TK
pkgtcl_LTLIBRARIES = libtkspline.la
man_MANS = $(man)
if ENABLE_MAN_PDFS
pdf_DATA = tkspline.3tk.pdf
endif
endif

libtkspline_la_LDFLAGS = -no-undefined -module -avoid-version
libtkspline_la_SOURCES = tkspline.c
libtkspline_la_LIBADD = \
	$(top_builddir)/tclpkg/tkstubs/libtkstubs_C.la \
	$(top_builddir)/tclpkg/tclstubs/libtclstubs_C.la $(TK_LIB_SPEC)

if WITH_TK
all-local: pkgIndex.tcl

pkgIndex.tcl: libtkspline.la
	sh $(top_srcdir)/tclpkg/mkpkgindex.sh libtkspline.la Tkspline $(VERSION)
endif

.3tk.3tk.pdf:
	rm -f $@; pdffile=$@; psfile=$${pdffile%pdf}ps; \
	$(GROFF) -Tps -man $< > $$psfile || { rm -f $$psfile; exit 1; }; \
	$(PS2PDF) $$psfile && rm -f $$psfile || { rm -f $$psfile; exit 1; }
SUFFIXES = .3tk .3tk.pdf

EXTRA_DIST = $(man) dllEntry.c

CLEANFILES = pkgIndex.tcl
DISTCLEANFILES = $(pdf_DATA)
