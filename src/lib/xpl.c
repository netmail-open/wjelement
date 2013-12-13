/*
    This file is part of WJElement.

    WJElement is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation.

    WJElement is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with WJElement.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <xpl.h>
#include <ctype.h>
#include <string.h>

/* hack for aix, windows, and other asprintf-less systems */
#include <stdio.h>
#include <stdlib.h>
#include "memmgr.h"
#ifndef asprintf
int asprintf(char **ret, const char *format, ...) {
	va_list ap;
	int count;
	*ret = NULL;
	va_start(ap, format);
	count = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	if(count >= 0) {
		char* buffer = MemMalloc(count + 1);
		if(buffer == NULL) {
			return -1;
		}
		va_start(ap, format);
		count = vsnprintf(buffer, count + 1, format, ap);
		va_end(ap);

		if(count < 0) {
			MemFree(buffer);
		} else {
			*ret = buffer;
		}
	}
	return count;
}
#endif

EXPORT size_t vstrcatf( char *buffer, size_t bufferSize, size_t *sizeNeeded, const char *format, va_list args )
{
	int		needed;				/* vsnprintf is prototyped as returning int */
	char	*start = NULL;		/* where to write if non-NULL */
	size_t	used = 0;			/* space consumed in buffer */
	size_t	space = 0;			/* space free in buffer */
	size_t	ret = 0;

								/* have buffer with a NUL inside it? */
	if(buffer && bufferSize && (start = memchr(buffer, '\0', bufferSize))) {
		used = start - buffer;	/* yes, number of used bytes */
								/* available space in buffer */
		space = bufferSize - used;
	}
#ifdef _MSC_VER
	needed = _vsnprintf(start, space, format, args);
	if (-1==needed) {
		needed = _vscprintf(format, args);
	}
#else
	needed = vsnprintf(start, space, format, args);
#endif
	if(needed < 0) {
		DebugAssert(0);			// output error from vsnprintf
	}
	else if(start) {			/* have place to write? */
		if(needed >= space) {
			DebugAssert(sizeNeeded);// buffer overflow and size return not provided
			*start = '\0';		/* tie off buffer as if vsnprintf never happened */
		}
		else {
			ret = needed;		/* success, return bytes written */
		}
	}
	else if(buffer) {			/* no place to write, did we have a buffer? */
		DebugAssert(0);			// no terminating NUL found in buffer
		// This may be a bug, as it will delete a byte and will return a size
		// count that is short by 1.  However, we already overflowed the buffer
		// if we got here, so there isn't much hope of goodness anyway.  If it
		// happens we need to fix the underlying bug, not try to recover here.
		used = bufferSize - 1;	/* make space for one byte */
		buffer[used] = '\0';	/* put a NUL there */
	}
	if(sizeNeeded) {			/* need to return size? */
		*sizeNeeded = used + needed;
	}
	return ret;
}

EXPORT size_t strprintf( char *buffer, size_t bufferSize, size_t *sizeNeeded, const char *format, ... )
{
	size_t	needed;
	va_list	args;

	if( buffer && bufferSize )
	{
		*buffer = '\0';
	}
	va_start( args, format );
	needed = vstrcatf( buffer, bufferSize, sizeNeeded, format, args );
	va_end( args );
	return needed;
}

EXPORT char * strichr(char *str, char c)
{
    c = toupper(c);

    if (str) {
        char *ptr;

        for (ptr = str; *ptr != '\0'; ptr++) {
            if (c == toupper(*ptr)) {
                return(ptr);
            }
        }
    }

    return(NULL);
}

EXPORT char * stristrn(char *haystack, char *needle, size_t len)
{
    if (haystack && needle && *haystack && *needle) {
        char   *ptr;

        for (ptr = strichr(haystack, *needle); ptr; ptr = strichr(ptr + 1, *needle)) {
            if (!strnicmp(ptr, needle, len)) {
                return(ptr);
            }
        }
    }

    return(NULL);
}

static int stripat_r(char *str, char *pattern, size_t len, int depth)
{
	char	*next;
	char	*match;
	char	*end = pattern + len;

	if (!str || !pattern || depth > 15) {
		return(-1);
	}

	for (;;) {
		if (pattern >= end || !*pattern) {
			if (!*str) {
				return(0);
			} else {
				return(-1);
			}
		}

		switch (*pattern) {
			case '?':
				/* A ? must match a single character */
				if (*str) {
					str++;
					pattern++;
				} else {
					return(-1);
				}

			case '*':
				/*
					A * may match 0 or more characters.

					Find the next wild card in the pattern, and then search for
					the exact value between the two wild cards in the string.

					Example, if pattern points to "*abc*" then "abc" must be
					found in the string.
				*/
				pattern++;

				for (next = pattern; *next && next < end && *next != '*' && *next != '?'; next++);
				if (next > pattern) {
					match = str - 1;
					while ((match = stristrn(match + 1, pattern, next - pattern))) {
						if (!stripat_r(match + (next - pattern), next, end - next, depth + 1)) {
							return(0);
						}
					}

					return(-1);
				} else if (pattern >= end || !*pattern) {
					/* The last character of pattern is a *, so we match. */
					return(0);
				}

				break;

			default:
				if (isspace(*pattern)) {
					/* Any whitespace matches any whitespace */
					if (isspace(*str)) {
						for (str++; isspace(*str); str++);
						for (pattern++; *pattern && pattern < end && isspace(*pattern); pattern++);
					} else {
						return(-1);
					}
				} else if (toupper(*pattern) == toupper(*str)) {
					pattern++;
					str++;
				} else {
					return(-1);
				}

				break;
		}
	}
}

EXPORT int stripatn(char *str, char *pattern, size_t len)
{
	if (!pattern) return(-1);

	return(stripat_r(str, pattern, len, 1));
}

EXPORT int stripat(char *str, char *pattern)
{
	if (!pattern) return(-1);

	return(stripat_r(str, pattern, strlen(pattern), 1));
}

EXPORT char *chopspace( char *value )
{
    char    *p;

    if( value )
    {
        for(p=value+strlen(value);p>value;p--)
        {
            if( !isspace( *(p-1) ) )
            {
                break;
            }
        }
        *p = '\0';
    }
    return( skipspace( value ) );
}

EXPORT char * _skipspace( char *source, const char *breakchars )
{
	if( source )
	{
		while( *source && isspace( *source ) )
		{
#if 0
			DebugAssert(*source != '\r');
			DebugAssert(*source != '\n');
#endif
			if (breakchars && strchr(breakchars, *source)) {
				break;
			}

			source++;
		}
	}
	return source;
}

EXPORT char * strspace( char *source )
{
	while( source && *source )
	{
		if( isspace( *source ) )
		{
			return source;
		}
		source++;
	}
	return NULL;
}

#if defined(_WIN32)
EXPORT char * strndup( char *p, size_t maxlen )
{
	if( p )
	{
		char *r = malloc( maxlen + 1 );
		if( !r )
		{
			return r;
		}
		strncpy( r, p, maxlen );
		r[maxlen] = '\0';
		return r;
	}

	return NULL;
}
#endif

EXPORT void * MemMallocEx(void *ptr, size_t size, size_t *actual, XplBool wait, XplBool zero)
{
	void	*result;

	do {
		if (ptr) {
			result = realloc(ptr, size);

			/*
				This implementation doesn't currently have any way to know how
				big the existing allocation is, and so it can't zero the new
				data without writing over the old data.
			*/
			zero = FALSE;
		} else {
			result = malloc(size);
		}

		if (result) {
			if (actual) {
				/*
					In this implementation we aren't actually pulling from a
					pool so the allocation size will be exactly what the
					consumer asked for.
				*/
				*actual = size;
			}

			if (zero) {
				memset(result, 0, size);
			}
		}
	} while (wait && !result);

	return(result);
}

