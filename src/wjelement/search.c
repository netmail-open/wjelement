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
#include <ctype.h>

/*
	Return the element that would be 'next' if the 'root' element was flattened
	into a sequential list of items.

	If 'children' is FALSE then any children of 'last' will be excluded.
*/
static WJElement WJENext(WJElement root, WJElement last, XplBool children)
{
	if (!root)						return(NULL);
	if (!last)						return(root->child);
	if (children && last->child)	return(last->child);

	do {
		if (last->next)				return(last->next);
	} while ((last = last->parent) && last != root);

	return(NULL);
}

/* Return the depth of the provided element */
static int WJEDepth(WJElement root, WJElement e)
{
	int		r = 0;

	while (e && e != root) {
		e = e->parent;
		r++;
	}

	return(r);
}

/* Return the offset of the provided element within it's parent */
static long _WJEIndex(WJElement e)
{
	long		r = 0;

	while (e && e->prev) {
		e = e->prev;
		r++;
	}

	return(r);
}

static int WJEMatchAll(WJElement root, WJElement parent, WJElement e, WJEAction action, char *name, size_t len)
{
	if (e) {
		return(0);
	} else {
		/* It matches anything, but does not allow creation */
		return(-1);
	}
}

static int WJEMatchNone(WJElement root, WJElement parent, WJElement e, WJEAction action, char *name, size_t len)
{
	return(-1);
}

static int WJEMatchExact(WJElement root, WJElement parent, WJElement e, WJEAction action, char *name, size_t len)
{
	if (!e) {
		/*
			When called with a NULL element this becomes a check to see if
			creation is allowed.
		*/
		if (len) {
			return(0);
		} else {
			return(-1);
		}
	} else {
		/* Verify that the name matches exactly */

		if (e->name && strlen(e->name) == len &&
			name && !strnicmp(e->name, name, len)
		) {
			return(0);
		} else {
			return(-1);
		}
	}
}

/*
	Take a portion of a path, and clean it to the point that it can be used as
	a name for a new element, or for comparing to names of existing elements.

	Since the original path can't be modified a temporary can be returned if the
	name contains escaped characters.
*/
static char * WJECleanName(char *name, size_t *len, char **tmp)
{
	char	q;
	size_t	l;
	char	*p;

	if (!name || !len || !tmp) return(NULL);

	*tmp = NULL;
	if (isalnumx(*name)) {
		/* This is a regular old alpha numeric name.  Just use it. */
		return(name);
	}

	l = *len;
	q = *name;

	if (q != '"' && q != '\'') {
		/* This is not a quoted string.  We can't do anything with it. */
		return(NULL);
	}

	name++;
	l--;

	for (p = name; p && p < (name + l) && !*tmp; p++) {
		switch (*p) {
			case '\0':
				/* The string isn't terminated.  Can't use it. */
				return(NULL);

			case '"': case '\'':
				if (*p == q) {
					/* Found the closing quote, and there was nothing escaped */
					*len = p - name;
					return(name);
				}

			default:
				break;

			case '\\':
				/*
					The string contains at least one escape character.  We have
					to create a temporary copy.
				*/
				if (!(*tmp = MemStrndup(name, l))) {
					return(NULL);
				}
				break;
		}
	}

	if (*tmp) {
		/* Cleanup the temporary string */
		for (p = *tmp; p; p++) {
			if (*p == '\\') {
				if (!*(p + 1)) {
					/* It wasn't terminated */
					MemRelease(tmp);
					break;
				}

				memmove(p, p + 1, strlen(p));
			} else if (*p == q) {
				*p = '\0';
				break;
			} else if (!*p) {
				/* It wasn't terminated */
				MemRelease(tmp);
				break;
			}
		}
	}

	if (*tmp) {
		*len = strlen(*tmp);
	}
	return(*tmp);
}

static int WJEMatchSubscript(WJElement root, WJElement parent, WJElement e, WJEAction action, char *name, size_t len)
{
	/*
		Walk through all items in the subscript and do the proper check on each
		piece.  Return 0 on the first match.

		The name has already been validated, so the existance of a closing ] is
		a sure thing, and any quoted string will be terminated.
	*/
	char		*p;
	int			a, b;
	char		*x, *y, *tmp;
	size_t		l;
	long		i, r;

	/* Find the index and the wrapped index of this element */
	if (e) {
		i = _WJEIndex(e);
		r = i - parent->count;
	} else {
		/*
			A NULL e may match $, or parent->count, both of which reference a
			spot that elements may be inserted into an array.
		*/
		i = r = parent->count;
	}

	p = name;
	while (p) {
		switch (*p) {
			case '"':
				/*
					Compare a character at a time, handling escaped characters
					and ignoring case.
				*/
				if (e && e->name) {
					y = e->name;
				} else {
					y = "";
				}

				for (x = p + 1; ; x++, y++) {
					if (*x == '\\' && *(x + 1)) {
						x++;
						y--;

						continue;
					}

					if (*x == '"') {
						if (!*y && e) {
							return(0);
						} else {
							break;
						}
					}

					if (!*y || !*x || tolower(*x) != tolower(*y)) {
						break;
					}
				}

				/* No match.  Skip the rest of the quoted string */
				for (; *x; x++) {
					if (*x == '\\' && *(x + 1)) {
						x++;
					} else if (*x == '"') {
						break;
					}
				}

				if (*x == '"') {
					p = x + 1;

					if (!e) {
						/*
							This was a valid name, so we can allow an element to
							be created.
						*/
						return(0);
					}
				} else {
					p = NULL;
				}
				break;

			case '\'':
				/*
					Get a cleaned string that does not contain any escaped
					characters that can be passed to stripatn().
				*/
				tmp = NULL;
				l = len - (p - name);
				if (e && (x = WJECleanName(p, &l, &tmp))) {
					if (!stripatn(e->name, x, l)) {
						if (tmp) MemRelease(&tmp);
						return(0);
					}
				}

				if (tmp) MemRelease(&tmp);

				/* No match.  Skip the rest of the quoted string */
				for (x = p + 1; *x; x++) {
					if (*x == '\\' && *(x + 1)) {
						x++;
					} else if (*x == '\'') {
						break;
					}
				}

				if (*x == '\'') {
					p = x + 1;

					if (!e) {
						/*
							This was a valid name, so we can allow an element to
							be created.
						*/
						return(0);
					}
				} else {
					p = NULL;
				}

				break;

			default:
				/*
					The value wasn't a quoted string, so it must be an offset,
					or a range.
				*/
				a = b = strtol(p, &tmp, 0); b++;
				if (tmp == p) {
					/* There where no valid numbers, this is not a valid offset */
					return(- __LINE__);
				}

				p = skipspace(tmp);
				if (*p == ':' && (p = skipspace(p + 1))) {
					b = strtol(p, &tmp, 0);
					if (tmp == p) {
						/* There where no valid numbers, bogus */
						return(- __LINE__);
					}

					p = skipspace(tmp);
				}

				/* The offset or range is ready to check */
				if ((i >= a && i < b) || (r >= a && r < b)) {
					if (e) return(0);

					/*
						The case below handles $, which is used as a special
						offset to append to an array.  The offset here is
						referencing the same thing, so fallthrough.
					*/
				} else {
					/* No match */
					break;
				}

			case '$':
				if (e) {
					if (action == WJE_GET && parent->type == WJR_TYPE_ARRAY && -1 == r) {
						return(0);
					}

					break;
				}


				if (e) break;
				if (!parent->count && (
					parent->type == WJR_TYPE_OBJECT ||
					parent->type == WJR_TYPE_UNKNOWN
				)) {
					/*
						They are trying to append an item to an empty object.
						Chances are this object was just created, and an array
						was really wanted.
					*/
					parent->type = WJR_TYPE_ARRAY;
				}
				if (WJR_TYPE_ARRAY != parent->type) break;

				/* Allow appending an item to the array */
				return(0);

			case '\0':
			case ']':
				return(- __LINE__);
		}

		if (!(p = skipspace(p)) || *p != ',') {
			/* There must be at least one comma between items */
			return(- __LINE__);
		}

		/* Skip any extra commas and whitespace */
		while ((p = skipspace(p)) && *p == ',') {
			p++;
		}
	}

	return(- __LINE__);
}


#ifdef DEBUG
#define DEBUG_WJE 1
#endif

/*
	Helper for WJENextName() to be called when the end of a portion of the path
	has been found to verify that the start of the next portion appears valid.
*/
static int WJENextNameCheck(char *path, char *name, size_t *len, char **end)
{
#ifdef DEBUG_WJE
	char	*originalpath = path;
#endif
	path = skipspace(path);

	switch (*path) {
		case '|':
		case '[':
			if (end) *end = path;
			break;

		case '.':
			path++;
			if (isalnumx(*path)) {
				if (end) *end = path;
				break;
			}

			/* fallthrough */
		default:
#ifdef DEBUG_WJE
			fprintf(stderr,
				"WJElement: Invalid syntax at byte %ld in JSON path: %s\n",
					(long) (path - originalpath), path);
DebugAssert(0);
#endif
			if (len) *len = 0;
			return(-1);

		case ';': case '\0':
		case '>': case '<':
		case '!': case '=':
			/* The last name has been found, but there is a condition */
			if (*path) {
				if (end) *end = path;
			} else {
				if (end) *end = NULL;
			}

			/*
				If there where spaces before the condition operator those are
				not part of the current name.
			*/
			if (len && path) {
				while (*len > 0 && isspace(name[*len - 1])) {
					(*len)--;
				}
			}
			break;
	}

	return(0);
}

/*
	Return the begining of the name of the next element in the path.  Length
	will be filled out, and end will be set to the first character that is not
	part of the path for this element.

	A match function may also be provided which can be used to determine if a
	specific element is a match for this portion of the path.  The match
	function assumes that all ancestory of this element have already been
	matched to their respective portions of the path.

	The match function will be set based on the syntax that the portion of the
	path used to ensure that the correct comparison is done.
*/
static char * WJENextName(char *path, size_t *len, char **end, WJEMatchCB *match, XplBool *specific)
{
	char	*p;
	char	q;

	if (len)		*len		= 0;
	if (end)		*end		= NULL;
	if (match)		*match		= WJEMatchNone;
	if (specific)	*specific	= TRUE;

	if (!path || !*path) return(NULL);

	if ('|' == *path) {
		/* Part of the path was marked as optional, continue */
		path++;
	}

	if ('[' == *path) {
		/*
			subscript

			Walk past any subscript values that appear to be correct, and find
			the closing square bracket.  The next portion of the name must start
			with another opening square bracket, or a dot.
		*/
		p = path = skipspace(path + 1);

		/* An empty subscript is a special case that allows referencing all */
		if (p && *p == ']') {
			if (len) *len = 0;
			if (!WJENextNameCheck(p + 1, p, len, end)) {
				if (match)		*match		= WJEMatchAll;
				if (specific)	*specific	= FALSE;
				return(p);
			}
		}

		while (p) {
			switch (*p) {
				case '"':
				case '\'':
					q = *p;

					/*
						Skip to the end of the quoted string, skipping escaped
						characters
					*/
					for (p++; *p; p++) {
						if ('\\' == *p && *(p + 1)) {
							p++;
						} else if (q == *p) {
							break;
						} else if (specific && q == '\'' &&
							('*' == *p || '?' == *p)
						) {
							*specific = FALSE;
						}
					}

					if (q != *p) {
#ifdef DEBUG_WJE
						fprintf(stderr,
							"WJElement: Unterminated quoted string in JSON path: %s\n",
							path);
#endif
						return(NULL);
					}
					p++;

					break;

				case '$':
					/*
						A $ in a subscript references the item AFTER last.  It
						can not match any existing item, but is useful for
						appending to an array.

						Even if this is the only item it is not specific because
						it doesn't reference a single name.
					*/
					if (specific) *specific = FALSE;
					p++;
					break;

				case '\0':
#ifdef DEBUG_WJE
					fprintf(stderr,
						"WJElement: Unterminated subscript string in JSON path: %s\n",
						path);
#endif
					return(NULL);

				default:
#ifdef DEBUG_WJE
					fprintf(stderr,
						"WJElement: Invalid syntax at byte %ld in JSON path: %s\n",
							(long) (p - path), path);
DebugAssert(0);
#endif
					return(NULL);

				/*
					Allow any character that is valid in an offset or offset
					range and whitespace.
				*/
				case ':':
					if (specific) *specific = FALSE;
					/* fallthrough */

				case '+': case '-': case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7': case '8': case '9':
				case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				case 'x': case 'X': case ' ': case '\t':
					p++;
					break;

				case ']':
					if (len) *len = p - path;

					p++;
					if (!WJENextNameCheck(p, path, len, end)) {
						if (match) *match = WJEMatchSubscript;
						return(path);
					}

					return(NULL);
			}

			/* Skip any commas and whitespace */
			while ((p = skipspace(p)) && *p == ',') {
				if (specific) *specific = FALSE;
				p++;
			}
		}
	} else if (isalnumx(*path)) {
		/*
			alpha-numeric name

			Find the first character that is not part of the alpha-numeric name
			and verify that it is a valid terminator for this name.
		*/
		for (p = path + 1; isalnumx(*p); p++);

		if (len) *len = p - path;
		if (!WJENextNameCheck(p, path, len, end)) {
			if (match) *match = WJEMatchExact;
			return(path);
		}
	}

	return(NULL);
}

static int WJECheckCondition(WJElement e, char **condition, WJEAction action)
{
	char		*cond, *value, *tmp, *v, *s;
	size_t		len;
	double		r;

	if (!e || !condition || !*condition || !**condition) return(0);
	cond = *condition;

	switch (*cond) {
		case '>': case '<': case '!': case '=': case ';':
			value = cond + 1;
			if ('=' == *value) value++;
			break;

		default:
			/* This is part of the path, not a condition */
			return(0);
	}

	switch (action) {
		case WJE_NEW:
		case WJE_SET:
			/* Conditions do not behave correctly with WJE_NEW or WJE_SET */
			DebugAssert(0);
			break;

		default:
			break;
	}

	value = skipspace(value);

	if (';' == *cond) {
		if (WJEGet(e, value, NULL)) {
			if (condition) *condition = NULL;
			return(0);
		} else {
			return(-1);
		}
	}

	r = strtod(value, &tmp);
	tmp = skipspace(tmp);
	if (r == 0 && *tmp) {
		/* Let's assume this wasn't a double */
		r = (double) strtol(value, &tmp, 0);
	}

	if (tmp > value) {
		r -= WJEDouble(e, NULL, WJE_GET, 0);

		if ((value = skipspace(tmp)) && *value) return(-1);
	} else {
		r	= -1;
		tmp	= NULL;
		len	= strlen(value);

		if ((s = WJEString(e, NULL, WJE_GET, NULL)) &&
			(v = WJECleanName(value, &len, &tmp))
		) {
			if ('\'' == *value) {
				r = stripatn(s, v, len);
			} else if (!(r = strnicmp(s, v, len))) {
				r = strlen(s) - len;
			}
		}

		if (tmp) MemRelease(&tmp);
	}

	// printf("\"%s\" is %d\n", cond, r);
	switch (*cond) {
		case '<':
			if (*(cond + 1) == '=') {
				r = !(r >= 0);
			} else {
				r = !(r > 0);
			}
			break;

		case '>':
			if (*(cond + 1) == '=') {
				r = !(r <= 0);
			} else {
				r = !(r < 0);
			}
			break;

		case '!':
			r = (!r);
			break;

		case '=':
			r = (r);
			break;

		default:
			r = -1;
	}

	if (!r && condition) *condition = NULL;
	return(r);
}

/* Find a child WJElement by name.  See description in wjelement.h */
WJElement WJESearch(WJElement container, const char *path, WJEAction *action, WJElement last, const char *file, const int line)
{
	WJElement	e, n;
	WJElement	match		= NULL;
	WJElement	parent		= NULL;
	char		*name		= NULL;
	char		*end		= NULL;
	size_t		len			= 0;
	WJEMatchCB	cb			= NULL;
	XplBool		specific	= TRUE;
	char		*tmp;
	int			depth;

	if (!container && (*action == WJE_NEW || *action == WJE_SET)) {
		if ((container = WJENew(NULL, NULL, 0, file, line))) {
			MemUpdateOwner(container, file, line);
			container->type = WJR_TYPE_UNKNOWN;
		}
	}

	MemAssert(container);

	/* An empty name, or "." refers to the current object, so just return it */
	if (!path || !*path || !stricmp(path, ".") || !container) {
		if (!last) {
			return(container);
		} else {
			return(NULL);
		}
	}

	/*
		'match' points to the last partial match.  If it is non-NULL then it's
		children will be checked when looking for a full match for the path.

		If 'last' is non-NULL then it was a complete match, so it's children
		should NOT be checked.
	*/
	if (last) {
		match = NULL;
	} else {
		match = container;
	}

	e = last;
	for (;;) {
		if (match && !match->child && (*action == WJE_NEW || *action == WJE_SET)) {
			/* Insert additional child elements if needed to satisfy the path */
			e		= NULL;
			parent	= match;
			depth	= WJEDepth(container, parent) + 1;
		} else {
			/* Find the next potential match */
			if (!(e = WJENext(container, e, match != NULL))) {
				break;
			}
			parent	= e->parent;
			depth	= WJEDepth(container, e);
		}

		/*
			Find the next portion of the path, and additional information about
			it to allow matching it to an element or to create a new match.
		*/
		cb	= NULL;
		end	= (char *) path;
		for (; depth; depth--) {
			if (!(name = WJENextName(end, &len, &end, &cb, &specific))) {
				break;
			}
		}

		if (!cb || !name || depth) {
			/* This portion of the path is invalid.  Bail. */
			return(NULL);
		}

		/*
			Does e match?

			Since all ancestors of e have already been verified and the current
			portion of the path has already been parsed might as well check
			siblings of e as well.
		*/
		for (n = e; n; n = n->next) {
			e = n;
			if (!cb(container, n->parent, n, *action, name, len) &&
				(!end || !*end || !WJECheckCondition(n, &end, *action))
			) {
				/* n matches */
				break;
			}
		}

		if (n && *action == WJE_NEW && (!end || !*end)) {
			/*
				n matches the path completely, meaning that a new element can't
				be created.  The match must be returned, but the caller needs to
				know that it is not a new element.
			*/
			*action = WJE_GET;
		}

		if (!n && (*action == WJE_NEW || *action == WJE_SET) &&
			!cb(container, parent, NULL, *action, name, len)
		) {
			/*
				A match wasn't found, but it is possible that an element can be
				created to create a match.

				Calling cb() with a NULL element will return 0 if creation is
				valid with the current name.
			*/
			if (specific &&
				(parent->type == WJR_TYPE_OBJECT || parent->type == WJR_TYPE_UNKNOWN)
			) {
				parent->type = WJR_TYPE_OBJECT;
				tmp = NULL;
				if ((name = WJECleanName(name, &len, &tmp))) {
					n = WJENew(parent, name, len, file, line);
				}
				if (tmp) MemRelease(&tmp);
			} else if ( (parent->type == WJR_TYPE_ARRAY)	||
						(parent->type == WJR_TYPE_UNKNOWN)	||
						(parent->type == WJR_TYPE_OBJECT && !parent->count)
			) {
				parent->type = WJR_TYPE_ARRAY;

				/* append an unnamed item to the end of the array */
				n = WJENew(parent, NULL, 0, file, line);
			}

			/* The consumer must set the type */
			if (n) n->type = WJR_TYPE_UNKNOWN;
		}

		if (!(match = n)) {
			/* A match couldn't be found */
			if (!e) e = parent;
			continue;
		}

		if (!end || !*end) {
			/* We have a matched the whole selector */
			return(n);
		}

		if (n && !n->child && '|' == *end) {
			/*
				The rest of the selector is marked as optional, and we have no
				children, so consider this a match
			*/
			return(n);
		}

		/* But there is more to check */
		continue;
	}

	return(NULL);
}

EXPORT WJElement _WJEGet(WJElement container, char *path, WJElement last, const char *file, const int line)
{
	WJEAction	a = WJE_GET;

	return(WJESearch(container, path, &a, last, file, line));
}

EXPORT WJElement _WJEChild(WJElement container, char *name, WJEAction action, const char *file, const int line)
{
	WJElement		e;

	if (!container || !name) return(NULL);

	for (e = container->child; e; e = e->next) {
		if (e->name && !strcmp(e->name, name)) {
			break;
		}
	}

	switch (action) {
		case WJE_GET:
		case WJE_PUT:
			return(e);

		case WJE_NEW:
			if (!e) {
				e = WJENew(container, name, strlen(name), file, line);
			}

			break;

		case WJE_SET:
			if (!e) {
				e = WJENew(container, name, strlen(name), file, line);
			} else {
				e = WJEReset(e, e->type);
			}

			break;
	}

	if (e) {
		MemUpdateOwner(e, file, line);
	}
	return(e);
}

