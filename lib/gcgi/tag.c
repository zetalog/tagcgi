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
 * @(#)tag.c: tagging common gateway interface (CGI) routines
 * $Id: tag.c,v 1.36 2006/10/18 16:43:21 zhenglv Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include "cgi_tag.h"

/* TODO: change all vector_t stuff to list_t */
#include "vector.h"
#ifdef CONFIG_GCGI
#include <gcgi.h>
#include "gcgi_private.h"
#else
#error gCGI is required currently!
#endif
#include "getline.h"

#ifdef WIN32
#define MAX_PATH	_MAX_PATH
#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp
#define vsnprintf	_vsnprintf
#define popen		_popen
#define pclose		_pclose
#endif

#ifndef MAX_PATH
#define MAX_PATH	256
#endif

/* commentary for list file */
#define TAG_COMMENT_SIGN	';'

#define TAG_TOKEN_UNKNOWN	-1

/* value processing */
#define TAG_TOKEN_VALUE		1

/* list processing */
#define TAG_TOKEN_COUNT		2
#define TAG_TOKEN_INDEX		3
#define TAG_TOKEN_ITEM		4
#define TAG_TOKEN_TITLE		5
#define TAG_TOKEN_NEW		6
#define TAG_TOKEN_DELETE	7

/* block processing */
#define TAG_TOKEN_INCLUDE	8
#define TAG_TOKEN_FOR		9
#define TAG_TOKEN_END		10
#define TAG_TOKEN_IF		11
#define TAG_TOKEN_ELSE		12
#define TAG_TOKEN_BREAK		13
#define TAG_TOKEN_CONTINUE	14

/* logical processing */
#define TAG_TOKEN_NOT		15
#define TAG_TOKEN_AND		16
#define TAG_TOKEN_OR		17
#define TAG_TOKEN_EQ		18
#define TAG_TOKEN_NEQ		19
#define TAG_TOKEN_GT		20
#define TAG_TOKEN_GTEQ		21
#define TAG_TOKEN_LT		22
#define TAG_TOKEN_LTEQ		23

#define TAG_STATE_UNKNOWN	-1
#define TAG_STATE_DOLLAR	0
#define TAG_STATE_TOKEN		1
#define TAG_STATE_PARAMETER	2

/* used with logical processing */
#define TAG_VALUE_NULL		"null"
#define TAG_VALUE_NONE		"none"
#define TAG_VALUE_FALSE		"false"
#define TAG_VALUE_TRUE		"true"
#define TAG_VALUE_NO		"no"
#define TAG_VALUE_YES		"yes"

#define TAG_MAX_NUMBER		10

typedef struct _tag_token_t {
	int type;
	int state;
	vector_t *params;
	int nparam;
} tag_token_t;

typedef struct _tag_block_t {
#define TAG_START_FIRST		0
	int start;
#define TAG_LIMIT_NONE		-1
	int limit;
	int match;
	vector_t *match_stack;
	int current;
	long offset;
	tag_list_t *list;
} tag_block_t;

static const char *tag_keywords[] = {
	"tag_value",
	"tag_count",
	"tag_index",
	"tag_item",
	/* TODO: following 3 list processing */
	"tag_title",
	"tag_new",
	"tag_delete",

	"tag_include",		/* file inclusion */
	"tag_for",		/* new block */
	"tag_end",		/* end block */
	"tag_if",		/* block logic match */
	"tag_else",		/* block logic not match */
	/* TODO: following 2 block processing */
	"tag_break",		/* block break */
	"tag_continue",		/* block continue */

	"tag_not",		/* ! */
	"tag_and",		/* && */
	"tag_or",		/* || */
	"tag_eq",		/* == */
	"tag_neq",		/* != */
	/* TODO: following 4 logical processing */
	"tag_gt",		/* > */
	"tag_lt",		/* < */
	"tag_gteq",		/* >= */
	"tag_lteq",		/* <= */
};

list_t tag_tokens;
list_t tag_lists;
char *tag_template_root = NULL;

tag_token_t *tag_current_token;
vector_t *tag_token_stack;
tag_block_t *tag_current_block;
vector_t *tag_block_stack;
int tag_current_state;
int tag_want_linefeed;
vector_t *tag_state_stack;
tag_list_t *tag_null_list = NULL;

/*
 * XXX: WARNING - do not remove this unless you know why
 * this is maintained for some syntaxes requiring word numbers.
 */
int tag_nwords = 0;

#define tag_is_block_token(token)	\
	(token->type == TAG_TOKEN_FOR || token->type == TAG_TOKEN_IF)
#define tag_is_block_processing()	\
	(element_count(tag_block_stack) > 0)

/*
 * syntax elements:
 *   list: used to hold $tag_list
 *   token: used to hold all $tag_*** tokens
 *   block: used to handle $tag_for / $tag_if / $tag_else / $tag_end
 */
tag_list_t *tag_create_list(char *name);
void tag_destroy_list(tag_list_t *list);
tag_list_t *tag_find_list(char *name);
tag_token_t *tag_create_token(int type);
void tag_destroy_token(tag_token_t *token);
tag_block_t *tag_create_block(int start, int limit, long offset, int match, tag_list_t *list);
void tag_destroy_block(tag_block_t *block);
tag_block_t *tag_find_block(char *name);

/* parser & scanner */
int tag_scan_stream(FILE *stream, char **bufptr, int *n, int *word_begin);
char *tag_parse_stream(FILE *stream, char *word);
char *tag_process_stream(FILE *stream, char *old_buffer);

/* token execution */
char* tag_execute_command(FILE *stream, int type, int argc, char **argv);

/* list processing */
char *tag_process_value(FILE *stream, int argc, char **argv);
char *tag_process_count(FILE *stream, int argc, char **argv);
char *tag_process_index(FILE *stream, int argc, char **argv);
char *tag_process_item(FILE *stream, int argc, char **argv);
/* logical processing */
char *tag_process_not(FILE *stream, int argc, char **argv);
char *tag_process_and(FILE *stream, int argc, char **argv);
char *tag_process_or(FILE *stream, int argc, char **argv);
char *tag_process_eq(FILE *stream, int argc, char **argv);
char *tag_process_neq(FILE *stream, int argc, char **argv);
char *tag_process_gt(FILE *stream, int argc, char **argv);
char *tag_process_gteq(FILE *stream, int argc, char **argv);
char *tag_process_lt(FILE *stream, int argc, char **argv);
char *tag_process_lteq(FILE *stream, int argc, char **argv);
/* block processing */
char *tag_process_include(FILE *stream, int argc, char **argv);
char *tag_process_for(FILE *stream, int argc, char **argv);
char *tag_process_if(FILE *stream, int argc, char **argv);
char *tag_process_else(FILE *stream, int argc, char **argv);
char *tag_process_end(FILE *stream, int argc, char **argv);

/* csv file utility */
vector_t *csv_parse_quoted(char *line);
char *csv_from_word(const char *word_in);
char *csv_to_word(const char *word_in);
int csv_comment_line(char *line);

int tag_init_cgi();
tag_list_t *tag_create_value(char *name);
char *tag_fetch_value(char *name, char *key);

char *append_string(char *old_str, const char *str);
void chomp(char *line);
int create_buffer(char **bufptr, int *n, char **posptr, int *avail);
int read_char(FILE *stream, char **bufptr, int *n, char **posptr, int *avail, char *pchar);
extern void strupper(char *string);

int tag_scan_stream(FILE *stream, char **bufptr, int *n, int *word_begin)
{
	int nchars_avail;   /* Allocated but unused chars in *LINEPTR.  */
	char *read_pos;     /* Where we're reading into *LINEPTR. */
	int ret;
	
	int word_started = 0;
	int double_quoted = 0;
	
	*word_begin = -1;
	
	if (create_buffer(bufptr, n, &read_pos, &nchars_avail) < 0)
		return -1;

	for (; ; ) {
		unsigned char c, next;
		int res;
		
		res = read_char(stream, bufptr, n, &read_pos, &nchars_avail, &c);
		
		if (res < 0)
			return -1;
		else if (res == 0)
			break;
		
		if (double_quoted) {
			/* XXX: currently, we do not support in-quoter tags */
			if (c == '\\') {
				res = read_char(stream, bufptr, n, &read_pos, &nchars_avail, &next);
				
				if (res < 0)
					return -1;
				else if (res == 0)
					break;
				
				continue;
			} else if (c == '\"') {
				double_quoted = 0;
				break;
			} else
				continue;
		}

		switch (c) {
		case '$':
			if (tag_current_state == TAG_STATE_UNKNOWN ||
			    tag_current_state == TAG_STATE_PARAMETER) {
				if (word_started) {
					ungetc(c, stream);
					read_pos--;
				} else {
					*word_begin = read_pos - *bufptr -1;
				}
				goto break_char;
			}
			break;
		case '(':
			if (tag_current_state == TAG_STATE_DOLLAR && word_started) {
				ungetc(c, stream);
				read_pos--;
				goto break_char;
			} else if (tag_current_state == TAG_STATE_TOKEN && !word_started) {
				*word_begin = read_pos - *bufptr -1;
				goto break_char;
			}
			break;
		case ')':
		case ',':
			if (tag_current_state == TAG_STATE_PARAMETER) {
				if (word_started) {
					ungetc(c, stream);
					read_pos--;
				} else {
					*word_begin = read_pos - *bufptr -1;
				}
				goto break_char;
			}
			break;
		case '\"':
			/* quoter for constants */
			if (tag_current_state == TAG_STATE_PARAMETER) {
				if (!word_started) {
					word_started = double_quoted = 1;
					*word_begin = read_pos - *bufptr -1;
					break;
				} else {
					ungetc(c, stream);
					read_pos--;
					goto break_char;
				}
			}
			break;
		case '\n':
		case '\r':
		case '\t':
		case ' ':
			/* spaces */
			if (word_started) {
				ungetc(c, stream);
				read_pos--;
				goto break_char;
			}
			else
				*word_begin = read_pos - *bufptr;
			break;
		default:
			/* function or parameters */
			if (tag_current_state != TAG_STATE_UNKNOWN && !word_started) {
				word_started = 1;
				*word_begin = read_pos - *bufptr -1;
			}
			break;
		}
		continue;
	}
	
break_char:
	/* Done - NUL terminate and return the number of chars read.  */
	*read_pos = '\0';
	
	ret = read_pos - (*bufptr);
	tag_nwords++;
	return ret;
}

char *tag_parse_stream(FILE *stream, char *word)
{
	char *result = NULL;

	/* determine line type */
	switch (tag_current_state) {
	case TAG_STATE_UNKNOWN:
		if (strcasecmp("$", word) == 0) {
			tag_current_state = TAG_STATE_DOLLAR;
		}
		break;
	case TAG_STATE_DOLLAR:
		if (strcasecmp("tag_value", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_VALUE);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_count", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_COUNT);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_index", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_INDEX);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_item", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_ITEM);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_not", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_NOT);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_and", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_AND);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_or", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_OR);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_eq", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_EQ);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_neq", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_NEQ);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_include", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_INCLUDE);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_for", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_FOR);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_else", word) == 0) {
			result = tag_execute_command(stream, TAG_TOKEN_ELSE, 0, NULL);
			tag_current_state = TAG_STATE_UNKNOWN;
		} else if (strcasecmp("tag_if", word) == 0) {
			push_element(tag_token_stack, tag_current_token);
			tag_current_token = tag_create_token(TAG_TOKEN_IF);
			tag_current_state = TAG_STATE_TOKEN;
		} else if (strcasecmp("tag_end", word) == 0) {
			result = tag_execute_command(stream, TAG_TOKEN_END, 0, NULL);
			tag_current_state = TAG_STATE_UNKNOWN;
		} else if (strcasecmp("tag_break", word) == 0) {
			;
		} else if (strcasecmp("tag_continue", word) == 0) {
			;
		} else {
			/* make $ as escaped character */
			tag_current_state = TAG_STATE_UNKNOWN;
			return strdup(word);
		}
		break;
	case TAG_STATE_TOKEN:
		if (word[0] == '(') {
			tag_current_state = TAG_STATE_PARAMETER;
		} else {
			assert(0);
		}
		break;
	case TAG_STATE_PARAMETER:
		if (word[0] == '\"') {
			;
		}
		else if (word[0] == '$') {
			/* expression in expression */
			push_element(tag_state_stack, (void *)tag_current_state);
			tag_current_state = TAG_STATE_DOLLAR;
		}
		else if (word[0] == ')') {
			tag_token_t *token;

			tag_current_token->nparam++;

			/* save and prepare new params */
			tag_current_state = (int)pop_element(tag_state_stack);
			if (tag_current_state != TAG_STATE_PARAMETER)
				tag_current_state = TAG_STATE_UNKNOWN;
			token = tag_current_token;
			if (!tag_is_block_token(token)) {
				tag_current_token = pop_element(tag_token_stack);
			}

			/* execute commands */
			result = tag_execute_command(stream, token->type,
						     token->nparam,
						     (char **)token->params->elements);
			if (result && tag_current_state == TAG_STATE_PARAMETER) {
				char *element = (char *)get_element(tag_current_token->params,
								    tag_current_token->nparam);
				set_element(tag_current_token->params, tag_current_token->nparam,
					    append_string(element, result));
				free(result);
				result = NULL;
			}

			/* [if / for] need not to be destroyed, they should be destroyed at [end] block */
			if (!tag_is_block_token(token)) {
				tag_destroy_token(token);
			}
		}
		else if (word[0] == ',')
			tag_current_token->nparam++;
		else {
			char *element = (char *)get_element(tag_current_token->params,
							    tag_current_token->nparam);
			set_element(tag_current_token->params, tag_current_token->nparam,
				    append_string(element, word));
		}
		break;
	}

	return result;
}

char *tag_process_stream(FILE *stream, char *old_buffer)
{
	char *buffer = NULL;
	int size = 0;
	int length = 0;
	int begin;
	int stored = 0;
	char *new_string = NULL;

	char *result = old_buffer;
	
	assert(stream);
	
	while (!feof(stream)) {
		char *word = NULL;
		
		length = tag_scan_stream(stream, &buffer, &size, &begin);
		
		if (length < 0) {
			/* do not report error in the end of file */
			if (!feof(stream)) {
				if (result)
					free(result);
				if (buffer)
					free(buffer);
				return NULL;
			}
			break;
		}
		
		assert(begin > -1);
		word = buffer+begin;

#ifdef CONFIG_CGI_DEBUG
		if (tag_is_block_processing()) {
			static int last_matching = 1;
			if (last_matching != tag_current_block->match) {
				cgi_debug("  current_block: %smatched<br>\n",
					  tag_current_block->match ? "" : "not ");
				last_matching = tag_current_block->match;
			}
		}
#endif

		/* store unparsed */
		stored = length-strlen(word);
		if (stored && tag_current_state == TAG_STATE_UNKNOWN) {
			if (tag_current_block->match) {
				char *add_string = NULL;
				new_string = malloc(length+1);
				memcpy(new_string, buffer, stored);
				new_string[stored] = '\0';
				add_string = new_string;
				if (tag_want_linefeed) {
					char *line = strchr(new_string, '\n');
					if (line) {
						add_string = line+1;
						tag_want_linefeed = 0;
					}
				}
				result = append_string(result, add_string);
				free(new_string);
			}
		}

		/* always parse token */
		new_string = tag_parse_stream(stream, word);
		if (new_string) {
			if (tag_current_block->match) {
				result = append_string(result, new_string);
			}
			free(new_string);
		}
	}
	
	if (buffer) free(buffer);
	return result;
}

char *tag_process_value(FILE *stream, int argc, char **argv)
{
	char *value = NULL;

	cgi_debug("tag_value(%s)<br>\n", argv[0]);
	value = tag_get_value(argv[0]);
	if (value) {
		cgi_debug("  value_string: found %s=%s<br>\n", argv[0], value);
		return strdup(value);
	}
	return value;
}

char *tag_process_include(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_include(%s)<br>\n", argv[0]);
	return tag_output_template(argv[0], NULL);
}

char *tag_process_for(FILE *stream, int argc, char **argv)
{
	int start, limit;
	tag_list_t *list;

	cgi_debug("tag_for(%s, %s, %s)<br>\n", argv[0], argv[1], argv[2]);
	push_element(tag_block_stack, tag_current_block);
	if (argc < 1 || !argv[0] || !strlen(argv[0]))
		list = tag_null_list;
	else {
		list = tag_find_list(argv[0]);
		if (!list)
			list = tag_load_list(argv[0]);
	}
	if (argc < 2)
		start = TAG_START_FIRST;
	else
		start = atoi(argv[1]);
	if (argc < 3)
		limit = TAG_LIMIT_NONE;
	else
		limit = atoi(argv[2]);
	tag_current_block = tag_create_block(start, limit, ftell(stream),
					     tag_current_block->match, list);
	return NULL;
}

char *tag_process_if(FILE *stream, int argc, char **argv)
{
	int match = tag_current_block->match;
	cgi_debug("tag_if(%s)<br>\n", argv[0]);
	push_element(tag_current_block->match_stack,
		     (void *)tag_current_block->match);
	if (match) {
		if (argv[0] && strlen(argv[0]))
			tag_current_block->match = tag_calc_logic(argv[0]);
		else
			tag_current_block->match = 0;
	} else
		tag_current_block->match = match;
	return NULL;
}

char *tag_process_else(FILE *stream, int argc, char **argv)
{
	int last_match;
	cgi_debug("tag_else<br>\n");

	last_match = (int)pop_element(tag_current_block->match_stack);
	if (last_match)
		tag_current_block->match = !tag_current_block->match;
	push_element(tag_current_block->match_stack, (void *)last_match);
	return NULL;
}

char *tag_process_end(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_end<br>\n");
	if (tag_current_token->type == TAG_TOKEN_IF) {
		tag_current_block->match = (int)pop_element(tag_current_block->match_stack);
		tag_destroy_token(tag_current_token);
		tag_current_token = pop_element(tag_token_stack);
	} else if (tag_current_token->type == TAG_TOKEN_FOR) {
		tag_current_block->current++;
		if (tag_current_block->current < tag_count_items(tag_current_block->list)) {
			fseek(stream, tag_current_block->offset, SEEK_SET);
		} else {
			tag_destroy_token(tag_current_token);
			tag_current_token = pop_element(tag_token_stack);
			tag_destroy_block(tag_current_block);
			tag_current_block = pop_element(tag_block_stack);
			tag_current_state = TAG_STATE_UNKNOWN;
		}
	}
	return NULL;
}

/*
 * tag_item([list], title, [index])
 */
char *tag_process_item(FILE *stream, int argc, char **argv)
{
	tag_block_t *block;
	char *title;
	char *result;
	int current;
	tag_list_t *list;

	cgi_debug("tag_item(%s, %s, %s)<br>\n", argv[0], argv[1], argv[2]);
	if (argc < 1 || !argv[0] || !strlen(argv[0]))
		return NULL;
	if (argc < 2) {
		title = argv[0];
		block = tag_current_block;
		if (!block) return NULL;
		current = block->current;
		list = block->list;
	} else if (argc < 3) {
		block = tag_find_block(argv[0]);
		if (!block) return NULL;
		title = argv[1];
		current = block->current;
		list = block->list;
	} else {
		list = tag_find_list(argv[0]);
		title = argv[1];
		current = atoi(argv[2]);
	}
	if (!list)
		return NULL;
	result = tag_get_item(list, title, current);
	if (result) {
		cgi_debug("  item_string: found %s=%s<br>\n", title, result);
		return strdup(result);
	}
	return NULL;
}

char *tag_process_index(FILE *stream, int argc, char **argv)
{
	tag_block_t *block;
	int index;
	char current[TAG_MAX_NUMBER+1] = "0";

	cgi_debug("tag_index(%s, %s)<br>\n", argv[0], argv[1]);
	if (argc < 1 || !argv[0] || !strlen(argv[0]))
		goto end;
	if (argc < 2) {
		index = atoi(argv[0]);
		block = tag_current_block;
	} else {
		block = tag_find_block(argv[0]);
		if (!block) goto end;
		index = atoi(argv[1]);
	}
	snprintf(current, TAG_MAX_NUMBER, "%d", block->current);
end:
	return strdup(current);
}

char *tag_process_count(FILE *stream, int argc, char **argv)
{
	tag_list_t *list;
	char count[TAG_MAX_NUMBER+1] = "0";

	cgi_debug("tag_count(%s)<br>\n", argv[0]);
	if (argc < 1 || !argv[0] || !strlen(argv[0]))
		goto end;
	list = tag_find_list(argv[0]);
	if (!list)
		tag_load_list(argv[0]);
	if (!list) goto end;
	snprintf(count, TAG_MAX_NUMBER, "%d", tag_count_items(list));
end:
	return strdup(count);
}

int tag_is_numeric(char *value)
{
	return 0;
}

int tag_calc_logic(char *value)
{
	if (!value || !strlen(value) || 
	    strcasecmp(value, TAG_VALUE_NULL) == 0 ||
	    strcasecmp(value, TAG_VALUE_NONE) == 0 ||
	    strcasecmp(value, TAG_VALUE_NO) == 0 ||
	    strcasecmp(value, TAG_VALUE_FALSE) == 0)
		return 0;
	else if (tag_is_numeric(value))
		return atoi(value) != 0;
	else
		return 1;
}

char *tag_process_not(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_not(%s)<br>\n", argv[0]);
	if (!tag_calc_logic(argv[0]))
		return strdup(TAG_VALUE_TRUE);
	return strdup(TAG_VALUE_FALSE);
}

char *tag_process_and(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_and(%s, %s)<br>\n", argv[0], argv[1]);
	if (!tag_calc_logic(argv[0]))
		return strdup(TAG_VALUE_FALSE);
	else if (!tag_calc_logic(argv[1]))
		return strdup(TAG_VALUE_FALSE);
	return strdup(TAG_VALUE_TRUE);
}

char *tag_process_or(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_or(%s, %s)<br>\n", argv[0], argv[1]);
	if (tag_calc_logic(argv[0]))
		return strdup(TAG_VALUE_TRUE);
	else if (tag_calc_logic(argv[1]))
		return strdup(TAG_VALUE_TRUE);
	return strdup(TAG_VALUE_FALSE);
}

char *tag_process_eq(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_eq(%s, %s)<br>\n", argv[0], argv[1]);
	if (argc < 2)
		return strdup(TAG_VALUE_FALSE);
	if (argv[0] == argv[1])
		return strdup(TAG_VALUE_TRUE);
	if (argv[0] && argv[1] && strcmp(argv[0], argv[1]) == 0)
		return strdup(TAG_VALUE_TRUE);
	return strdup(TAG_VALUE_FALSE);
}

char *tag_process_neq(FILE *stream, int argc, char **argv)
{
	cgi_debug("tag_neq(%s, %s)<br>\n", argv[0], argv[1]);
	if (argc < 2)
		return strdup(TAG_VALUE_FALSE);
	if (argv[0] == argv[1])
		return strdup(TAG_VALUE_FALSE);
	if (argv[0] && argv[1] && strcmp(argv[0], argv[1]) == 0)
		return strdup(TAG_VALUE_FALSE);
	return strdup(TAG_VALUE_TRUE);
}

char* tag_execute_command(FILE *stream, int type, int argc, char **argv)
{
	char *res = NULL;

	switch (type) {
		/* list processing */
	case TAG_TOKEN_VALUE:
		return tag_process_value(stream, argc, argv);
	case TAG_TOKEN_COUNT:
		return tag_process_count(stream, argc, argv);
	case TAG_TOKEN_INDEX:
		return tag_process_index(stream, argc, argv);
	case TAG_TOKEN_ITEM:
		return tag_process_item(stream, argc, argv);
		/* logical processing */
	case TAG_TOKEN_NOT:
		return tag_process_not(stream, argc, argv);
	case TAG_TOKEN_AND:
		return tag_process_and(stream, argc, argv);
	case TAG_TOKEN_OR:
		return tag_process_or(stream, argc, argv);
	case TAG_TOKEN_EQ:
		return tag_process_eq(stream, argc, argv);
	case TAG_TOKEN_NEQ:
		return tag_process_neq(stream, argc, argv);
		/* block processing */
	case TAG_TOKEN_INCLUDE:
		res = tag_process_include(stream, argc, argv);
		tag_want_linefeed = 1;
		break;
	case TAG_TOKEN_FOR:
		res = tag_process_for(stream, argc, argv);
		tag_want_linefeed = 1;
		break;
	case TAG_TOKEN_END:
		res = tag_process_end(stream, argc, argv);
		tag_want_linefeed = 1;
		break;
	case TAG_TOKEN_IF:
		res = tag_process_if(stream, argc, argv);
		tag_want_linefeed = 1;
		break;
	case TAG_TOKEN_ELSE:
		res = tag_process_else(stream, argc, argv);
		tag_want_linefeed = 1;
		break;
	default:
		break;
	}
	return res;
}

char *tag_output_template(char *file, char *old_buffer)
{
	char tmp_file[MAX_PATH+1];
	int size = 0;
	FILE *stream = NULL;
	char *buffer = NULL;
	
	sprintf(tmp_file, "%s/%s", tag_template_root, file);
	if (!old_buffer) {
		/* TODO: start dumping */
		/*tag_save_session(atoi(tag_get_value("session")));*/
	}

	stream = fopen(tmp_file, "r");
	if (!stream)
		goto failure;

	buffer = tag_process_stream(stream, old_buffer);
	fclose(stream);
	if (buffer)
		return buffer;

failure:
	return tag_output_template("main.html", NULL);
}

int cgi_output_message(char *message)
{
	printf("Content-Type: text/plain\r\n");
	printf("Content-Length: %d\r\n\r\n", strlen(message));
	printf("%s", message);
	return CGI_ERROR_SUCCESS;
}

int cgi_output_template(char *file)
{
	char *buffer = tag_output_template(file, NULL);

	if (buffer) {
		printf("Content-Type: text/html\r\n\r\n");
		printf("%s", buffer);
		return CGI_ERROR_SUCCESS;
	} else {
		return CGI_ERROR_CONFIG;
	}
}

int cgi_output_content(char *type, char *file)
{
	FILE *fp = NULL;
	int res, n, m, stsize;
	char buf[64];
	struct stat stbuf;
	char *name = NULL;
	int rc = CGI_ERROR_SUCCESS;

	if (!type || !file)
		return CGI_ERROR_PARAMETER;
	name = strrchr(file, '/');
	if (name)
		name++;
	else
		name = file;
	if (stat(file, &stbuf) == 0) {
		stsize = stbuf.st_size;
		printf("Content-Type: %s\r\n", type);
		printf("Content-Disposition: attachment; filename=\"%s\"\r\n", name);
		printf("Content-Length: %d\r\n", stsize);
		printf("\r\n", type);
		fp = fopen(file, "rb");
		if (fp) {
			while (!feof(fp)) {
				n = fread(buf, sizeof (char), sizeof (buf), fp);
				if (n > 0) {
					res = 0;
					do {
						m = fwrite(buf, sizeof (char), n-res, stdout);
						if (m > 0) res += m;
						else break;
					} while (res < n);
					if (m < 0) break;
				}
				else break;
			}
			fclose(fp);
		}
		if (n < 0 || m < 0) {
			rc = CGI_ERROR_FILE;
			goto end;
		}
	} else
		return CGI_ERROR_FILE;

end:
	return rc;
}

/* TODO: CSV functions should be extended to support wchar for globalization */
#define CSV_TERMINATOR		'\0'
#define CSV_QUOTER		'\"'
#define CSV_SPACES		" \r\n\t"

char *csv_from_word(const char *word_in)
{
	int index = 0;
	int word_len = 0;
	char *word_out = NULL;
	int index_out = 0;
	int append = 0;
	
	word_len = strlen(word_in);
	
	if (!(word_in && word_len >= 2 && 
		((word_in[0] == '\"') && (word_in[strlen(word_in) - 1] == '\"')) ||
		((word_in[0] == '\'') && (word_in[strlen(word_in) - 1] == '\''))))
		return NULL;
	
	word_out = (char *)malloc(sizeof(char)*word_len*2+3);
	if (!word_out)
		return NULL;
	
	word_out[index_out++] = '\"';
	do {
		unsigned char c = *(word_in+index);
		index++;
		switch (c) {
		case '\"':
			word_out[index_out++] = '\"';
			word_out[index_out++] = '\"';
			break;
		case '\n':
			word_out[index_out++] = '\\';
			word_out[index_out++] = 'n';
			break;
		case '\r':
			word_out[index_out++] = '\\';
			word_out[index_out++] = 'r';
			break;
		default:
			word_out[index_out++] = c;
			break;
		}
		
	} while (index < word_len);
	word_out[index_out++] = '\"';
	word_out[index_out] = CSV_TERMINATOR;
	
	assert(word_out && (strlen(word_out) >= 2) && (word_out[0] == '\"') && 
		(word_out[strlen(word_out) - 1] == '\"'));
	
	return strdup(word_out);
}

char *csv_to_word(const char *word_in)
{
	int index = 0;
	int word_len = 0;
	char *word_out = NULL;
	unsigned char next_char;
	int index_out = 0;
	
	word_len = strlen(word_in);
	
	if (!(word_in && word_len >= 2 && (word_in[0] == '\"') && 
		(word_in[strlen(word_in) - 1] == '\"')))
		return NULL;
	
	word_out = (char *)malloc(sizeof(char)*word_len);
	if (!word_out)
		return NULL;
	
	index++;
	while (index < word_len - 1) {
		unsigned char c = word_in[index++];
		switch (c) {
		case '\"':
			next_char = word_in[index++];
			switch (next_char) {
			case '\"':
				word_out[index_out++] = '\"';
				break;
			default:
				break;
			}
			break;
		default:
			word_out[index_out++] = c;
			break;
		}
		
	}
	word_out[index_out] = CSV_TERMINATOR;
	
	assert(word_out);
	return word_out;
}

vector_t *csv_parse_quoted(char *line)
{
	int quoted = 0, i;
	unsigned char *string = line;
	char *matched = line;
	int count;
	vector_t *vector = create_vector((DESTROYER)free,
					 (COMPARER)strcmp);
	
	for (string; *string; string++) {
		if (*string >= 0x80) {
			string++;
			continue;
		}
		if (*string == CSV_QUOTER) {
			if (quoted) {
				char next = *(string+1);
				
				if (next != CSV_QUOTER) {
					*(string+1) = CSV_TERMINATOR;
					quoted = 0;
					append_element(vector, strdup(matched), 0);
				}
				string++;
			} else {
				quoted = 1;
				matched = string;
			}
		}
	}
	count = element_count(vector);
	if (count) {
		for (i = 0; i < count; i++) {
			char *old_word = get_element(vector, i);
			if (old_word) {
				char *word = csv_to_word(old_word);
				set_element(vector, i, word);
			}
		}
	} else {
		destroy_vector(vector);
		vector = NULL;
	}
	
	return vector;
}

int tag_save_list(tag_list_t *list)
{
	int i, j;
	int rows = -1;
	int columns = -1;
	char file[MAX_PATH+1];
	FILE *stream = NULL;
	
	if (list) {
#ifdef CONFIG_PROTECT_WRITE
		sprintf(file, "%s/CGI/%s.tmp", tag_template_root, list->name);
#else
		sprintf(file, "%s/CGI/%s.lst", tag_template_root, list->name);
#endif
		stream = fopen(file, "w");
		if (!stream)
			return -1;
		
		columns = element_count(list->title);
		rows = element_count(list->table);
		
		for (i = 0; i < columns; i++) {
			char *word = get_element(list->title, i);
			if (word) {
				word = csv_to_word(word);
				fprintf(stream, "%s", (char *)word);
			} else
				fprintf(stream, "\"\"");
			if (i < columns-1)
				fprintf(stream, ",");
			else
				fprintf(stream, "\n");
		}
		
		for (i = 0; i < rows; i++) {
			vector_t *line = get_element(list->table, i);
			for (j = 0; j < columns; j++) {
				char *word = get_element(line, j);
				if (word) {
					word = csv_to_word(word);
					fprintf(stream, "%s", (char *)word);
				} else
					fprintf(stream, "\"\"");
				if (i < columns-1)
					fprintf(stream, ",");
				else
					fprintf(stream, "\n");
			}
		}

		fclose(stream);
#ifdef CONFIG_PROTECT_WRITE
		copy_file(list->name, file);
#endif
	}
	return 0;
}

int csv_comment_line(char *line)
{
	char *ch;
	char *sharp = strchr(line, TAG_COMMENT_SIGN);
	if (sharp) {
		for (ch = line; ch < sharp; ch++) {
			if (!strchr("\t \r\n", *ch))
				break;
		}
		return (ch == sharp) ? 1 : 0;
	}
	return 0;
}

tag_list_t *tag_load_list(char *name)
{
	int lineno = -1;
	int length;
	char *buffer = NULL;
	int size = 0;
	FILE *stream = NULL;
	tag_list_t *list = tag_find_list(name);
	char file[MAX_PATH+1];
	vector_t *messages = NULL;

	if (list)
		return list;
	sprintf(file, "%s/CGI/%s.lst", tag_template_root, name);
	stream = fopen(file, "r");
	if (!stream)
		return NULL;
	list = tag_create_list(name);
	if (!list) goto failure;
	
	while (!feof(stream)) {
		length = getline(&buffer, &size, stream);
		if (length > 0) {
			chomp(buffer);
			
			if (strlen(buffer)) {
				/* escape comment */
				if (csv_comment_line(buffer))
					goto next;
				messages = csv_parse_quoted(buffer);
				
				if (lineno == -1)
					list->title = messages;
				else
					set_element(list->table, lineno, messages);
			} else
				continue;
next:
			lineno++;
		}
	}
	
	if (buffer) free(buffer);
	if (stream) fclose(stream);
	if (list) list_insert_head(&list->list, &tag_lists);
	return list;
failure:
	if (buffer) free(buffer);
	if (list) tag_destroy_list(list);
	if (stream) fclose(stream);
	return NULL;
}

int tag_count_items(tag_list_t *list)
{
	if (list)
		return element_count(list->table);
	return 0;
}

char *tag_get_item(tag_list_t *list, char *title, int current)
{
	int found = 0, i;
	int count;
	vector_t *line = NULL;

	line = get_element(list->table, current);
	if (line) {
		count = element_count(line);
		for (i = 0; i < count; i++) {
			char *name = get_element(list->title, i);
			if (name && strcasecmp(name, title) == 0) {
				found = 1;
				break;
			}
		}
		if (found)
			return get_element(line, i);
	}
	return NULL;
}

void tag_set_item(tag_list_t *list, char *title, int current, char *value)
{
	int found = 0, i;
	int count;
	vector_t *line = NULL;

	line = get_element(list->table, current);
	if (line) {
		count = element_count(list->title);
		for (i = 0; i < count; i++) {
			char *name = get_element(list->title, i);
			if (strcasecmp(name, title) == 0) {
				found = 1;
				break;
			}
		}
		if (found)
			set_element(line, i, value ? strdup(value) : NULL);
	}
}

int tag_set_cell(tag_list_t *list, char *title, int index, char *value)
{
	int i;
	int count, found = 0;
	vector_t *line = NULL;

	if (index < 0)
		index = element_count(list->table);
	else
		line = get_element(list->table, index);
	if (!line) {
		if (index != element_count(list->table))
			index = element_count(list->table);
		line = create_vector((DESTROYER)free,
				     (COMPARER)strcmp);
		if (line)
			set_element(list->table, index, line);
	}
	if (line) {
		count = element_count(list->title);
		for (i = 0; i < count; i++) {
			char *name = get_element(list->title, i);
			if (strcasecmp(name, title) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			/* create a new column */
			set_element(list->title, i, strdup(title));
		}
		set_element(line, i, value ? strdup(value) : NULL);
	}
	return index;
}

char *tag_get_index(tag_list_t *list, int index, int current)
{
	int found = 0;
	vector_t *line = NULL;

	line = get_element(list->table, current);
	if (line)
		return get_element(line, index);
	return NULL;
}

void tag_set_index(tag_list_t *list, int index, int current, char *value)
{
	int found = 0;
	vector_t *line = NULL;

	line = get_element(list->table, current);
	if (line) {
		set_element(line, index, value ? strdup(value) : NULL);
		return;
	}
	return;
}

int tag_find_title(tag_list_t *list, char *title)
{
	int col = -1;
	int i, count;
	vector_t *vector = NULL;
	
	if (!list || !title)
		return -1;
	vector = list->title;
	count = element_count(vector);
	for (i = 0; i < count; i++) {
		char *element = get_element(vector, i);
		if (element && strcasecmp(element, title) == 0) {
			col = i;
			break;
		}
	}
	return col;
}

int tag_find_cell(tag_list_t *list, char *title, char *value)
{
	int i, count;
	int col;
	
	if (!list)
		return -1;
	col = tag_find_title(list, title);

	if (col > -1) {
		count = tag_count_items(list);
		for (i = 0; i < count; i++) {
			char *element = tag_get_index(list, col, i);
			if (element && value && strcasecmp(element, value) == 0)
				return i;
		}
	}
	return -1;
}

char *tag_fetch_value(char *name, char *key)
{
	tag_list_t *list;
	int index;

	if (!name || !key) return NULL;
	list = tag_find_list(name);
	if (!list) return NULL;
	index = tag_find_cell(list, "name", key);
	if (index >= 0)
		return tag_get_item(list, "value", index);
	return NULL;
}

char *tag_get_env(char *name)
{
	char *env, *val = NULL;
	if (name) {
		env = strdup(name);
		strupper(env);
		val = tag_fetch_value("session_envs", env);
		free(env);
	}
	return val;
}

char *tag_get_value(char *name)
{
	char *value = NULL;

	value = tag_fetch_value("query_string", name);
	if (!value)
		value = tag_fetch_value("service_config", name);
	return value;
}

void tag_set_value(char *name, char *value)
{
	tag_list_t *list;
	int index;

	list = tag_find_list("query_string");
	if (!list)
		return;
	index = tag_find_cell(list, "name", name);
	if (index < 0) {
		index = tag_count_items(list);
		tag_set_cell(list, "name", index, name);
	}
	tag_set_cell(list, "value", index, value);
}

void tag_print_value(char *name, char *format, ...)
{
	va_list args;
	int i;
#define MAX_BUF		1024
	char buf[MAX_BUF+1];
	int size = MAX_BUF;
	
	if (format) {
		va_start(args, format);
		i = vsnprintf(buf, size, format, args);
		va_end(args);

		tag_set_value(name, buf);
	}
}

void tag_unset_value(char *name)
{
	tag_set_value(name, NULL);
}

tag_list_t *tag_create_list(char *name)
{
	tag_list_t *list = malloc(sizeof (tag_list_t));

	if (list) {
		list->title = create_vector((DESTROYER)free,
					    (COMPARER)strcasecmp);
		if (!list->title) goto failure;
		list->table = create_vector((DESTROYER)destroy_vector,
					    (COMPARER)compare_vector);
		if (!list->table) goto failure;
		if (name) list->name = strdup(name);
		list_init(&list->list);
	}
	return list;

failure:
	tag_destroy_list(list);
	return NULL;
}

void tag_destroy_list(tag_list_t *list)
{
	if (list) {
		if (list->title)
			destroy_vector(list->title);
		if (list->table)
			destroy_vector(list->table);
		if (list->name)
			free(list->name);
		list_delete(&list->list);
	}
}

tag_list_t *tag_find_list(char *name)
{
	list_t *pos, * n;
	tag_list_t *entry;

	list_iterate_forward(pos, n, &tag_lists) {
		entry = list_entry(pos, tag_list_t, list);
		if (strcasecmp(entry->name, name) == 0)
			return entry;
	}
	return NULL;
}

tag_token_t *tag_create_token(int type)
{
	tag_token_t *token = malloc(sizeof (tag_token_t));
	if (token) {
		token->type = type;
		token->state = TAG_STATE_TOKEN;
		token->params = create_vector(NULL, NULL);
		token->nparam = 0;
	}
	return token;
}

void tag_destroy_token(tag_token_t *token)
{
	if (token) {
		if (token->params)
			destroy_vector(token->params);
		free(token);
	}
}

tag_block_t *tag_create_block(int start, int limit, long offset, int match, tag_list_t *list)
{
	tag_block_t *block = malloc(sizeof (tag_block_t));
	if (block) {
		block->start = start;
		block->limit = limit;
		block->list = list;
		block->current = start;
		block->match = match;
		block->offset = offset;
		block->match_stack = create_vector(NULL, NULL);
	}
	return block;
}

void tag_destroy_block(tag_block_t *block)
{
	if (block) {
		if (block->match_stack)
			destroy_vector(block->match_stack);
		free(block);
	}
}

tag_block_t *tag_find_block(char *name)
{
	int count, i;
	tag_block_t *block = tag_current_block;

	if (block && block->list && !strcasecmp(name, block->list->name))
		return block;
	count = element_count(tag_block_stack);
	for (i = 0; i < count; i++) {
		block = get_element(tag_block_stack, i);
		if (block && block->list && !strcasecmp(name, block->list->name))
			return block;
	}
	return NULL;
}

tag_list_t *tag_create_value(char *name)
{
	tag_list_t *list;
	
	list = tag_create_list(name);
	if (!list)
		return NULL;
	if (list) {
		list->title = create_vector((DESTROYER)free,
					    (COMPARER)strcasecmp);
		if (!list->title) {
			tag_destroy_list(list);
			return NULL;
		}
		set_element(list->title, 0, strdup("name"));
		set_element(list->title, 1, strdup("value"));
		list_insert_head(&list->list, &tag_lists);
	}
	return list;
}

int tag_init_cgi()
{
#ifdef CONFIG_GCGI
	int i;
	QueryStringNode *node;
	tag_list_t *list;
	
	if (initCgi() < 0) {
		return CGI_ERROR_SESSION;
	}
	/* TODO: internal object query_string, make this session specific */
	list = tag_create_value("query_string");
	if (!list)
		return CGI_ERROR_MEMORY;
	for (i = 0; cgiQuery->query && i < cgiQuery->queryCount; i++) {
		for (node = cgiQuery->query[i]->beg; node != NULL; node = node->next) {
			tag_set_cell(list, "name", i, node->field);
			tag_set_cell(list, "value", i, node->data);
		}
	}
	/* TODO: internal object session_envs, make this session specific */
	list = tag_create_value("session_envs");
	if (!list)
		return CGI_ERROR_MEMORY;
	for (i = 0; envVars[i]; i++) {
		tag_set_cell(list, "name", i, (void *)envVars[i]);
		if (cgiQuery->env[i] != NULL) {
			tag_set_cell(list, "value", i, cgiQuery->env[i]);
		}
	}
	tag_create_list("reply_header");
#endif
	return CGI_ERROR_SUCCESS;
}

/* tag_list_t mapping for session specific objects */
typedef struct _tag_mapping_t {
	char *name;
	char *file;
	list_t mapping_list;
} tag_mapping_t;

int tag_load_session(int id)
{
	tag_load_list("session_context");
	return CGI_ERROR_SUCCESS;
}

int tag_save_session(int id)
{
	return CGI_ERROR_SUCCESS;
}

int tag_init_service(char *root)
{
	if (root)
		tag_template_root = strdup(root);
	else
		return CGI_ERROR_PARAMETER;

#ifdef WIN32
	/* CGI need O_BINARY for various content-types */
	setmode(fileno(stdout), O_BINARY);
#endif

	list_init(&tag_tokens);
	list_init(&tag_lists);
	/* empty list */
	tag_null_list = tag_create_list("");
	tag_want_linefeed = 0;
	tag_current_state = TAG_STATE_UNKNOWN;
	tag_current_token = tag_create_token(TAG_TOKEN_UNKNOWN);
	tag_current_block = tag_create_block(0, 1, 0, 1, NULL);
	tag_state_stack = create_vector(NULL, NULL);
	tag_token_stack = create_vector(NULL, NULL);
	tag_block_stack = create_vector(NULL, NULL);
	/* TODO: internal object service_config, should be loaded from a server_config file */
	tag_load_list("service_config");
	/* TODO: session specifict context information */
	/*tag_load_session(tag_get_value("session"));*/

#ifdef CONFIG_CGI_DEBUG
	printf("Content-Type: text/html\r\n\r\n");
#endif
	return tag_init_cgi();
}

void tag_service_close()
{
	if (tag_template_root) {
		free(tag_template_root);
		tag_template_root = NULL;
	}
	if (tag_state_stack) {
		destroy_vector(tag_state_stack);
		tag_state_stack = NULL;
	}
	if (tag_token_stack) {
		destroy_vector(tag_token_stack);
		tag_token_stack = NULL;
	}
	if (tag_block_stack) {
		destroy_vector(tag_block_stack);
		tag_block_stack = NULL;
	}
	if (tag_current_token) {
		tag_destroy_token(tag_current_token);
		tag_current_token = NULL;
	}
	if (tag_current_block) {
		tag_destroy_block(tag_current_block);
		tag_current_block = NULL;
	}
}

char *tag_get_location(char *list, char *locate_title, char *locate_value, char *title)
{
	int index;
	tag_list_t *object = tag_load_list(list);
	if (!object)
		return NULL;
	index = tag_find_cell(object, locate_title, locate_value);
	if (index < 0)
		return NULL;
	return tag_get_item(object, title, index);
}

void tag_set_location(char *list, char *locate_title, char *locate_value,
		      char *title, char *value)
{
	int index;
	tag_list_t *object = tag_load_list(list);
	if (!object)
		return;
	index = tag_find_cell(object, locate_title, locate_value);
	if (index < 0)
		return;
	tag_set_cell(object, title, index, value);
}
