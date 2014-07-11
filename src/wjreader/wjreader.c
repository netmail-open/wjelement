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


#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <xpl.h>
#include <hulautil.h>
#include <memmgr.h>

#include <wjreader.h>

#if defined(_MSC_VER)
#define strtoull(nptr, endptr, base) _strtoui64(nptr, endptr, base)
#define strtoll(nptr, endptr, base) _strtoi64(nptr, endptr, base)
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
				"array3":	[ 1, 2, 3 ]
			],
			"array4"	:	[ ]
		},
		"tricky":"foo","ugg":{"1":1,"2":2,"3:"3,"four":[1,2,3,4]}
	}
*/

#define HexDigit(c)				((c) <= '9' ? (c) - '0' : (toupper((c)) & 7) + 9)

/*
	As each element is found in the buffer information about it is stored at the
	start of the buffer. This includes a type character and a terminated name.
*/

/*
	As each element is found in the buffer very minimal information about it is
	stored in order to allow walking back up the document structure properly.
	This data is stored within the space at the begining of the buffer.	A single
	character for each element of the structure.	This space will always be
	avaliable because there is always at least one character between elements.
*/

typedef struct {
	WJReaderPublic		pub;

	/* Internal depth, may not match the depth in the public structure		*/
	uint32				depth;

	/* Does the document need to be free when we're done with it?			*/
	XplBool				free;

	/* Points to the type character of the current object.					*/
	char				*current;

	/* Points to the end of a value that needs to be protected.				*/
	char				*protect;

	/* The current position to read from the buffer at.						*/
	char				*read;

	/* The current poisition to write into the buffer at.					*/
	char				*write;

	/* If current is WJR_TYPE_NUMBER then is that number negative?			*/
	XplBool				negative;

	/*
		A place holder character to store what was punched out in order to
		terminate a string value.
	*/
	char				punchout;

	/*
		Callback Information

		When data is needed by the JSON parser the callback is called so that it
		can fill out data in the buffer.  The callback is given a void * that
		can contain anything it can read from, and the number of bytes it has
		returned so far so that it can keep it's position.
	*/
	WJReadCallback		callback;
	int					seen;

	size_t				buffersize;
	char				buffer[];
} WJIReader;

#ifdef DEBUG_ASSERT
#define WJRDocAssert(d)	if ((d)->free) MemAssert((d));
#else
#define WJRDocAssert(d)
#endif



/*
	Fill the buffer as much as possible.  The type list that is currently in use
	will be preserved as well as any data between the read and write pointers.
	The data between the read and write pointers will be moved to directly after
	the name data, and then any remaining space will be read into.
*/
static int WJRFillBuffer(WJIReader *doc)
{
	if (doc) {
		/*
			Move any buffered portion of the document that hasn't been parsed
			yet to the front of the buffer, and adjust the pointers to match.
		*/
		char	*r;
		int		depth;

		if (doc->depth > 0) {
			depth = doc->depth;
		} else {
			depth = 0;
		}

		if (doc->read > doc->write) {
			doc->read = doc->write;
		}

		if (doc->current) {
			r = doc->current + strlen(doc->current) + 1;
		} else {
			r = doc->buffer + doc->pub.maxdepth + 1;
		}

		if (r < doc->protect) {
			r = doc->protect;
		}

		if (r < doc->current + doc->pub.maxdepth - depth + 1) {
			r = doc->current + doc->pub.maxdepth - depth + 1;
		}

		if (r < doc->read) {
			doc->write = doc->read + strlen(doc->read);
			WJRDocAssert(doc);
			memmove(r, doc->read, doc->write - doc->read);
			WJRDocAssert(doc);

			doc->write	-= doc->read - r;
			doc->read	= r;
		} else {
			r = doc->read;
		}

		if (doc->write < doc->buffer + doc->buffersize && r < doc->buffer + doc->buffersize) {
			/* Fill the rest of the buffer */
			if (doc->write < doc->buffer + doc->buffersize) {
				int			c;

				WJRDocAssert(doc);
				if (doc->callback) {
					c = doc->callback(doc->write,
						doc->buffer + doc->buffersize - doc->write,
						doc->seen, doc->pub.userdata);
				} else {
					c = 0;
				}
				doc->seen += c;

				WJRDocAssert(doc);
				doc->write += c;
				*(doc->write) = '\0';
				WJRDocAssert(doc);

				if (c <= 0) {
					doc->callback = NULL;
					return(-1);
				}
				return(c);
			}
		}

		/* There is no room in the buffer */
		return(0);
	}

	return(-1);
}

/*
	Determine what type the next value is, and position the read pointer so that
	the value can be parsed.
*/
static int WJRDown(WJIReader *doc)
{
	WJRDocAssert(doc);
	doc->current += strlen(doc->current) + 1;

	doc->pub.depth++;
	doc->depth++;

	doc->negative = FALSE;

	if (doc->depth >= doc->pub.maxdepth) {
		/* The document has gotten too deep.  Refuse to parse any further. */
		return(-1);
	}

	for (;;) {
		do {
			for (; doc->read < doc->write && isspace(*doc->read); doc->read++);
		} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);

		switch (*doc->read) {
			case '{':
				/* Object.	No Value. */
				doc->current[0] = WJR_TYPE_OBJECT;

				/*
					Leave the read pointer where it is, but change the value to
					a comma, so that the next call to WJRNext() will look for a
					value following it.
				*/
				*doc->read = ',';
				break;

			case '[':
				/*
					Array.

					No Value.  All other elements at this level will have no
					name.
				*/
				doc->current[0] = WJR_TYPE_ARRAY;

				/*
					Leave the read pointer where it is, but change the value to
					a comma, so that the next call to WJRNext() will look for a
					value following it.
				*/
				*doc->read = ',';
				break;

			case '"':
				/*
					String.

					Advance past the quote, and set the type to indicate the
					quote type.
				*/
				doc->current[0] = WJR_TYPE_STRING;

				doc->read++;
				break;

			case '-':
				doc->negative = TRUE;
				doc->read++;

				/* fallthrough */

			case '+':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				/* Number */
				doc->current[0] = WJR_TYPE_NUMBER;
				break;

			case 't':	case 'T':
				/* True */
				doc->current[0] = WJR_TYPE_TRUE;
				break;

			case 'f':	case 'F':
				/* False */
				doc->current[0] = WJR_TYPE_FALSE;
				break;

			case 'n':	case 'N':
				/* Null */
				doc->current[0] = WJR_TYPE_NULL;
				break;

			case '#':
				/* Python style comment. Skip until the end of the line */
				do {
					for (; doc->read < doc->write && *doc->read != '\n'; doc->read++);
				} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);
				continue;

			default:
				/* Unknown */
				doc->current[0] = WJR_TYPE_UNKNOWN;
				break;
		}

		break;
	}

	return(0);
}

EXPORT WJReader _WJROpenDocument(WJReadCallback callback, void *userdata, char *buffer, size_t buffersize, uint32 maxdepth)
{
	WJIReader	*doc	= NULL;

	if (callback) {
		if (!buffer) {
			if (buffersize == 0) {
				/*
					Default to 4k.  Using MemMallocEx will provide the actual
					avaliable size of the allocation, so asking for a little
					less than 4k will result in just shy of 4k of usable buffer.
				*/
				buffersize = (3 * 1024);
			} else if (buffersize < (sizeof(WJIReader) + 512)) {
				/* Enforce a minimum size */
				buffersize = (sizeof(WJIReader) + 512);
			}

			doc = MemMallocEx(NULL, buffersize, &buffersize, TRUE, FALSE);
		} else {
			doc = (WJIReader *)buffer;
		}

		if (doc) {
			memset(doc, 0, sizeof(WJIReader));

			doc->buffersize				= buffersize - sizeof(WJIReader) - 1;
			doc->callback				= callback;
			doc->pub.userdata			= userdata;

			doc->current				= doc->buffer;
			doc->protect				= NULL;

			doc->free					= buffer ? FALSE : TRUE;

			/* Leave one character for the type stack */
			doc->read					= doc->buffer + 1;
			doc->write					= doc->buffer + 1;

			*doc->read					= '\0';

			/*
				Add a psudeo container on the stack that we never return, as a
				place holder.  The type must be ARRAY because the values in the
				document will not have a name.	Preceeding the value with a
				comma will allow finding a value just as if we where already in
				the middle of the document.
			*/
			doc->current[0]				= WJR_TYPE_ARRAY;
			doc->current[1]				= '\0';

			doc->pub.maxdepth			= maxdepth;
			doc->read					= doc->buffer + doc->pub.maxdepth + 1;
			doc->write					= doc->buffer + doc->pub.maxdepth + 2;

			*doc->read					= ',';
			*doc->write					= '\0';

			WJRFillBuffer(doc);
		}
	}

	return((WJReader) doc);
}

EXPORT XplBool WJRCloseDocument(WJReader indoc)
{
	WJIReader	*doc = (WJIReader *)indoc;
	size_t		c;

	if (!doc) {
		return(FALSE);
	}

	/*
		Read the remaining data from the document.	This is very important
		when dealing with a network stream where we will get out of sync
		with the client if the entire document is not read.
	*/
	WJRDocAssert(doc);
	while (doc->callback &&
		(c = doc->callback(doc->buffer, doc->buffersize, doc->seen, doc->pub.userdata)) &&
		c > 0
	) {
		doc->seen += c;
		WJRDocAssert(doc);
	}

	if (doc->free) {
		MemFree(doc);
	}
	return(TRUE);
}

/*
	Helper Open Functions

	Open a JSON document using pre-canned callback functions which can read from
	common data sources, such as an open file, or an open connection.
*/

EXPORT size_t WJRFileCallback(char *buffer, size_t length, size_t seen, void *data)
{
	size_t		r;

	DebugAssert(length);
	DebugAssert(data);
	if (!data) {
		return(0);
	}

	for (;;) {
		r = fread(buffer, sizeof(char), length, (FILE *) data);

		if (seen == 0 && r >= 3 && buffer[0] == (char) 0xEF &&
			buffer[1] == (char) 0xBB && buffer[2] == (char) 0xBF
		) {
			/*
				Ignore the BOM (byte order marker) since it is pointless in
				UTF-8.

				http://unicode.org/faq/utf_bom.html#bom5
			*/
			r -= 3;
			memmove(buffer, buffer + 3, r);

			if (!r) {
				/* If that was all we read then try the read one more time */
				continue;
			}
		}

		break;
	}

	DebugAssert(r || feof((FILE *) data));
	return(r);
}

EXPORT size_t WJRMemCallback(char *buffer, size_t length, size_t seen, void *userdata)
{
	char		*json = (char *) userdata;
	size_t		len;

	if (!json) {
		return(0);
	}

	len = strlen(json);
	if (len <= seen) {
		return(0);
	}
	len -= seen;
	json += seen;

	if (len > length) {
		len = length;
	}

	if (len > 0) {
		memcpy(buffer, json, len);
	}
	return(len);
}

/*
	Move current back up one level

	The format is:
	Otop\0Astuff\0S\0S\0S\0

	Type, name, terminator, repeat

	So to move back skip back one character, which will be a terminator, then
	keep going back until one more terminator is found, then advance one
	character.
*/
static __inline void WJRUp(WJIReader *doc)
{
	if (doc) {
		if(doc->current && doc->current > doc->buffer) {
			for (doc->current -= 2; *doc->current != '\0' && doc->current >= doc->buffer; doc->current--);

			if (*doc->current == '\0') {
				doc->current++;
			}
		}

		doc->pub.depth--;
		doc->depth--;
	}
}

/*
	Behaves like WJRString(), but doesn't bother with moving the data and such,
	because the goal is simply to skip the rest of the string.
*/
static XplBool WJRSkipString(WJReader indoc)
{
	WJIReader		*doc	= (WJIReader *)indoc;
	XplBool			skip	= FALSE;
	signed int		i;

	if (doc && doc->current && *doc->current == WJR_TYPE_STRING) {
		/*
			Starting at the read pointer look for escaped characters, and for
			non-escaped quotes of the correct type.  If an escaped character is
			found, unescape it, and memmove the rest of the data that is in the
			buffer (and update the write pointer to match).  If the write
			pointer is reached without finding the closing quote then the buffer
			will need to be filled.
		*/

		if (*doc->read == '\0') {
			*doc->read = doc->punchout;
			WJRDocAssert(doc);
			doc->punchout = '\0';
			WJRDocAssert(doc);

			while (*doc->read == '\0' && WJRFillBuffer(doc) > 0);
		}

		for (i = 0; ; i++) {
			if (doc->read[i] == '\0') {
				/*
					Since none of this data is needed, we can move
					doc->write to match doc->read, throwing away any data
					that we had already.
				*/
				*doc->read = '\0';
				doc->write = doc->read;

				switch (WJRFillBuffer(doc)) {
					case 0:
						/*
							There was no room to add anything else to the
							buffer. The string is not complete though.
						*/
						*doc->read = '\0';
						return(FALSE);

					case -1:
						/*
							We have reached the end of the document.  The
							string is complete, because there is no more
							data.

							Walk back up the stack one level.
						*/
						*doc->read = '\0';
						if (doc->current > doc->buffer) {
							WJRUp(doc);
						} else {
							doc->current = NULL;
						}

						return(TRUE);

					default:
						/*
							We now have data, but we set doc->write back to
							doc->read, so it needs to be reset the for loop.
						*/
						i = -1;
						continue;
				}
			}

			if (skip) {
				skip = FALSE;
				continue;
			}

			switch (doc->read[i]) {
				default:
					break;

				case '\\':
					/*
						Simply skip any characters that are part of the escape
						sequence.  This is a rather simplistic implementation,
						since a \u or \x will have more characters, but they
						will not matter here.
					*/
					skip = TRUE;
					break;

				case '"':
					/* We have actually reached the end of the string */
					WJRDocAssert(doc);
					doc->read[i] = '\0';
					doc->read += i + 1;
					WJRDocAssert(doc);

					for (; doc->read < doc->write && *doc->read && *doc->read != ':' && *doc->read != ',' && *doc->read != ']' && *doc->read != '}'; doc->read++);

					/*
						This element is complete.  Walk back up the stack one
						level.
					*/
					if (doc->current > doc->buffer) {
						WJRUp(doc);
					} else {
						doc->current = NULL;
					}

					return(TRUE);
			}
		}
	}

	/* There is no string data to return, so we are complete */
	return(TRUE);
}

/*
	Find the parent object, and then advance the current object until a child of
	the provided parent is found.
*/
EXPORT char * WJRNext(char *parent, size_t maxnamelen, WJReader indoc)
{
	WJIReader	*doc	= (WJIReader *)indoc;
	char		*child;

	if (!parent) {
		parent = doc->buffer;
	}

	/* The position that a direct child of parent will be at */
	child = parent + strlen(parent) + 1;

	if (doc) {
		while (doc->current) {
			if (*doc->read == '\0' && doc->punchout != '\0') {
				WJRDocAssert(doc);
				*doc->read = doc->punchout;
				doc->punchout = '\0';
				WJRDocAssert(doc);
			}

			do {
				for (; doc->read < doc->write && isspace(*doc->read); doc->read++);
			} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);

			switch (*doc->current) {
				case WJR_TYPE_STRING: {
					/* Walk past the string. */
					while (!WJRSkipString(indoc));
					break;
				}

				case WJR_TYPE_NUMBER: {
					/* Call WJRDouble() to walk past the number. */
					WJRDouble(indoc);
					break;
				}

				case WJR_TYPE_TRUE:
				case WJR_TYPE_FALSE:
				case WJR_TYPE_NULL: {
					/* Call WJRBoolean() to walk past the bool */
					WJRBoolean(indoc);
					break;
				}

				default:
				case WJR_TYPE_UNKNOWN: {
					/* Parsing failed, so fall out */
					doc->current = NULL;
					break;
				}

				case WJR_TYPE_OBJECT:
				case WJR_TYPE_ARRAY: {
					char		*current = doc->current;

					if (*doc->read == ',') {
						doc->read++;

						/*
							Insert an item on the stack for the next value.

							A comma indicates that a value waiting in the
							buffer.  This may not be the case however because
							the WJRDown() will replace a [ or a { with a comma,
							so that the first item may be parsed in the same
							manner as all other objects.

							If this happens to be an empty object or array, or
							the document has a trailing comma after an item then
							there may be no actual value.

							Insert an item onto the stack to use for the value.
							This may be removed if there isn't actually a value,
							and will be abused if a name needs to be found.
						*/
						if (WJRDown(doc)) {
							/* We've exceeded the maximum document depth */
							doc->current = NULL;
							break;
						}

						/* Terminate the name, assuming there is no name */
						WJRDocAssert(doc);
						doc->current[1] = '\0';
						WJRDocAssert(doc);

						if (*doc->current == WJR_TYPE_STRING &&
							*current == WJR_TYPE_OBJECT
						) {
							/*
								This value is actually the name of the value.
								Read off the string, and store it as a name for
								the real value which is to follow.
							*/
							if (child == doc->current) {
								/*
									Rather than finding a value, a name for the
									value has been found (which is the exact
									same format as a string value), and it
									happens to be at the correct level.

									We want the name to be stored directly after
									doc->current, so we will read the string,
									and then memmove it into place.  It can't be
									read into that spot directly because
									doc->read can not get within
									doc->pub.maxdepth chars of doc->buffer.
								*/
								char		*name;
								XplBool		complete	= FALSE;
								size_t		length		= 0;

								if ((name = WJRStringEx(&complete, &length, indoc))) {
									char		*to;

									/*
										Cut off any portion of the string that
										isn't needed
									*/
									name[min(maxnamelen, length)] = '\0';

									/*
										Determine where the name needs to go.
										If the name value was read completely
										then the placeholder value will have
										been removed from the stack.  If it
										wasn't read completely then it hasn't
										been removed, and can't yet because the
										buffer space is still in use by the name
									*/
									to = doc->current + 1;
									if (complete) {
										to += strlen(doc->current) + 1;
									}

									/*
										Move the name to it's final location, so
										that the buffer is free'd up.
									*/
									WJRDocAssert(doc);
									memmove(to, name, length + 1);
									WJRDocAssert(doc);

									/* Mark the end of the name as protected */
									doc->protect = to + strlen(to) + 1;
								}
							}

							/*
								Rather than finding a value, we have found a
								name (which is just a string value) but either
								we are not at the right level, or there is no
								buffer to copy it into, so just skip past the
								string.
							*/
							while (!WJRSkipString(indoc));

							while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0) {
								for (; doc->read < doc->write && isspace(*doc->read); doc->read++);
							}

							if (*doc->read == ':') {
								doc->read++;
								/*
									Now we've found a colon, so the real value
									is up next.	The call to WJRString() that was
									used to find the name above has also removed
									an item from the type stack. We now need to
									increment it again, and find the real value.
								*/
								if (WJRDown(doc)) {
									/* We've exceeded the maximum document depth */
									doc->current = NULL;
									return(NULL);
								}

								/* Turn off protection, if it was on */
								doc->protect = NULL;

								if (child == doc->current) {
									return(doc->current);
								}
							}
						} else if (*doc->current == WJR_TYPE_UNKNOWN) {
							/*
								A value couldn't be found.  Remove the stack
								entry that was created.
							*/
							if (doc->current > doc->buffer) {
								WJRUp(doc);
							} else {
								doc->current = NULL;
							}
						} else if (*current == WJR_TYPE_ARRAY) {
							if (child == doc->current) {
								return(doc->current);
							}
						}
					}

					if ((*doc->current == WJR_TYPE_ARRAY || *doc->current == WJR_TYPE_OBJECT) && *doc->read != ',') {
						/* This is either a ']', '}' or an invalid char */
						if (doc->current > doc->buffer) {
							switch (*doc->read) {
								case ']': case '}':
									break;

								default:
									/*
										Make sure that the consumer knows that the
										depth was wrong due to a badly formated JSON
										document.
									*/
									doc->pub.depth++;
									break;
							}
						}

						if (doc->read < doc->write) {
							doc->read++;
						}

						/* This array or object is now closed */
						if (doc->current > doc->buffer) {
							WJRUp(doc);
						} else {
							doc->current = NULL;
						}
					}

					if (doc->current < doc->buffer && !doc->depth) {
						/*
							Look for any non-whitespace data left in the buffer,
							and set doc->pub.depth to a negative value if any
							exists so that the consumer knows that the document
							had trailing garbage.
						*/
						do {
							for (; doc->read < doc->write && isspace(*doc->read); doc->read++);
						} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);

						if (*doc->read && doc->read < doc->write) {
							doc->pub.depth = -1;
						}
					}

					if (doc->current < parent) {
						/*
							We are back to the parent object, or have past it.
							This implies that we are done with it's children.
						*/

						return(NULL);
					}

					break;
				}
			}
		}
	}

	return(NULL);
}

EXPORT char * WJRStringEx(XplBool *complete, size_t *length, WJReader indoc)
{
	WJIReader	  *doc = (WJIReader *)indoc;

	if (doc && doc->current && *doc->current == WJR_TYPE_STRING) {
		/*
			Starting at the read pointer look for escaped characters, and for
			non-escaped quotes of the correct type.  If an escaped character is
			found, unescape it, and memmove the rest of the data that is in the
			buffer (and update the write pointer to match).  If the write
			pointer is reached without finding the closing quote then the buffer
			will need to be filled.
		*/
		unsigned int	i;
		unsigned int	utf8CharLen;

		if (*doc->read == '\0') {
			WJRDocAssert(doc);
			*doc->read = doc->punchout;
			doc->punchout = '\0';
			WJRDocAssert(doc);

			while (*doc->read == '\0' && WJRFillBuffer(doc) > 0);
		}

		for (i = 0,utf8CharLen = 1; ; i += utf8CharLen, utf8CharLen = 1) {
			switch (doc->read[i]) {
				default: {
					break;
				}

				case '\0': {
					switch (WJRFillBuffer(doc)) {
						case 0: {
							/*
								We have filled the buffer.  Return what we have
								and leave the state as it is so it will be
								called again
							*/
							char	*result = doc->read;

							doc->read += i;
							if (complete) {
								*complete = FALSE;
							}

							if (length) {
								*length = i;
							}

							return(result);
						}

						case -1: {
							/*
								We have reached the end of the document.  Return
								what we have and walk up the stack one level.
							*/
							char		*result = doc->read;

							WJRDocAssert(doc);
							doc->read[i] = '\0';
							WJRDocAssert(doc);
							doc->read += i + 1;

							if (doc->current > doc->buffer) {
								WJRUp(doc);
							} else {
								doc->current = NULL;
							}

							if (complete) {
								*complete = TRUE;
							}

							if (length) {
								*length = i;
							}

							return(result);
						}

						default: {
							/*
								We now have data there, so back up one character
								and work on it
							*/
							i--;
							break;
						}
					}

					break;
				}

				case '\\': {
					unsigned int	move = 0;

					if (strlen(doc->read + i + 1) < (toupper(doc->read[i + 1]) == 'U' ? 5 : (toupper(doc->read[i + 1]) == 'X' ? 3 : 1))) {
						/* Make sure we have enough characters */
						WJRFillBuffer(doc);
						if (strlen(doc->read + i + 1) < (toupper(doc->read[i + 1]) == 'U' ? 5 : (toupper(doc->read[i + 1]) == 'X' ? 3 : 1))) {
							/*
								Even after filling the buffer there aren't
								enough characters.  This means that this is the
								end of the document, or the buffer is full.

								Either way punch out the slash, and return what
								we have.
							*/
							char		*result = doc->read;

							if (-1 != WJRFillBuffer(doc)) {
								WJRDocAssert(doc);
								doc->punchout = '\\';
								doc->read[i] = '\0';
								WJRDocAssert(doc);
							}

							doc->read += i;

							if (complete) {
								*complete = FALSE;
							}

							if (length) {
								*length = i;
							}

							if (!doc->callback) {
								/* We've reached the end of the document */
								if (doc->current > doc->buffer) {
									WJRUp(doc);
								} else {
									doc->current = NULL;
								}
							}

							return(result);
						}
					}

					switch (toupper(doc->read[i + 1])) {
						default:
						case '"':	case '\\':	case '/': {
							/* Simply move the character over */
							move = 1;
							break;
						}

						/* Replace with the escape character */
						case 'B':	doc->read[i + 1] = '\b'; move = 1; break;
						case 'F':	doc->read[i + 1] = '\f'; move = 1; break;
						case 'N':	doc->read[i + 1] = '\n'; move = 1; break;
						case 'R':	doc->read[i + 1] = '\r'; move = 1; break;
						case 'T':	doc->read[i + 1] = '\t'; move = 1; break;

						case 'U': {
							/* Replace four hex digits with a utf-8 encoding */
							unsigned int	c = (HexDigit(doc->read[i + 2]) << 12)	+
												(HexDigit(doc->read[i + 3]) << 8)	+
												(HexDigit(doc->read[i + 4]) << 4)	+
												(HexDigit(doc->read[i + 5]));
							// We right-justify the bytes we output on top of the characters
							// read so memmove works correctly
							if (c < 0x80) {
								/* The 4 hex digits took 1 space, move 5 */
								doc->read[i + 5]	= (char) c;
								// utf8CharLen = 1; /* one byte is all we used */
							} else if (c < 0x800) {
								/* The 4 hex digits took 2 spaces, move 4 */
								doc->read[i + 4]	= (char) 0xC0 | (c >> 6);
								doc->read[i + 5]	= (char) 0x80 | (c & 0x3F);
								utf8CharLen = 2; /* we used two bytes */
							} else if ((c >= 0xD800) && (c <= 0xDBFF)) {
								/* high surrogate codepoint */
								utf8CharLen = 0;
								/* must be followed by low surrogate */
								if((doc->read[i + 6] == '\\') && (toupper(doc->read[i + 7] == 'U'))) {
									unsigned int	d = (HexDigit(doc->read[i + 8]) << 12)	+
														(HexDigit(doc->read[i + 9]) << 8)	+
														(HexDigit(doc->read[i + 10]) << 4)	+
														(HexDigit(doc->read[i + 11]));
									if ((d >= 0xDC00) && (d <= 0xDFFF)) {
										unsigned int	e = 0x10000 + (((c & 0x3ff) << 10) | (d & 0x3ff));
										/* surrogate pair */
										utf8CharLen = 4; /* we used 4 bytes */
										doc->read[i+3] = (char) 0xF0 | (e >> 18);
										doc->read[i+4] = (char) 0x80 | ((e >> 12) & 0x3F);
										doc->read[i+5] = (char) 0x80 | ((e >> 6) & 0x3F);
										doc->read[i+6] = (char) 0x80 | (e & 0x3F);

										move = sizeof("\\u0000\\u0000")-1 - utf8CharLen;
										break; /* don't use the normal exit from this case */
									}
								}
							} else if ((c >= 0xDC00) && (c <= 0xDFFF)) {
								/* low surrogate codepoint is invalid here */
								utf8CharLen = 0;
							} else {
								/* The 4 hex digits took 3 spaces, move 3 */
								doc->read[i + 3]	= (char) 0xE0 | (c >> 12);
								doc->read[i + 4]	= (char) 0x80 | ((c >> 6) & 0x3F);
								doc->read[i + 5]	= (char) 0x80 | (c & 0x3F);
								utf8CharLen = 3; /* we used three bytes */
							}
							move = sizeof("\\u0000")-1 - utf8CharLen;
							break;
						}
						case 'X': {
							/* Replace two hex digits with a non-utf-8 byte */
							unsigned int	c = (HexDigit(doc->read[i + 2]) << 4)	+
												(HexDigit(doc->read[i + 3]));

							/*
								We right-justify the bytes we output on top of
								the characters read so memmove works correctly.
							*/

							/* The 2 hex digits took 1 space, move 3 */
							doc->read[i+3] = (char) c;
							move = sizeof("\\x00") - 1 - utf8CharLen;
							break;
						}
					}

					WJRDocAssert(doc);
					memmove(
						doc->read + i,			/* to	*/
						doc->read + i + move,	/* from	*/

						/* Length, including terminator and at least 1 char */
						strlen(doc->read + i + move + 1) + 2);
					WJRDocAssert(doc);
					break;
				}

				case '"': {
					char		*result = doc->read;

					WJRDocAssert(doc);
					doc->read[i] = '\0';
					WJRDocAssert(doc);
					doc->read += i + 1;

					for (; doc->read < doc->write && *doc->read && *doc->read != ':' && *doc->read != ',' && *doc->read != ']' && *doc->read != '}'; doc->read++);

					/*
						This element is complete.  Walk back up the stack one
						level.
					*/
					if (doc->current > doc->buffer) {
						WJRUp(doc);
					} else {
						doc->current = NULL;
					}

					if (complete) {
						*complete = TRUE;
					}

					if (length) {
						*length = i;
					}

					return(result);
				}
			}
		}
	}

	if (complete) {
		/* There is no string data to return, so it is complete */
		*complete = TRUE;
	}

	return(NULL);
}

typedef enum
{
	WJR_TYPE_INT32 = 0,
	WJR_TYPE_UINT32,
	WJR_TYPE_INT64,
	WJR_TYPE_UINT64,
	WJR_TYPE_DOUBLE,

	WJR_TYPE_MIXED
} WJRNumberType;

typedef struct
{
	XplBool		hasDecimalPoint;

	uint64		i;
	double		d;
} WJRMixedNumber;

static void WJRNumber(void *value, WJRNumberType type, WJReader indoc)
{
	WJIReader	*doc	= (WJIReader *)indoc;

	/* WJRDown() has already advanced past any leading whitespace */
	if (doc && doc->current && *doc->current == WJR_TYPE_NUMBER) {
		char			*end;

		switch (type) {
			default:
			case WJR_TYPE_INT32:
				*((int32 *) value) = strtol(doc->read, &end, 0);
				break;
			case WJR_TYPE_UINT32:
				*((uint32 *) value) = strtoul(doc->read, &end, 0);
				break;
			case WJR_TYPE_INT64:
				*((int64 *) value) = strtoll(doc->read, &end, 0);
				break;
			case WJR_TYPE_UINT64:
				*((uint64 *) value) = strtoull(doc->read, &end, 0);
				break;
			case WJR_TYPE_DOUBLE:
				*((double *) value) = strtod(doc->read, &end);
				break;
			case WJR_TYPE_MIXED:
				((WJRMixedNumber *) value)->hasDecimalPoint = FALSE;
				((WJRMixedNumber *) value)->i = strtoul(doc->read, &end, 0);

				if (end && '.' == *end) {
					/* A decimal point was found, parse again as a double */
					((WJRMixedNumber *) value)->hasDecimalPoint = TRUE;
					((WJRMixedNumber *) value)->d = strtod(doc->read, &end);
				}
				break;
		}

		if ((char *) end <= doc->read || !end || !*end ||
			(*end != ',' && !isspace(*end))
		) {
			/*
				It is likely that we didn't get the entire value.  A valid
				number in a JSON document must be followed by whitespace or a
				comma.  Anything else indicates that there is a portion of the
				number that wasn't parsed and the buffer needs to be filled.
			*/
			WJRFillBuffer(doc);
			switch (type) {
				default:
				case WJR_TYPE_INT32:
					*((int32 *) value) = strtol(doc->read, &end, 0);
					break;
				case WJR_TYPE_UINT32:
					*((uint32 *) value) = strtoul(doc->read, &end, 0);
					break;
				case WJR_TYPE_INT64:
					*((int64 *) value) = strtoll(doc->read, &end, 0);
					break;
				case WJR_TYPE_UINT64:
					*((uint64 *) value) = strtoull(doc->read, &end, 0);
					break;
				case WJR_TYPE_DOUBLE:
					*((double *) value) = strtod(doc->read, &end);
					break;
				case WJR_TYPE_MIXED:
					((WJRMixedNumber *) value)->hasDecimalPoint = FALSE;
					((WJRMixedNumber *) value)->i = strtoul(doc->read, &end, 0);

					if (end && '.' == *end) {
						/* A decimal point was found, parse again as a double */
						((WJRMixedNumber *) value)->hasDecimalPoint = TRUE;
						((WJRMixedNumber *) value)->d = strtod(doc->read, &end);
					}
					break;
			}
		}

		if (end) {
			doc->read = end;
		}

		/* Skip past any whitespace */
		do {
			for (; doc->read < doc->write && isspace(*doc->read); doc->read++);
		} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);
	}

	/* This element is complete.  Walk back up the stack one level. */
	if (doc->current > doc->buffer) {
		WJRUp(doc);
	} else {
		doc->current = NULL;
	}
}

EXPORT XplBool WJRNegative(WJReader indoc)
{
	WJIReader	*doc	= (WJIReader *)indoc;

	return(doc->negative);
}

EXPORT int32 WJRInt32(WJReader doc)
{
	int32	result	= 0;

	WJRNumber(&result, WJR_TYPE_INT32, doc);
	return(result);
}

EXPORT uint32 WJRUInt32(WJReader doc)
{
	uint32	result	= 0;

	WJRNumber(&result, WJR_TYPE_UINT32, doc);
	return(result);
}

EXPORT int64 WJRInt64(WJReader doc)
{
	int64	result	= 0;

	WJRNumber(&result, WJR_TYPE_INT64, doc);
	return(result);
}

EXPORT uint64 WJRUInt64(WJReader doc)
{
	uint64	result	= 0;

	WJRNumber(&result, WJR_TYPE_UINT64, doc);
	return(result);
}

EXPORT double WJRDouble(WJReader doc)
{
	double	result	= 0;

	WJRNumber(&result, WJR_TYPE_DOUBLE, doc);
	return(result);
}

/*
	Parse the number in the JSON document as either a UInt64 or a double,
	depending on the existance of a decimal point.

	Return TRUE if there was a decimal point, and fill out *d. If there is no
	decimal point then return FALSE and fill out *i.
*/
EXPORT XplBool WJRIntOrDouble(WJReader doc, uint64 *i, double *d)
{
	WJRMixedNumber result;

	memset(&result, 0, sizeof(result));
	WJRNumber(&result, WJR_TYPE_MIXED, doc);

	if (result.hasDecimalPoint) {
		if (i) *i = (uint64) result.d;
		if (d) *d = result.d;

		return(TRUE);
	} else {
		if (i) *i = result.i;
		if (d) *d = (double) result.i;

		return(FALSE);
	}
}

EXPORT XplBool WJRBoolean(WJReader indoc)
{
	WJIReader	*doc	= (WJIReader *)indoc;
	XplBool		result	= FALSE;

	/* WJRDown() has already advanced past any leading whitespace */
	if (doc && doc->current && (*doc->current == WJR_TYPE_TRUE || *doc->current == WJR_TYPE_FALSE || *doc->current == WJR_TYPE_NULL)) {
		result = (*doc->current == WJR_TYPE_TRUE || (*doc->current != WJR_TYPE_FALSE && *doc->read != '0' && !isspace(*doc->read)));

		/*
			Skip past any alpha numeric characters, which are likely part of the
			value.
		*/
		do {
			for (; doc->read < doc->write && isalnum(*doc->read); doc->read++);
		} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);

		/* Skip past any whitespace */
		do {
			for (; doc->read < doc->write && isspace(*doc->read); doc->read++);
		} while ((doc->read >= doc->write || *doc->read == '\0') && WJRFillBuffer(doc) > 0);
	}

	/* This element is complete.  Walk back up the stack one level. */
	if (doc->current > doc->buffer) {
		WJRUp(doc);
	} else {
		doc->current = NULL;
	}
	return(result);
}

