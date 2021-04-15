
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

/* miscellaneous functions */
void chomp(char *line)
{
    char *last = line+strlen(line);

    for (last; last >= line; last--)
    {
        if (strchr(" \r\n\t", *last))
            *last = '\0';
        else
            break;
    }
}

char *append_string(char *old_str, const char *str)
{
	int old_size;
	int new_size;
	char *text;
	int size = strlen(str);
	
	if (old_str) {
		old_size = strlen(old_str);
		new_size = old_size + size + 1;
		text = realloc(old_str, new_size);
		memcpy(text + old_size, str, size);
		text[new_size-1] = 0;
	} else {
		text = strdup(str);
	}
	return text;
}

void strupper(char *string)
{
    for (; string && *string; string++)
    {
        if (islower(*string))
            *string = _toupper(*string);
    }
}

char *stristr(const char *string, const char *set)
{
    char *_string = NULL;
    char *_set = NULL;
    char *position;
    char *result = NULL;

    if (!string || !set)
        return NULL;

    _string = strdup(string);
    _set = strdup(set);

    assert(_string && _set);

    strupper(_string);
    strupper(_set);

    position = strstr(_string, _set);
    result = (char *)(position?string+(position-_string):NULL);

    free(_string);
    free(_set);

    return result;
}

#define MIN_CHUNK	128
int create_buffer(char **bufptr, int *n, char **posptr, int *avail)
{
	if (!*bufptr) {
		*n = MIN_CHUNK;
		*bufptr = malloc (*n);
		if (!*bufptr) {
			errno = ENOMEM;
			return -1;
		}
	}
	
	*avail = *n;
	*posptr = *bufptr;
	
	return 0;
}

int read_char(FILE *stream, char **bufptr, int *n, char **posptr, int *avail, char *pchar)
{
	int save_errno;
	
	*pchar = getc(stream);
	
	save_errno = errno;
	
	/*
	 * We always want at least one char left in the buffer, since we
	 * always (unless we get an error while reading the first char)
	 * NUL-terminate the line buffer.
	 */
	if (*avail < 2) {
		if (*n > MIN_CHUNK)
			*n *= 2;
		else
			*n += MIN_CHUNK;
		
		*avail = *n + *bufptr - *posptr;
		*bufptr = realloc (*bufptr, *n);
		if (!*bufptr) {
			errno = ENOMEM;
			return -1;
		}
		*posptr = *n - *avail + *bufptr;
		assert ((*bufptr + *n) == (*posptr + *avail));
	}
	
	if (ferror (stream)) {
		/*
		 * Might like to return partial line, but there is no
		 * place for us to store errno.  And we don't want to just
		 * lose errno.
		 */
		errno = save_errno;
		return -1;
	}
	
	if (*pchar == EOF) {
		/* Return partial line, if any.  */
		if (*posptr == *bufptr)
			return -1;
		else
			return 0;
	}
	
	*(*posptr)++ = *pchar;
	*avail = *avail-1;
	return 1;
}
