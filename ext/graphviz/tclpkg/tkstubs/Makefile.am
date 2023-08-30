# Process this file with automake to produce Makefile.in

if WITH_TK
noinst_LTLIBRARIES = libtkstubs_C.la
noinst_HEADERS = tkInt.h
endif

# tkStubLib.c is a dummy file just to fool automake
libtkstubs_C_la_SOURCES = tkStubLib.c tkStubImg.c
libtkstubs_C_la_CPPFLAGS = $(TCL_CFLAGS) $(TK_CFLAGS) $(TCL_INCLUDES) $(TK_INCLUDES) $(TK_XINCLUDES)
libtkstubs_C_la_LIBADD = $(TK_STUB_LIB_SPEC)
libtkstubs_C_la_LDFLAGS = -module -avoid-version
