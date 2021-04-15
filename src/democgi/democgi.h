#ifndef __EPS_CGI_H_INCLUDE__
#define __EPS_CGI_H_INCLUDE__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <cgi_tag.h>

#ifdef WIN32
#include <io.h>
#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp
#define vsnprintf	_vsnprintf
#define popen		_popen
#define pclose		_pclose
#endif

#ifndef MAX_PATH
#define MAX_PATH	256
#endif

#define EPSCGI_MAX_METHOD	32
#define EPSCGI_MAX_MESSAGE	1024

#define EPSCGI_GET_REQUEST	1
#define EPSCGI_POST_REQUEST	2

#define EPSCGI_FRAME_ADMIN_FILE	"index.html"
#define EPSCGI_FRAME_INIT_FILE	"init.html"

void epscgi_location(char *frame, char *temp_file);
char *epscgi_failure(char *format, ...);
char *epscgi_success(char *format, ...);
void epscgi_admin_template(char *tmplate, int request);
void epscgi_init_template(char *tmplate, int request);

#define EPSCGI_BEGIN_BEAN					\
	if (eps_empty(tmplate)) {				\
		relocated = "main.html";
#define EPSCGI_END_BEAN						\
	}
#define EPSCGI_LOAD_BEAN(x)					\
	} else if (strcasecmp(tmplate, #x".html") == 0) {	\
		relocated = epscgi_##x(request);

char *epscgi_ldap_client(int request);
char *epscgi_netif_list(int request);
char *epscgi_netif_conf(int request);
char *epscgi_dns_conf(int request);
char *epscgi_ca_conf(int request);
char *epscgi_ca_init(int request);
char *epscgi_ca_down(int request);
char *epscgi_slapd_conf(int request);
char *epscgi_db_conf(int request);
char *epscgi_db_list(int request);
char *epscgi_proc_stat(int request);
char *epscgi_user_create(int request);
char *epscgi_user_delete(int request);
char *epscgi_user_list(int request);
char *epscgi_user_modify(int request);
char *epscgi_user_search(int request);
char *epscgi_user_cert(int request);
char *epscgi_user_issue(int request);
char *epscgi_radius_conf(int request);

#endif /* __EPS_CGI_H_INCLUDE__ */
