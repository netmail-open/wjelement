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


#include <string.h>
#include <stdlib.h>

#include <xpl.h>
#include <nmutil.h>
#include <memmgr.h>

#include <wjwriter.h>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

/*
	JSON syntax (http://www.json.org/)
	==================================
		object
			{}
			{ members }

		members
			pair
			pair , members

		pair
			string : value

		array
			[]
			[ elements ]

		elements
			value
			value , elements

		value
			string
			number
			object
			array
			true
			false
			null

		string
			""
			"chars"

		chars
			char
			char chars

		char
			any-Unicode-character- except-"-or-\-or- control-character
			\"
			\\
			\/
			\b
			\f
			\n
			\r
			\t
			\u four-hex-digits

		number
			int
			int frac
			int exp
			int frac exp

		int
			digit
			digit1-9 digits
			- digit
			- digit1-9 digits

		frac
			. digits

		exp
			e digits
		digits
			digit
			digit digits

		e
			e
			e+
			e-
			E
			E+
			E-

	Complex JSON Document Example
	=============================

	A complex JSON object:

	{
		"string1"	:	"This is a string",
		"bool1"		:	true,
		"bool2"		:	false,
		"object1"	: {
			"string2"	:	"This is another string",
			"array1"	:	[
				"one",
				false,
				92,
				"two"
			],
			"array2"	:	[
				array3		: [ 1, 2, 3 ]
			],
			"array4"	:	[ ]
		},
		"tricky":"foo","ugg":{"1":1,"2":2,"3:"3,"four":[1,2,3,4]}
	}
*/

typedef struct {
	WJWriterPublic		public;

	/*
		If the document, or an object, or an array have just been opened then
		the next value should not have a comma.  All other values must be
		preceded with a comma.
	*/
	XplBool				skipcomma;
	XplBool				skipbreak;
	XplBool				instring;
	int					depth;

	size_t				size;
	size_t				used;
	char				buffer[1];
} WJIWriter;

static int WJWrite(WJIWriter *doc, char *data, size_t length)
{
	size_t		result	= 0;
	size_t		size;
	int			offset;

	if (doc && !doc->size) {
		if (doc->public.write.cb) {
			return(doc->public.write.cb(data, length, doc->public.write.data));
		} else {
			return(0);
		}
	}

	if (!doc || !doc->public.write.cb) {
		return(0);
	}

	DebugAssert(doc->used <= doc->size);
	while (length) {
		/* Fill our buffer as much as possible */
		if (doc->used < doc->size) {
			size = xpl_min(length, doc->size - doc->used);

			memcpy(doc->buffer + doc->used, data, size);
			result		+= size;
			doc->used	+= size;
			data		+= size;
			length		-= size;
		}

		/* Flush the buffer to make room for any data that didn't fit */
		offset = 0;
		while (offset < doc->used &&
			((doc->size - doc->used) + offset) < length
		) {
			size = doc->public.write.cb(doc->buffer + offset, doc->used - offset,
						doc->public.write.data);
			DebugAssert((signed int) size >= 0 && size <= doc->used - offset);
			offset += size;

			if (!size) {
				/* The callback failed */
				doc->public.write.cb = NULL;
				doc->used -= offset;

				DebugAssert(doc->used <= doc->size);
				return(result);
			}
		}
		doc->used -= offset;

		/* Reallign to the begining of the buffer */
		if (doc->used) {
			memmove(doc->buffer, doc->buffer + offset, doc->used);
		}

		/*
			If the data still won't fit in the buffer after flushing as much as
			possible then attempt to send it directly.
		*/
		if (length > (doc->size - doc->used)) {
			size = doc->public.write.cb(data, length, doc->public.write.data);
			DebugAssert((signed int) size >= 0 && size <= length);

			result		+= size;
			data		+= size;
			length		-= size;
		}
	}

	DebugAssert(doc->used <= doc->size);
	doc->buffer[doc->used] = '\0';
	return(result);
}

EXPORT WJWriter _WJWOpenDocument(XplBool pretty, WJWriteCallback callback, void *writedata, size_t buffersize)
{
	WJIWriter	*doc	= NULL;
	size_t		size	= buffersize;

	if (!callback) {
		errno = EINVAL;
		return(NULL);
	}

	if (size < sizeof(WJIWriter)) {
		size = sizeof(WJIWriter);
	}

	doc = MemMallocEx(NULL, size, &size, TRUE, FALSE);
	memset(doc, 0, sizeof(WJIWriter));

	doc->public.write.cb	= callback;
	doc->public.write.data	= writedata;
	if (buffersize != 0) {
		/*
			Use the avaliable size returned by MemMallocEx so that the write
			buffer is at least buffersize, but may be larger.
		*/
		doc->size			= size - sizeof(WJIWriter);
	} else {
		/*
			Specifying a buffer size of 0 disables WJWriter buffering entirely
			which is important when using a callback that writes to a buffered
			interface to prevent double buffering.
		*/
		doc->size			= 0;
	}

	/*
		The first value after opening a document should not be preceded by a
		comma.  skipcomma will be reset after reading that first value.
	*/
	doc->public.pretty				= pretty;
	doc->public.escapeInvalidChars	= TRUE;
	doc->public.base				= 10;
	doc->skipcomma					= TRUE;
	doc->skipbreak					= TRUE;

	return((WJWriter) doc);
}

EXPORT XplBool WJWCloseDocument(WJWriter indoc)
{
	WJIWriter	*doc	= (WJIWriter *)indoc;
	XplBool		result	= FALSE;

	if (doc) {
		if (doc->size) {
			size_t		size;
			size_t		offset;

			DebugAssert(doc->used <= doc->size);

			/* Write any remaining buffered data */
			offset = 0;
			while (doc->public.write.cb && offset < doc->used) {
				size = doc->public.write.cb(doc->buffer + offset, doc->used - offset,
							doc->public.write.data);
				DebugAssert((signed int) size >= 0 && size <= doc->used - offset);
				offset += size;

				if (!size) {
					/* The callback failed */
					doc->public.write.cb = NULL;
					doc->used -= offset;
					break;
				}
			}
			doc->used -= offset;
			DebugAssert(doc->used <= doc->size);
		}

		if (doc->public.user.freecb) {
			doc->public.user.freecb(doc->public.user.data);
		}

		if (doc->public.write.cb) {
			/* If the callback is still set then there where no errors */
			result = TRUE;
		}

		MemFree(doc);
	}

	return(result);
}

/*
	Helper Open Functions

	Open a JSON document using pre-canned callback functions which can read from
	common data sources, such as an open file, or an open connection.
*/
EXPORT size_t WJWFileCallback(char *buffer, size_t length, void *data)
{
	if (data) {
		return((int)fwrite(buffer, sizeof(char), length, (FILE *)data));
	}

	return(0);
}

static size_t WJWMemCallback(char *buffer, size_t length, void *data)
{
	char		**mem = data;
	size_t		l;

	if (mem) {
		if (!*mem) {
			*mem = MemMallocWait(length + 1);
			memcpy(*mem, buffer, length);
			(*mem)[length] = '\0';
		} else {
			l = strlen(*mem);

			*mem = MemReallocWait(*mem, l + length + 1);
			memcpy(*mem + l, buffer, length);
			(*mem)[l + length] = '\0';
		}

		return(length);
	}

	return(0);
}

EXPORT WJWriter WJWOpenMemDocument(XplBool pretty, char **mem)
{
	if (!mem) {
		return(NULL);
	}

	return(_WJWOpenDocument(pretty, WJWMemCallback, mem, 0));
}

/*
	Verify that str points to a valid UTF8 character, and return the length of
	that character in bytes.  If the value is not a full valid UTF8 character
	then -1 will be returned.
*/
static ssize_t WJWUTF8CharSize(char *str, size_t length)
{
	ssize_t			r	= -1;
	unsigned char	test;
	int				i;

	if (!str || !length) {
		return(0);
	}

	test = (unsigned char) *str;

	if (!(test & 0x80)) {
		/* ASCII */
		return(1);
	} else if ((test & 0xC0) == 0x80) {
		/*
			This is not a primary octet of a UTF8 char, but is valid as any
			other octet of the UTf8 char.
		*/
		return(-1);
	} else if ((test & 0xE0) == 0xC0) {
		if (test == 0xC0 || test == 0xC1) {
			/* redundant code point. */
			return(-1);
		}
		r = 2;
	} else if ((test & 0xF0) == 0xE0) {
		r = 3;
	} else if ((test & 0xF8) == 0xF0) {
		if ((test & 0x07) >= 0x05) {
			/* undefined character range. RFC 3629 */
			return(-1);
		}

		r = 4;
	} else {
		/*
			Originally there was room for (more 4,) 5 and 6 byte characters but
			these where outlawed by RFC 3629.
		*/
		return(-1);
	}

	if (r > length) {
		r = length;
	}

	for (i = 1; i < r; i++) {
		if (((unsigned char)(str[i]) & 0xC0) != 0x80) {
			/* This value is not valid as a non-primary UTF8 octet */
			return(-1);
		}
	}

	return(r);
}

static XplBool WJWriteString(char *value, size_t length, XplBool done, WJIWriter *doc)
{
	char		*v;
	char		*e;
	ssize_t		l;
	char		esc[3];

	if (!doc || !doc->public.write.cb) {
		return(FALSE);
	}

	if (!doc->instring) {
		WJWrite(doc, "\"", 1);
	}

	*(esc + 0) = '\\';
	*(esc + 1) = '\0';
	*(esc + 2) = '\0';

	for (v = e = value; e < value + length; e++) {
		switch (*e) {
			case '\\':	*(esc + 1) = '\\';	break;
			case '"':	*(esc + 1) = '"';	break;
			case '\n':	*(esc + 1) = 'n';	break;
			case '\b':	*(esc + 1) = 'b';	break;
			case '\t':	*(esc + 1) = 't';	break;
			case '\v':	*(esc + 1) = 'v';	break;
			case '\f':	*(esc + 1) = 'f';	break;
			case '\r':	*(esc + 1) = 'r';	break;

			default:
				l = WJWUTF8CharSize(e, length - (e - value));
				if (l > 1 || (l == 1 && *e >= '\x20')) {
					/*
						*e is the primary octect of a multi-octet UTF8
						character.  The remaining characters have been verified
						as valid UTF8 and can be skipped.
					*/
					e += (l - 1);
				} else if (l == 1) {
					/*
						*e is valid UTF8 but is not a printable character, and
						will be escaped before being sent, using the
						JSON-standard "\u00xx" form
					*/
					char	unicodeHex[sizeof("\\u0000")];

					WJWrite(doc, v, e - v);

					sprintf(unicodeHex, "\\u00%02x", (unsigned char) *e);
					WJWrite(doc, unicodeHex, sizeof(unicodeHex)-1);

					v = e + 1;
				} else if (l < 0) {
					/*
						*e is not valid UTF8 data, and must be escaped before
						being sent. But JSON-standard does not give us a
						mechanism so we chose "\xhh" format because of its
						almost universal comprehension.
					*/
					char	nonUnicodeHex[sizeof("\\x00")];

					WJWrite(doc, v, e - v);

					if (doc->public.escapeInvalidChars) {
						sprintf(nonUnicodeHex, "\\x%02x", (unsigned char) *e);
						WJWrite(doc, nonUnicodeHex, sizeof(nonUnicodeHex)-1);
					}

					v = e + 1;
				}
				continue;
		}

		WJWrite(doc, v, e - v);
		v = e + 1;

		WJWrite(doc, esc, 2);
		continue;
	}

	WJWrite(doc, v, length - (v - value));

	if (done) {
		WJWrite(doc, "\"", 1);
	}
	doc->instring = !done;
	return(TRUE);
}

EXPORT XplBool WJWOpenArray(char *name, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->public.pretty) {
			int		i;

			if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}

			if (!doc->skipbreak) {
				WJWrite(doc, "\n", 1);
			}
			doc->skipbreak = FALSE;

			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		} else if (!doc->skipcomma) {
			WJWrite(doc, ",", 1);
		}

		doc->depth++;

		if (name) {
			WJWriteString(name, strlen(name), TRUE, doc);
			WJWrite(doc, ":", 1);
		}

		doc->skipcomma = TRUE;
		return(1 == WJWrite(doc, "[", 1));
	}

	return(FALSE);
}

EXPORT XplBool WJWCloseArray(WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->depth > 0) {
			doc->depth--;
		}

		if (doc->public.pretty) {
			int		i;

			WJWrite(doc, "\n", 1);
			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		}

		doc->skipcomma = FALSE;
		return(1 == WJWrite(doc, "]", 1));
	}

	return(FALSE);
}

EXPORT XplBool WJWOpenObject(char *name, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->public.pretty) {
			int		i;

			if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}

			if (!doc->skipbreak) {
				WJWrite(doc, "\n", 1);
			}
			doc->skipbreak = FALSE;
			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		} else if (!doc->skipcomma) {
			WJWrite(doc, ",", 1);
		}

		doc->depth++;

		if (name) {
			WJWriteString(name, strlen(name), TRUE, doc);
			WJWrite(doc, ":", 1);
		}

		doc->skipcomma = TRUE;
		return(1 == WJWrite(doc, "{", 1));
	}

	return(FALSE);
}

EXPORT XplBool WJWCloseObject(WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->depth > 0) {
			doc->depth--;
		}

		if (doc->public.pretty) {
			int		i;

			WJWrite(doc, "\n", 1);
			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		}

		doc->skipcomma = FALSE;
		return(1 == WJWrite(doc, "}", 1));
	}

	return(FALSE);
}

EXPORT XplBool WJWStringN(char *name, char *value, size_t length, XplBool done, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb && value) {
		if (!doc->instring) {
			if (doc->public.pretty) {
				int		i;

				if (!doc->skipcomma) {
					WJWrite(doc, ",", 1);
				}

				WJWrite(doc, "\n", 1);
				for (i = 0; i < doc->depth; i++) {
					WJWrite(doc, "\t", 1);
				}
			} else if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}
			doc->skipcomma = FALSE;

			if (name) {
				WJWriteString(name, strlen(name), TRUE, doc);
				WJWrite(doc, ":", 1);
			}
		}

		return(WJWriteString(value, length, done, doc));
	}

	return(FALSE);
}

EXPORT XplBool WJWString(char *name, char *value, XplBool done, WJWriter doc)
{
	if (value) {
		return(WJWStringN(name, value, strlen(value), done, doc));
	} else {
		return(WJWStringN(name, "", 0, done, doc));
	}
}

static XplBool WJWNumber(char *name, char *value, size_t size, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->public.pretty) {
			int		i;

			if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}

			WJWrite(doc, "\n", 1);
			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		} else if (!doc->skipcomma) {
			WJWrite(doc, ",", 1);
		}

		doc->skipcomma = FALSE;

		if (name) {
			WJWriteString(name, strlen(name), TRUE, doc);
			WJWrite(doc, ":", 1);
		}

		if (size > 0) {
			size -= WJWrite(doc, value, size);
			return(size == 0);
		}
	}

	return(FALSE);
}

EXPORT XplBool WJWInt32(char *name, int32 value, WJWriter doc)
{
	char		v[256];
	size_t		s;

	switch (doc->base) {
		default:
		case 10:
			s = strprintf(v, sizeof(v), NULL, "%ld",		(long) value);
			break;
		case 16:
			s = strprintf(v, sizeof(v), NULL, "0x%08lx",	(long) value);
			break;

		case 8:
			s = strprintf(v, sizeof(v), NULL, "0%lo",		(long) value);
			break;
	}

	return(WJWNumber(name, v, s, doc));
}

EXPORT XplBool WJWUInt32(char *name, uint32 value, WJWriter doc)
{
	char		v[256];
	size_t		s;

	switch (doc->base) {
		default:
		case 10:
			s = strprintf(v, sizeof(v), NULL, "%lu",		(unsigned long) value);
			break;

		case 16:
			s = strprintf(v, sizeof(v), NULL, "0x%08lx",	(unsigned long) value);
			break;

		case 8:
			s = strprintf(v, sizeof(v), NULL, "0%lo",		(unsigned long) value);
			break;
	}

	return(WJWNumber(name, v, s, doc));
}

EXPORT XplBool WJWInt64(char *name, int64 value, WJWriter doc)
{
	char		v[256];
	size_t		s;

	switch (doc->base) {
		default:
		case 10:
			s = strprintf(v, sizeof(v), NULL, "%lld",		(long long) value);
			break;

		case 16:
			s = strprintf(v, sizeof(v), NULL, "0x%016llx",	(long long) value);
			break;

		case 8:
			s = strprintf(v, sizeof(v), NULL, "0%llo",		(long long) value);
			break;
	}

	return(WJWNumber(name, v, s, doc));
}

EXPORT XplBool WJWUInt64(char *name, uint64 value, WJWriter doc)
{
	char		v[256];
	size_t		s;

	switch (doc->base) {
		default:
		case 10:
			s = strprintf(v, sizeof(v), NULL, "%llu",		(unsigned long long) value);
			break;

		case 16:
			s = strprintf(v, sizeof(v), NULL, "0x%016llx",	(unsigned long long) value);
			break;

		case 8:
			s = strprintf(v, sizeof(v), NULL, "0%llo",		(unsigned long long) value);
			break;
	}

	return(WJWNumber(name, v, s, doc));
}

EXPORT XplBool WJWDouble(char *name, double value, WJWriter doc)
{
	char		v[256];
	size_t		s;

	s = strprintf(v, sizeof(v), NULL, "%e", value);
	return(WJWNumber(name, v, s, doc));
}

EXPORT XplBool WJWBoolean(char *name, XplBool value, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->public.pretty) {
			int		i;

			if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}

			WJWrite(doc, "\n", 1);
			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		} else if (!doc->skipcomma) {
			WJWrite(doc, ",", 1);
		}


		doc->skipcomma = FALSE;

		if (name) {
			WJWriteString(name, strlen(name), TRUE, doc);
			WJWrite(doc, ":", 1);
		}

		if (value) {
			WJWrite(doc, "true", 4);
		} else {
			WJWrite(doc, "false", 5);
		}

		return(TRUE);
	}

	return(FALSE);
}

EXPORT XplBool WJWNull(char *name, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (doc->public.pretty) {
			int		i;

			if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}

			WJWrite(doc, "\n", 1);
			for (i = 0; i < doc->depth; i++) {
				WJWrite(doc, "\t", 1);
			}
		} else if (!doc->skipcomma) {
			WJWrite(doc, ",", 1);
		}

		doc->skipcomma = FALSE;

		if (name) {
			WJWriteString(name, strlen(name), TRUE, doc);
			return(6 == WJWrite(doc, ":null", 5));
		} else {
			return(4 == WJWrite(doc, "null", 4));
		}
	}

	return(FALSE);
}

EXPORT XplBool WJWRawValue(char *name, char *value, XplBool done, WJWriter indoc)
{
	WJIWriter	*doc = (WJIWriter *)indoc;

	if (doc && doc->public.write.cb) {
		if (!doc->instring) {
			if (doc->public.pretty) {
				if (!doc->skipcomma) {
					WJWrite(doc, ",", 1);
				}

				WJWrite(doc, "\n", 1);
				/*
					Don't indent, because the raw value will not be indented to
					match.  Its up to the caller to prettify their JSON
				*/
			} else if (!doc->skipcomma) {
				WJWrite(doc, ",", 1);
			}

			doc->skipcomma = FALSE;

			if (name) {
				WJWriteString(name, strlen(name), TRUE, doc);
				WJWrite(doc, ":", 1);
			}
		}

		doc->instring = !done;
		return(strlen(value) == WJWrite(doc, value, strlen(value)));
	}

	return(FALSE);
}

