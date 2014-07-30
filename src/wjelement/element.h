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


#ifndef __WJ_ELEMENT_P_H
#define __WJ_ELEMENT_P_H

#include <string.h>
#include <stdlib.h>

#include <xpl.h>
#include <hulautil.h>
#include <memmgr.h>

#include <wjelement.h>

typedef struct {
	WJElementPublic		pub;
	WJElementPublic		*parent;

	union {
		char			*string;
		XplBool			boolean;

		struct {
			uint64		i;
			double		d;

			XplBool		hasDecimalPoint;
			XplBool		negative;
		} number;
	} value;

	char				_name[];
} _WJElement;

/* element.c */
_WJElement * _WJENew(_WJElement *parent, char *name, size_t len, const char *file, int line);
_WJElement * _WJEReset(_WJElement *e, WJRType type);

/* search.c */
typedef int (* WJEMatchCB)(WJElement root, WJElement parent, WJElement e, WJEAction action, char *name, size_t len);
WJElement WJESearch(WJElement container, const char *path, WJEAction *action, WJElement last, const char *file, const int line);

/*
	Allow a few extra characters in dot seperated alpha numeric names for the
	sake of backwards compatability.
*/
#define isalnumx(c)				(						\
									isalnum((c))	||	\
									' ' == (c)		||	\
									'_' == (c)		||	\
									'-' == (c)			\
								)

/*
	The following macros allow using many internal functions against a public
	WJElement or a private _WJElement * without needing to cast the arguments or
	result.

	A function with a leading _ indicates that it uses the private versions, and
	without uses the public.

	This convention is not used on exported functions, which use a leading _ to
	indicate an advanced version of the function.
*/
#define WJENew(p, n, l, f, ln)			(WJElement) _WJENew((_WJElement *) (p), (n), (l), (f), (ln))
#define WJEReset(e, t)					(WJElement) _WJEReset((_WJElement *) (e), t)
#define _WJESearch(c, p, a, l, f, ln)	(_WJElement *) WJESearch((c), (p), (a), (l), (f), (ln))

void WJEChanged(WJElement element);
#define _WJEChanged(e) WJEChanged((WJElement) (e))

#endif // __WJ_ELEMENT_P_H
