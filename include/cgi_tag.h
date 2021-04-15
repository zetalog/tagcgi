/*
 * ZETALOG's Personal COPYRIGHT
 *
 * Copyright (c) 2006
 *    ZETALOG - "Lv ZHENG".  All rights reserved.
 *    Author: Lv "Zetalog" Zheng
 *    Internet: zetalog@gmail.com
 *
 * This COPYRIGHT used to protect Personal Intelligence Rights.
 * Redistribution and use in source and binary forms with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the Lv "Zetalog" ZHENG.
 * 3. Neither the name of this software nor the names of its developers may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 4. Permission of redistribution and/or reuse of souce code partially only
 *    granted to the developer(s) in the companies ZETALOG worked.
 * 5. Any modification of this software should be published to ZETALOG unless
 *    the above copyright notice is no longer declaimed.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ZETALOG AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE ZETALOG OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)cgi_tag.h: tagging common gateway interface (CGI) definitions
 * $Id: cgi_tag.h,v 1.12 2006/10/12 00:47:56 zhenglv Exp $
 */
#ifndef __CGI_TAG_H_INCLUDE__
#define __CGI_TAG_H_INCLUDE__

#include <list.h>
#include <vector.h>
#include <stdarg.h>

#define CONFIG_GCGI		1
//#define CONFIG_CGI_DEBUG	1
/* #undef CONFIG_PROTECT_WRITE	*/
#define CONFIG_CGI_MAX_QUERY	256

#ifdef CONFIG_CGUI_MAX_QUERY
#define CGI_MAX_QUERY	CONFIG_CGUI_MAX_QUERY
#else
#define CGI_MAX_QUERY	256
#endif

#ifdef CONFIG_CGI_DEBUG
#define cgi_debug	printf
#else
static void cgi_debug(char *format, ...) {};
#endif

/* error indicators */
#define CGI_ERROR_SUCCESS	0
#define CGI_ERROR_MEMORY	1
#define CGI_ERROR_PARAMETER	2
#define CGI_ERROR_CONFIG	3
#define CGI_ERROR_SESSION	4
#define CGI_ERROR_FILE		5

int cgi_output_content(char *type, char *file);
int cgi_output_template(char *file);
int cgi_output_message(char *message);

typedef struct _tag_list_t {
	char *name;		/* list file name */
	vector_t *title;	/* title */
	vector_t *table;	/* name / value table, index=0 means header */
	list_t list;
} tag_list_t;

extern list_t tag_lists;

/* tag-CGI functions */
int tag_init_service(char *root);
char *tag_output_template(char *file, char *old_buffer);

/* keyword processing */
char *tag_get_value(char *name);
void tag_set_value(char *name, char *value);
void tag_print_value(char *name, char *format, ...);
void tag_unset_value(char *name);
int tag_calc_logic(char *value);

/* hash list processing */
tag_list_t *tag_load_list(char *name);
int tag_save_list(tag_list_t *list);
void tag_destroy_list(tag_list_t *list);
/* find column number of the title */
int tag_find_title(tag_list_t *list, char *title);
/* find row number of the title/value pair */
int tag_find_cell(tag_list_t *list, char *title, char *value);
char *tag_get_item(tag_list_t *list, char *title, int current);
void tag_set_item(tag_list_t *list, char *title, int current, char *value);
int tag_set_cell(tag_list_t *list, char *title, int index, char *value);
char *tag_get_index(tag_list_t *list, int index, int current);
void tag_set_index(tag_list_t *list, int index, int current, char *value);
int tag_count_items(tag_list_t *list);

char *tag_get_location(char *list, char *locate_title,
		       char *locate_value, char *title);
void tag_set_location(char *list, char *locate_title,
		      char *locate_value, char *title, char *value);

char *tag_get_env(char *name);

int tag_load_session(int id);
int tag_save_session(int id);

#endif /* __CGI_TAG_H_INCLUDE__ */
