#include "democgi.h"

#define DOT1X_PREFIX	"/usr/dot1x"

static char epscgi_result_template[] = "result.html";

void epscgi_location(char *frame, char *temp_file)
{
	int res;
	tag_set_value("template", temp_file);
	res = cgi_output_template(frame);
	if (res != CGI_ERROR_SUCCESS) {
		tag_set_value("template", "main.html");
		cgi_output_template(frame);
	}
	exit(0);
}

char *epscgi_failure(char *format, ...)
{
	va_list args;
	int i;
	char buf[EPSCGI_MAX_MESSAGE+1];
	int size = EPSCGI_MAX_MESSAGE;

	va_start(args, format);
	i = vsnprintf(buf, size, format, args);
	va_end(args);

	tag_set_value("result_indicator", "Failure");
	tag_set_value("result_message", buf);
	return epscgi_result_template;
}

char *epscgi_success(char *format, ...)
{
	va_list args;
	int i;
	char buf[EPSCGI_MAX_MESSAGE+1];
	int size = EPSCGI_MAX_MESSAGE;

	va_start(args, format);
	i = vsnprintf(buf, size, format, args);
	va_end(args);

	tag_set_value("result_indicator", "Success");
	tag_set_value("result_message", buf);
	return epscgi_result_template;
}

void epscgi_load_menus()
{
	tag_list_t *list;
	int count, i;

	list = tag_load_list("main_menu");
	if (!list)
		epscgi_failure("Abnormal Configuration!");
	/* load sub menus */
	count = tag_count_items(list);
	for (i = 0; i < count; i++)
		tag_load_list(tag_get_item(list, "sub_menu", i));
}

void epscgi_load_config()
{
	static char env[MAX_PATH];
	epscgi_load_menus();
	putenv(env);
}

void epscgi_load_session()
{
	/*
	 * TODO: load session private data
	 *  1. check user crendential
	 *  2. load all user private session data
	 *  3. delete outdated session information
	 */
	tag_set_value("charset", "iso-8859-1");
	tag_set_value("login_name", "Administrator");
	tag_set_value("log_level", "20");
}

char *epscgi_ca_conf(int request)
{
	//int rc;
	
	if (request == EPSCGI_GET_REQUEST) {
		/*
		rc = eps_ssl_load_conf(EPS_SSL_CONF_OPENSSL);
		if (rc != EPS_ERROR_SUCCESS)
			return epscgi_failure("%s has not been loaded!", "SSL Configuration");
		*/
	} else {
		/*
		rc = eps_ssl_save_conf(EPS_SSL_CONF_OPENSSL);
		if (rc) {
			if (EPS_IS_SYNTAX_ERROR(rc))
				eps_restore_user_value(EPS_SSL_CONF_OPENSSL, "param", "value");
			else
				return epscgi_failure("%s has not been saved!", "SSL Configuration");
		} else
			return epscgi_success("%s has been saved!", "SSL Configuration");
		*/
	}
	return NULL;
}

#define eps_empty(x)		(!x || !strlen(x))

#define EPSCGI_BEGIN_BEAN					\
	if (eps_empty(tmplate)) {				\
		relocated = "main.html";
#define EPSCGI_END_BEAN						\
	}
#define EPSCGI_LOAD_BEAN(x)					\
	} else if (strcasecmp(tmplate, #x".html") == 0) {	\
		relocated = epscgi_##x(request);

void epscgi_admin_template(char *tmplate, int request)
{
	char *relocated = "main.html";

	//eps_log_error_message(EPS_LOG_DEBUG(2), "epscgi_admin: template(%s)", tmplate);

	EPSCGI_BEGIN_BEAN
	EPSCGI_LOAD_BEAN(ca_conf)
	EPSCGI_END_BEAN

	/*
	eps_log_error_message(EPS_LOG_DEBUG(2), "epscgi_admin: relocate(%s), template(%s)",
			      relocated, tmplate);
	*/

	if (!relocated)
		epscgi_location(EPSCGI_FRAME_ADMIN_FILE, tmplate);
	else {
		/*
		 * if we are jumping to another page, this means the page should be processed
		 * as GET method initative.
		 */
		if (tmplate && strcasecmp(tmplate, relocated) != 0 &&
		    strcasecmp(relocated, "result.html") != 0)
			epscgi_admin_template(relocated, EPSCGI_GET_REQUEST);
		else {
			epscgi_location(EPSCGI_FRAME_ADMIN_FILE, relocated);
		}
	}
}

int main(int argc, char *argv[])
{
#ifdef WIN32
#define CONFIG_EPSDIR	"/democgi"
#endif
	char *tmplate;
	int request;
	char *request_method = NULL;
	char *http_referer = NULL;
	char *buffer = NULL;
	char htdocs[MAX_PATH] = "";

	sprintf(htdocs, "%s/htdocs", CONFIG_EPSDIR);
	tag_init_service(htdocs);

	epscgi_load_config();
	epscgi_load_session();

	tmplate = tag_get_value("template");
	request_method = tag_get_env("REQUEST_METHOD");
	if (!request_method) {
		epscgi_failure("Abnormal request!");
	}
	http_referer = tag_get_env("HTTP_REFERER");

	if (strncasecmp(request_method, "POST", 4) == 0) {
		request = EPSCGI_POST_REQUEST;
	} else if (strncasecmp(request_method, "GET", 3) == 0) {
		request = EPSCGI_GET_REQUEST;
	} else {
		epscgi_failure("Abnormal request!");
	}
	epscgi_admin_template(tmplate, request);
}
