noinst_LIBRARIES = libcmn.a
libcmn_a_CFLAGS = -DREGEX_MALLOC=1
libcmn_a_LIBADD =
libcmn_a_SOURCES = \
	md5.c md5.h \
	sha1.c sha1.h \
	base64.c base64.h \
	url.c url.h \
	hmac.c hmac.h \
	regex.c regex.h \
	isaac.c isaac.h \
	argcv.c \
	getline.c vector.c
INCLUDES = \
	-I$(top_srcdir)/include -I.
EXTRA_DIST = \
	cmn.dsp
