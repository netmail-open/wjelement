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

#include "element.h"
#include <time.h>

void WJEChanged(WJElement element)
{
	for (; element; element = element->parent) {
		element->changes++;
	}
}

_WJElement * _WJENew(_WJElement *parent, char *name, size_t len, const char *file, int line)
{
	_WJElement	*result;
	WJElement	prev;

	if (parent) {
		_WJEChanged((WJElement) parent);

		if (WJR_TYPE_ARRAY == parent->pub.type) {
			/* Children of an array can not have a name */
			name	= NULL;
			len		= 0;
		} else if (WJR_TYPE_OBJECT != parent->pub.type || !name || !*name) {
			/*
				Only arrays and objects can contain children, and elements in an
				object MUST be named.
			*/
			return(NULL);
		}
	}

	if ((result = MemMalloc(sizeof(_WJElement) + len + 1))) {
		memset(result, 0, sizeof(_WJElement));

		MemUpdateOwner(result, file, line);

		if (name) {
			strncpy(result->_name, name, len);
			result->pub.name = result->_name;
		}
		result->_name[len] = '\0';

		if (parent) {
			result->pub.parent = (WJElement) parent;

			if (!parent->pub.child) {
				parent->pub.child = (WJElement) result;
			} else {
				prev = parent->pub.last;
				prev->next = (WJElement) result;
				result->pub.prev = prev;
			}

			parent->pub.last = (WJElement) result;
			parent->pub.count++;
		}

		result->pub.type = WJR_TYPE_OBJECT;
	}

	return(result);
}

_WJElement * _WJEReset(_WJElement *e, WJRType type)
{
	WJElement	child;

	if (!e) return(NULL);

	while ((child = e->pub.child)) {
		WJEDetach(child);
		WJECloseDocument(child);
	}

	if (WJR_TYPE_STRING == e->pub.type && e->value.string) {
		MemRelease(&(e->value.string));
	}
	e->value.string	= NULL;
	e->pub.length	= 0;
	e->pub.type		= type;

	return(e);
}

EXPORT XplBool _WJEDetach(WJElement document, const char *file, const int line)
{
	if (!document) {
		return(FALSE);
	}

	MemUpdateOwner(document, file, line);

	/* Remove references to the document */
	if (document->parent) {
		WJEChanged(document->parent);

		if (document->parent->child == document) {
			document->parent->child = document->next;
		}
		if (document->parent->last == document) {
			document->parent->last = document->prev;
		}
		document->parent->count--;
		document->parent = NULL;
	}

	if (document->prev) {
		document->prev->next = document->next;
	}

	if (document->next) {
		document->next->prev = document->prev;
	}

	document->prev = NULL;
	document->next = NULL;

	return(TRUE);
}

EXPORT XplBool WJEAttach(WJElement container, WJElement document)
{
	WJElement	prev;

	if (!document || !container) {
		return(FALSE);
	}

	if (document->parent == container) {
		return(TRUE);
	}

	if (document->name) {
		while ((prev = WJEChild(container, document->name, WJE_GET))) {
			WJEDetach(prev);
			WJECloseDocument(prev);
		}
	}

	WJEDetach(document);

	/* Insert it into the new container */
	document->parent = container;
	if (!container->child) {
		container->child = document;
	} else {
		prev = container->last;
		prev->next = document;
		document->prev = prev;
	}
	container->last = document;
	container->count++;
	WJEChanged(container);

	return(TRUE);
}

EXPORT XplBool WJERename(WJElement document, const char *name)
{
	_WJElement	*current = (_WJElement *) document;
	WJElement	e;

	if (!document) {
		return(FALSE);
	}

	/* Look for any siblings with that name, and fail if found */
	if (name && document->parent) {
		for (e = document->parent->child; e; e = e->next) {
			if (e != document && !stricmp(e->name, name)) {
				return(FALSE);
			}
		}
	}

	/* Free the previous name if needed */
	if (document->name && current->_name != document->name) {
		MemRelease(&document->name);
	}

	/* Set the new name */
	if (name) {
		if (!(document->name = MemStrdup(name))) {
			return(FALSE);
		}
	} else {
		document->name = NULL;
	}

	return(TRUE);
}

static WJElement _WJELoad(_WJElement *parent, WJReader reader, char *where, WJELoadCB loadcb, void *data, const char *file, const int line)
{
	char		*current, *name, *value;
	_WJElement	*l = NULL;
	XplBool		complete;
	size_t		actual, used, len;

	if (!reader) {
		return((WJElement) _WJENew(NULL, NULL, 0, file, line));
	}

	if (!where) {
		/*
			A NULL poisition in a WJReader indicates the root of the document,
			so we must read to find the first real object.
		*/
		where = WJRNext(NULL, 2048, reader);
	}

	if (!where) {
		/* This appears to be an empty document */
		return(NULL);
	}

	name = where[1] ? where + 1 : NULL;

	if (name && (WJEChild((WJElement) parent, name, WJE_GET))) {
		/* Do not load duplicate names */
		return(NULL);
	}

	if (loadcb && !loadcb((WJElement)parent, name, data, file, line)) {
		/* The consumer has rejected this item */
		return(NULL);
	}

	if ((l = _WJENew(parent, name, name ? strlen(name) : 0, file, line))) {
		switch ((l->pub.type = *where)) {
			default:
			case WJR_TYPE_UNKNOWN:
			case WJR_TYPE_NULL:
				break;

			case WJR_TYPE_OBJECT:
			case WJR_TYPE_ARRAY:
				while (reader && (current = WJRNext(where, 2048, reader))) {
					_WJELoad(l, reader, current, loadcb, data, file, line);
				}
				break;

			case WJR_TYPE_STRING:
				actual			= 0;
				used			= 0;
				len				= 0;
				complete		= FALSE;
				l->value.string	= NULL;
				l->pub.length	= 0;

				do {
					if ((value = WJRStringEx(&complete, &len, reader))) {
						if (used + len >= actual) {
							l->value.string = MemMallocEx(l->value.string,
								len + 1 + used, &actual, TRUE, TRUE);
							MemUpdateOwner(l->value.string, file, line);
						}

						memcpy(l->value.string + used, value, len);
						used += len;
						l->value.string[used] = '\0';
						l->pub.length = used;
					}
				} while (!complete);
				break;

			case WJR_TYPE_NUMBER:
#ifdef WJE_DISTINGUISH_INTEGER_TYPE
			case WJR_TYPE_INTEGER:
#endif
				l->value.number.hasDecimalPoint = WJRIntOrDouble(reader,
								&l->value.number.i,
								&l->value.number.d);

#ifdef WJE_DISTINGUISH_INTEGER_TYPE
				if (l->value.number.hasDecimalPoint) {
					l->pub.type = WJR_TYPE_NUMBER;
				} else {
					l->pub.type = WJR_TYPE_INTEGER;
				}
#endif

				if (WJRNegative(reader)) {
					/*
						Store the number as a positive, but keep track of the
						fact that it was negative.
					*/
					l->value.number.negative = TRUE;
				} else {
					l->value.number.negative = FALSE;
				}
				break;

			case WJR_TYPE_TRUE:
				l->value.boolean = TRUE;
				break;

			case WJR_TYPE_BOOL:
			case WJR_TYPE_FALSE:
				l->value.boolean = FALSE;
				break;
		}
	}

	return((WJElement) l);
}

EXPORT WJElement _WJEOpenDocument(WJReader reader, char *where, WJELoadCB loadcb, void *data, const char *file, const int line)
{
	WJElement	element;

	if ((element = _WJELoad(NULL, reader, where, loadcb, data, file, line))) {
		MemUpdateOwner(element, file, line);
	}

	return(element);
}

typedef struct WJEMemArgs
{
	char		*json;
	char		quote;

	size_t		len;
} WJEMemArgs;

static size_t WJEMemCallback(char *buffer, size_t length, size_t seen, void *userdata)
{
	WJEMemArgs	*args	= (WJEMemArgs *) userdata;
	char		*json;
	char		*q;
	size_t		len;

	if (!args || !args->json) {
		return(0);
	}

	len		= args->len - seen;
	json	= args->json + seen;

	if (!len) {
		/* Done */
		return(0);
	}

	if (len > length) {
		len = length;
	}

	if (len > 0) {
		memcpy(buffer, json, len);

		switch (args->quote) {
			case '"':
			case '\0':
				/* Default behavior, do nothing to quotes */
				break;

			default:
				/* Replace quotes in the data we just copied */
				json	= buffer;
				length	= len;

				while (length && (q = memchr(json, args->quote, length))) {
					*q = '"';

					length -= (q - json);
					json = q;
				}

				break;
		}
	}
	return(len);
}

/*
	Parse a JSON document already in memory directoy without requiring the
	consumer to create a WJReader. This is meant as a simple utility function
	and allows parsing documents with a non standard quote char for the sake of
	embedding documents directly in C code.
*/
EXPORT WJElement __WJEFromString(const char *json, char quote, const char *file, const int line)
{
	WJElement		doc;
	WJEMemArgs		args;
	WJReader		reader;

	args.json	= (char *) json;
	args.quote	= quote;

	if (json) {
		args.len = strlen(json);
	}

	if (json && (reader = WJROpenDocument(WJEMemCallback, &args, NULL, 0))) {
		doc = _WJEOpenDocument(reader, NULL, NULL, NULL, file, line);
		WJRCloseDocument(reader);
	}

	return(doc);
}

EXPORT char * _WJEToString(WJElement document, XplBool pretty, const char *file, const int line)
{
	WJWriter		writer;
	char			*mem	= NULL;

	if ((writer = WJWOpenMemDocument(pretty, &mem))) {
		WJEWriteDocument(document, writer, NULL);
		WJWCloseDocument(writer);
	}
	if (mem) {
		MemUpdateOwner(mem, file, line);
	}
	return(mem);
}

EXPORT WJElement WJEFromFile(const char *path)
{
	WJElement	e	= NULL;
	FILE		*f;
	WJReader	reader;

	if (!path) {
		errno = EINVAL;
		return(NULL);
	}

	if ((f = fopen(path, "rb"))) {
		if ((reader = WJROpenFILEDocument(f, NULL, 0))) {
			e = WJEOpenDocument(reader, NULL, NULL, NULL);
			WJRCloseDocument(reader);
		}

		fclose(f);
	}

	return(e);
}

EXPORT XplBool WJEToFile(WJElement document, XplBool pretty, const char *path)
{
	FILE			*f;
	WJWriter		writer;
	XplBool			ret	= FALSE;

	if (!document || !path) {
		errno = EINVAL;
		return(FALSE);
	}

	if ((f = fopen(path, "wb"))) {
		if ((writer = WJWOpenFILEDocument(pretty, f))) {
			ret = WJEWriteDocument(document, writer, NULL);

			WJWCloseDocument(writer);
		}

		fclose(f);
	}

	return(ret);
}

static WJElement _WJECopy(_WJElement *parent, WJElement original, WJECopyCB copycb, void *data, const char *file, const int line)
{
	_WJElement	*l = NULL;
	_WJElement	*o;
	WJElement	c;
	char		*tmp;

	if (!(o = (_WJElement *) original)) {
		return(NULL);
	}

	if (copycb && !copycb((WJElement) parent, (WJElement) original, data, file, line)) {
		/* The consumer has rejected this item */
		return(NULL);
	}

	if ((l = _WJENew(parent, original->name, original->name ? strlen(original->name) : 0, file, line))) {
		switch ((l->pub.type = original->type)) {
			default:
			case WJR_TYPE_UNKNOWN:
			case WJR_TYPE_NULL:
				break;

			case WJR_TYPE_OBJECT:
			case WJR_TYPE_ARRAY:
				for (c = original->child; c; c = c->next) {
					_WJECopy(l, c, copycb, data, file, line);
				}
				break;

			case WJR_TYPE_STRING:
				if ((tmp = WJEString(original, NULL, WJE_GET, ""))) {
					l->value.string = MemStrdup(tmp);
					l->pub.length = original->length;
				} else {
					l->value.string = MemStrdup("");
					l->pub.length = 0;
				}
				break;

			case WJR_TYPE_NUMBER:
#ifdef WJE_DISTINGUISH_INTEGER_TYPE
			case WJR_TYPE_INTEGER:
#endif
				l->value.number.negative		= o->value.number.negative;
				l->value.number.i				= o->value.number.i;
				l->value.number.d				= o->value.number.d;
				l->value.number.hasDecimalPoint	= o->value.number.hasDecimalPoint;

#ifdef WJE_DISTINGUISH_INTEGER_TYPE
				if (l->value.number.hasDecimalPoint) {
					l->pub.type = WJR_TYPE_NUMBER;
				} else {
					l->pub.type = WJR_TYPE_INTEGER;
				}
#endif

				break;

			case WJR_TYPE_TRUE:
			case WJR_TYPE_BOOL:
			case WJR_TYPE_FALSE:
				l->value.boolean = WJEBool(original, NULL, WJE_GET, FALSE);
				break;
		}
	}

	return((WJElement) l);
}

EXPORT WJElement _WJECopyDocument(WJElement to, WJElement from, WJECopyCB copycb, void *data, const char *file, const int line)
{
	if (to) {
		WJElement	c;

		for (c = from->child; c; c = c->next) {
			_WJECopy((_WJElement *) to, c, copycb, data, file, line);
		}
	} else {
		if ((to = _WJECopy(NULL, from, copycb, data, file, line))) {
			MemUpdateOwner(to, file, line);
		}
	}

	return(to);
}

EXPORT XplBool WJEMergeObjects(WJElement to, WJElement from, XplBool overwrite)
{
	WJElement	a, b;

	if (!to || !from ||
		WJR_TYPE_OBJECT != to->type || WJR_TYPE_OBJECT != from->type
	) {
		return(FALSE);
	}

	for (a = from->child; a; a = a->next) {
		if ((b = WJEChild(to, a->name, WJE_GET))) {
			if (WJR_TYPE_OBJECT == b->type && WJR_TYPE_OBJECT == a->type) {
				/* Merge all the children */
				WJEMergeObjects(b, a, overwrite);

				continue;
			} else if (overwrite) {
				/* Remove the existing value and overwrite it */
				WJECloseDocument(b);
			} else {
				/* Do nothing with this element */
				continue;
			}
		}

		/* Copy the object over */
		WJEAttach(to, WJECopyDocument(NULL, a, NULL, NULL));
	}

	return(TRUE);
}

EXPORT XplBool _WJEWriteDocument(WJElement document, WJWriter writer, char *name,
						WJEWriteCB precb, WJEWriteCB postcb, void *data)
{
	_WJElement	*current = (_WJElement *) document;
	WJElement	child;

	if (precb && !precb(document, writer, data)) {
		return(FALSE);
	}

	if (document) {
		if (document->writecb) {
			return(document->writecb(document, writer, name));
		}

		switch (current->pub.type) {
			default:
			case WJR_TYPE_UNKNOWN:
				break;

			case WJR_TYPE_NULL:
				WJWNull(name, writer);
				break;

			case WJR_TYPE_OBJECT:
				WJWOpenObject(name, writer);

				child = current->pub.child;
				do {
					_WJEWriteDocument(child, writer, child ? child->name : NULL,
						precb, postcb, data);
				} while (child && (child = child->next));

				WJWCloseObject(writer);
				break;

			case WJR_TYPE_ARRAY:
				WJWOpenArray(name, writer);

				child = current->pub.child;
				do {
					_WJEWriteDocument(child, writer, NULL,
						precb, postcb, data);
				} while (child && (child = child->next));

				WJWCloseArray(writer);
				break;

			case WJR_TYPE_STRING:
				WJWStringN(name, current->value.string, current->pub.length, TRUE, writer);
				break;

			case WJR_TYPE_NUMBER:
#ifdef WJE_DISTINGUISH_INTEGER_TYPE
			case WJR_TYPE_INTEGER:
#endif
				if (current->value.number.hasDecimalPoint) {
					current->pub.type = WJR_TYPE_NUMBER;
					if (!current->value.number.negative) {
						WJWDouble(name, current->value.number.d, writer);
					} else {
						WJWDouble(name, -current->value.number.d, writer);
					}
				} else {
#ifdef WJE_DISTINGUISH_INTEGER_TYPE
					current->pub.type = WJR_TYPE_INTEGER;
#endif
					if (!current->value.number.negative) {
						WJWUInt64(name, current->value.number.i, writer);
					} else {
						WJWInt64(name, -((int64) current->value.number.i), writer);
					}
				}
				break;

			case WJR_TYPE_TRUE:
			case WJR_TYPE_BOOL:
			case WJR_TYPE_FALSE:
				WJWBoolean(name, current->value.boolean, writer);
				break;
		}
	}

	if (postcb && !postcb(document, writer, data)) {
		return(FALSE);
	}

	return(TRUE);
}

EXPORT XplBool _WJECloseDocument(WJElement document, const char *file, const int line)
{
	_WJElement	*current = (_WJElement *) document;
	WJElement	child;

	if (!document) {
		return(FALSE);
	}

	WJEDetach(document);

	if (document->freecb && !document->freecb(document)) {
		/* The callback has prevented free'ing the document */
		return(TRUE);
	}

	WJEChanged(document);

	/* Remove references to this object */
	if (document->parent) {
		if (document->parent->child == document) {
			document->parent->child = document->next;
		}
		if (document->parent->last == document) {
			document->parent->last = document->prev;
		}
		document->parent->count--;
	}

	if (document->prev) {
		document->prev->next = document->next;
	}

	if (document->next) {
		document->next->prev = document->prev;
	}


	/* Destroy all children */
	while ((child = document->child)) {
		WJEDetach(child);
		_WJECloseDocument(child, file, line);
	}

	if (current->pub.type == WJR_TYPE_STRING) {
		MemFreeEx(current->value.string, file, line);
		current->pub.length = 0;
	}

	if (document->name && current->_name != document->name) {
		MemReleaseEx(&document->name, file, line);
	}

	MemFreeEx(current, file, line);

	return(TRUE);
}

EXPORT void WJEDump(WJElement document)
{
	WJEWriteFILE(document, stdout);
}

EXPORT void WJEWriteFILE(WJElement document, FILE* fd)
{
	WJWriter		dumpWriter;

	if ((dumpWriter = WJWOpenFILEDocument(TRUE, fd))) {
		WJEWriteDocument(document, dumpWriter, NULL);
		WJWCloseDocument(dumpWriter);
	}
	fprintf(fd, "\n");
	fflush(fd);
}

EXPORT void WJEDumpFile(WJElement document)
{
	WJWriter		dumpWriter;
	FILE			*file;
	char			path[1024];

	strprintf(path, sizeof(path), NULL, "%08lx.json", (long) time(NULL));

	if ((file = fopen(path, "wb"))) {
		if ((dumpWriter = WJWOpenFILEDocument(TRUE, file))) {
			WJEWriteDocument(document, dumpWriter, NULL);
			WJWCloseDocument(dumpWriter);
		}
		fprintf(file, "\n");
		fclose(file);

		printf("Dumped JSON document to %s\n", path);
	}
}

static size_t fileReaderCB( char *data, size_t length, size_t seen, void *client )
{
	DebugAssert(length);
	DebugAssert(data);
	if(!data) {
		return 0;
	}
	return fread(data, 1, length, client);
}

EXPORT WJElement WJEReadFILE(FILE* fd)
{
	WJReader		reader;
	WJElement		obj = NULL;

	if ((reader = WJROpenDocument(fileReaderCB, fd, NULL, 0))) {
		obj = WJEOpenDocument(reader, NULL, NULL, NULL);
		WJRCloseDocument(reader);
	}
	return obj;
}

EXPORT void WJEMemFree(void *mem)
{
	if(mem != NULL) {
		MemFree(mem);
	}
}
EXPORT void WJEMemRelease(void **mem)
{
	if(mem != NULL && *mem != NULL) {
		MemRelease(mem);
	}
}
