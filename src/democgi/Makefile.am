bin_PROGRAMS = democgi
democgi_LDADD = \
	$(top_srcdir)/lib/gcgi/libcgi.a \
	$(top_srcdir)/lib/cmn/libcmn.a \
	-lm
democgi_SOURCES = \
	democgi.c democgi.h
INCLUDES = \
	-I$(top_srcdir)/include -I.
EXTRA_DIST = \
	democgi.dsp
