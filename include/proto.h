

#ifndef __PROTO_H_INCLUDE__
#define __PROTO_H_INCLUDE__

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef WIN32
#include <winsock.h>
#define vsnprintf	_vsnprintf
#define snprintf	_snprintf
#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp
#define S_ISREG(x)	(x & _S_IFREG)
#define S_ISDIR(x)	(x & _S_IFDIR)
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned char boolean;

#endif /* __PROTO_H_INCLUDE__ */
