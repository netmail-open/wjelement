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
#include <stdlib.h>

static void _WJEHash(WJElement document, int depth, WJEHashCB update, void *context)
{
	WJElement	child;
	char		*s;
	int32		n;
	uint32		b;

	if (!document) {
		return;
	}

	switch (document->type) {
		default:
		case WJR_TYPE_UNKNOWN:
			break;

		case WJR_TYPE_NULL:
			update(context, "", 1);
			break;

		case WJR_TYPE_OBJECT:
			update(context, &depth, sizeof(depth));

			for (child = document->child; child; child = child->next) {
				if (child->name) {
					update(context, child->name, strlen(child->name) + 1);
				}
				_WJEHash(child, depth + 1, update, context);
			}

			update(context, &depth, sizeof(depth));
			break;

		case WJR_TYPE_ARRAY:
			update(context, &depth, sizeof(depth));

			for (child = document->child; child; child = child->next) {
				update(context, "", 1);
				_WJEHash(child, depth + 1, update, context);
			}

			update(context, &depth, sizeof(depth));
			break;

		case WJR_TYPE_STRING:
			if ((s = WJEString(document, NULL, WJE_GET, ""))) {
				update(context, s, strlen(s) + 1);
			}

			break;

		case WJR_TYPE_NUMBER:
#ifdef WJE_DISTINGUISH_INTEGER_TYPE
		case WJR_TYPE_INTEGER:
#endif
			n = WJENumber(document, NULL, WJE_GET, 0);
			update(context, &n, sizeof(n));

			break;

		case WJR_TYPE_TRUE:
		case WJR_TYPE_BOOL:
		case WJR_TYPE_FALSE:
			b = WJEBool(document, NULL, WJE_GET, FALSE);
			update(context, &b, sizeof(b));
			break;
	}
}

EXPORT void WJEHash(WJElement document, WJEHashCB update, void *context)
{
	_WJEHash(document, 0, update, context);
}


