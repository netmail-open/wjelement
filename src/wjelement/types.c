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

EXPORT XplBool __WJEBool(WJElement container, const char *path, WJEAction action, WJElement *last, XplBool value, const char *file, const int line)
{
	_WJElement		*e;
	char			*s;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.
	*/
	e = _WJESearch(container, path, &action, last ? *last : NULL, file, line);
	if (e) {
		switch (e->pub.type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->pub.type = WJR_TYPE_BOOL;
				break;

			case WJR_TYPE_BOOL:
			case WJR_TYPE_TRUE:
			case WJR_TYPE_FALSE:
				if (e->value.boolean == value) {
					break;
				}
				/* fallthrough */

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = (WJElement) e;
	switch ((action & WJE_ACTION_MASK)) {
		default:
		case WJE_GET:
			if (!e) return(value);

			switch (e->pub.type) {
				case WJR_TYPE_BOOL:
				case WJR_TYPE_TRUE:
				case WJR_TYPE_FALSE:
					return(e->value.boolean);

				case WJR_TYPE_NUMBER:
					if (e->value.number.hasDecimalPoint) {
						return(e->value.number.d != 0);
					} else {
						return(e->value.number.i != 0);
					}
					break;

				default:
				case WJR_TYPE_UNKNOWN:
				case WJR_TYPE_NULL:
					/* Treat NULL as FALSE */
					return(FALSE);

				case WJR_TYPE_OBJECT:
				case WJR_TYPE_ARRAY:
					/* TRUE based on the existance of the object */
					return(TRUE);

				case WJR_TYPE_STRING:
					if ((s = e->value.string) && (
						!stricmp(s, "true")		|| !stricmp(s, "yes")	||
						!stricmp(s, "enabled")	|| !stricmp(s, "1")		||
						!stricmp(s, "t")		|| !stricmp(s, "on")
					)) {
						return(TRUE);
					} else {
						return(FALSE);
					}
			}

		case WJE_SET:
		case WJE_NEW:
		case WJE_PUT:
			if ((e = _WJEReset(e, value ? WJR_TYPE_TRUE : WJR_TYPE_FALSE))) {
				return((e->value.boolean = value));
			} else {
				return(!value);
			}
	}
}

static void _WJESetNum(void *dest, size_t size, XplBool issigned, uint64 src, XplBool negative)
{
	if (size == sizeof(uint64)) {
		if (issigned) {
			if (!negative) {
				(*((int64 *) dest)) = src;
			} else {
				(*((int64 *) dest)) = -src;
			}
		} else {
			(*((uint64 *) dest)) = src;
		}
	} else if (size == sizeof(uint32)) {
		if (issigned) {
			if (!negative) {
				(*((int32 *) dest)) = src;
			} else {
				(*((int32 *) dest)) = -src;
			}
		} else {
			(*((uint32 *) dest)) = src;
		}
	} else {
		DebugAssert(0);
	}
}

static uint64 _WJEGetNum(void *value, size_t size, XplBool issigned, XplBool *negative)
{
	uint64		num = 0;

	*negative = FALSE;

	if (size == sizeof(uint64)) {
		if (issigned) {
			if (*((int64 *) value) < 0) {
				*negative = TRUE;

				num = -(*((int64 *) value));
			} else {
				num = (*((int64 *) value));
			}
		} else {
			num = (*((uint64 *) value));
		}
	} else if (size == sizeof(uint32)) {
		if (issigned) {
			if (*((int32 *) value) < 0) {
				*negative = TRUE;

				num = -(*((int32 *) value));
			} else {
				num = (*((int32 *) value));
			}
		} else {
			num = (*((uint32 *) value));
		}
	} else {
		DebugAssert(0);
	}

	return(num);
}

static void _WJENum(WJElement container, const char *path, WJEAction action, WJElement *last, void *value, size_t size, XplBool issigned, const char *file, const int line)
{
	_WJElement		*e;
	char			*s, *end;
	int				r;
	uint64			v;
	XplBool			negative;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.
	*/
	e = _WJESearch(container, path, &action, last ? *last : NULL, file, line);

	if (e) {
		switch (e->pub.type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->pub.type = WJR_TYPE_NUMBER;
				break;

			case WJR_TYPE_NUMBER:
				negative = FALSE;

				if ((e->value.number.i == _WJEGetNum(value, size, issigned, &negative)) &&
					negative == e->value.number.negative
				) {
					break;
				}
				/* fallthrough */

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = (WJElement) e;
	switch ((action & WJE_ACTION_MASK)) {
		default:
		case WJE_GET:
			if (!e) {
				/* Leave value alone */
				return;
			}

			switch (e->pub.type) {
				case WJR_TYPE_NUMBER:
					_WJESetNum(value, size, issigned, e->value.number.i, e->value.number.negative);
					return;

				case WJR_TYPE_BOOL:
				case WJR_TYPE_TRUE:
				case WJR_TYPE_FALSE:
					if (e->value.boolean) {
						_WJESetNum(value, size, issigned, 1, FALSE);
					} else {
						_WJESetNum(value, size, issigned, 0, FALSE);
					}
					return;

				case WJR_TYPE_STRING:
					/* Does the string contain a number? */
					s = skipspace(e->value.string);
					r = strtol(s, &end, 0);
// TODO	Check for a -, and use 64 bit functions
					if (end != s && (!(s = skipspace(end)) || !*s)) {
						_WJESetNum(value, size, issigned, r, FALSE);
						return;
					}
					/* fallthrough */

				default:
				case WJR_TYPE_UNKNOWN:
				case WJR_TYPE_NULL:
				case WJR_TYPE_OBJECT:
				case WJR_TYPE_ARRAY:
					return;
			}

		case WJE_SET:
		case WJE_NEW:
		case WJE_PUT:
			if ((e = _WJEReset(e, WJR_TYPE_NUMBER))) {
				e->value.number.negative		= FALSE;
				e->value.number.hasDecimalPoint	= FALSE;

				e->value.number.i = _WJEGetNum(value, size, issigned, &e->value.number.negative);
				e->value.number.d = (double) e->value.number.i;
				return;
			} else {
				/* Negate the value - It must NOT match the original */

				v = _WJEGetNum(value, size, issigned, &negative);

				_WJESetNum(value, size, issigned, !v, negative);
				return;
			}
	}
}

EXPORT char * __WJEString(WJElement container, const char *path, WJEAction action, WJElement *last, const char *value, const char *file, const int line)
{
	size_t		len	= 0;

	switch ((action & WJE_ACTION_MASK)) {
		default:
			break;

		case WJE_SET:
		case WJE_NEW:
		case WJE_PUT:
			if (value) {
				len = strlen(value);
			}
			break;
	}

	return(__WJEStringN(container, path, action, last, value, len, file, line));
}

EXPORT char * __WJEStringN(WJElement container, const char *path, WJEAction action, WJElement *last, const char *value, size_t len, const char *file, const int line)
{
	_WJElement		*e;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.
	*/
	e = _WJESearch(container, path, &action, last ? *last : NULL, file, line);
	if (e) {
		switch (e->pub.type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->pub.type = WJR_TYPE_STRING;
				break;

			case WJR_TYPE_STRING:
				if (!e->value.string && !value) {
					break;
				}

				if (e->value.string && value && !wstrcmp(e->value.string, value, action)) {
					break;
				}
				/* fallthrough */

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = (WJElement) e;
	switch ((action & WJE_ACTION_MASK)) {
		default:
		case WJE_GET:
			if (!e) return((char *) value);

			switch (e->pub.type) {
				case WJR_TYPE_STRING:
					if (e->value.string) {
						return(e->value.string);
					} else {
						break;
					}

				case WJR_TYPE_BOOL:
				case WJR_TYPE_TRUE:
				case WJR_TYPE_FALSE:
					if (e->value.boolean) {
						return("true");
					} else {
						return("false");
					}

				default:
				case WJR_TYPE_NUMBER:
				case WJR_TYPE_UNKNOWN:
				case WJR_TYPE_NULL:
				case WJR_TYPE_OBJECT:
				case WJR_TYPE_ARRAY:
					break;
			}

			return((char *) value);

		case WJE_SET:
		case WJE_NEW:
		case WJE_PUT:
			if ((e = _WJEReset(e, WJR_TYPE_STRING))) {
				if (!value) {
					return((e->value.string = NULL));
				} else {
					e->value.string = MemMallocWait(len + 1);
					strncpy(e->value.string, value, len);
					e->value.string[len] = '\0';

					MemUpdateOwner(e->value.string, file, line);
					e->pub.length = len;
					return(e->value.string);
				}
			} else {
				return(NULL);
			}
	}
}

EXPORT WJElement __WJEObject(WJElement container, const char *path, WJEAction action, WJElement *last, const char *file, const int line)
{
	WJElement	e;
	WJEAction	a;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.

		When action is WJE_GET skip any elements of the wrong type.
	*/
	e = (last ? *last : NULL);
	do {
		a = action;
		e = WJESearch(container, path, &a, e, file, line);
	} while (e && (action & WJE_ACTION_MASK) == WJE_GET && e->type != WJR_TYPE_OBJECT);

	if (e) {
		switch (e->type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->type = WJR_TYPE_OBJECT;
				break;

			case WJR_TYPE_OBJECT:
				break;

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = e;

	switch ((a & WJE_ACTION_MASK)) {
		case WJE_SET:
		case WJE_PUT:
			return(WJEReset(e, WJR_TYPE_OBJECT));

		default:
		case WJE_GET:
		case WJE_NEW:
			return(e);
	}
}

EXPORT WJElement __WJEArray(WJElement container, const char *path, WJEAction action, WJElement *last, const char *file, const int line)
{
	WJElement	e;
	WJEAction	a;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.

		When action is WJE_GET skip any elements of the wrong type.
	*/
	e = (last ? *last : NULL);
	do {
		a = action;
		e = WJESearch(container, path, &a, e, file, line);
	} while (e && (action & WJE_ACTION_MASK) == WJE_GET && e->type != WJR_TYPE_ARRAY);

	if (e) {
		switch (e->type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->type = WJR_TYPE_ARRAY;
				break;

			case WJR_TYPE_ARRAY:
				break;

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = e;

	switch ((a & WJE_ACTION_MASK)) {
		case WJE_SET:
		case WJE_PUT:
			return(WJEReset(e, WJR_TYPE_ARRAY));

		default:
		case WJE_GET:
		case WJE_NEW:
			return(e);
	}
}

EXPORT WJElement __WJENull(WJElement container, const char *path, WJEAction action, WJElement *last, const char *file, const int line)
{
	WJElement	e;
	WJEAction	a;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.

		When action is WJE_GET skip any elements of the wrong type.
	*/
	e = (last ? *last : NULL);
	do {
		a = action;
		e = WJESearch(container, path, &a, e, file, line);
	} while (e && (action & WJE_ACTION_MASK) == WJE_GET && e->type != WJR_TYPE_NULL);

	if (e) {
		switch (e->type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->type = WJR_TYPE_NULL;
				break;

			case WJR_TYPE_NULL:
				break;

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = e;

	switch ((a & WJE_ACTION_MASK)) {
		case WJE_SET:
		case WJE_PUT:
			return(WJEReset(e, WJR_TYPE_NULL));

		default:
		case WJE_GET:
		case WJE_NEW:
			return(e);
	}
}

EXPORT int32 __WJEInt32(WJElement container, const char *path, WJEAction action, WJElement *last, int32 value, const char *file, const int line)
{
	_WJENum(container, path, action, last, &value, sizeof(value), TRUE, file, line);

	return(value);
}

EXPORT uint32 __WJEUInt32(WJElement container, const char *path, WJEAction action, WJElement *last, uint32 value, const char *file, const int line)
{
	_WJENum(container, path, action, last, &value, sizeof(value), FALSE, file, line);

	return(value);
}


EXPORT int64 __WJEInt64(WJElement container, const char *path, WJEAction action, WJElement *last, int64 value, const char *file, const int line)
{
	_WJENum(container, path, action, last, &value, sizeof(value), TRUE, file, line);

	return(value);
}

EXPORT uint64 __WJEUInt64(WJElement container, const char *path, WJEAction action, WJElement *last, uint64 value, const char *file, const int line)
{
	_WJENum(container, path, action, last, &value, sizeof(value), FALSE, file, line);

	return(value);
}

EXPORT double __WJEDouble(WJElement container, const char *path, WJEAction action, WJElement *last, double value, const char *file, const int line)
{
	_WJElement		*e;

	/*
		Find an element that is appropriate for the given action, creating new
		elements as needed.
	*/
	if ((e = _WJESearch(container, path, &action, last ? *last : NULL, file, line))) {
		switch (e->pub.type) {
			case WJR_TYPE_UNKNOWN:
				/*
					A new object was created, and the changes count has already
					been updated.
				*/
				e->pub.type = WJR_TYPE_BOOL;
				break;

			case WJR_TYPE_NUMBER:
				if (e->value.number.hasDecimalPoint) {
					if (e->value.number.negative) {
						if (e->value.number.d * -1.0 == value) {
							break;
						}
					} else {
						if (e->value.number.d == value) {
							break;
						}
					}
				} else {
					if (e->value.number.negative) {
						if ((double) e->value.number.i * -1.0 == value) {
							break;
						}
					} else {
						if ((double) e->value.number.i == value) {
							break;
						}
					}
				}
				/* fallthrough */

			default:
				if (WJE_GET != (action & WJE_ACTION_MASK)) {
					_WJEChanged(e);
				}
				break;
		}
	}

	if (last) *last = (WJElement) e;
	switch ((action & WJE_ACTION_MASK)) {
		default:
		case WJE_GET:
			if (!e) return(value);

			switch (e->pub.type) {
				case WJR_TYPE_NUMBER:
					if (e->value.number.hasDecimalPoint) {
						if (e->value.number.negative) {
							return(e->value.number.d * -1.0);
						} else {
							return(e->value.number.d);
						}
					} else {
						if (e->value.number.negative) {
							return((double) e->value.number.i * -1.0);
						} else {
							return((double) e->value.number.i);
						}
					}
					break;

				case WJR_TYPE_BOOL:
				case WJR_TYPE_TRUE:
				case WJR_TYPE_FALSE:
					if (e->value.boolean) {
						return(1);
					} else {
						return(0);
					}
					break;

				default:
					return(0);
			}

		case WJE_SET:
		case WJE_NEW:
		case WJE_PUT:
			if ((e = _WJEReset(e, WJR_TYPE_NUMBER))) {
				if (value < 0) {
					e->value.number.negative = TRUE;
					e->value.number.d = -value;
					e->value.number.i = (uint64) -value;
				} else {
					e->value.number.negative = FALSE;
					e->value.number.d = value;
					e->value.number.i = (uint64) value;
				}

				if (e->value.number.d != (double) e->value.number.i) {
					e->value.number.hasDecimalPoint = TRUE;
				} else {
					e->value.number.hasDecimalPoint = FALSE;
				}

				return(value);

			} else {
				/* Negate the value - It must NOT match the original */
				return(-value);
			}
	}
}

static char * WJEPathF(char *buffer, size_t len, const char *pathf, va_list args)
{
	size_t		needed;
	char		*path;
	va_list		localargs;

	needed		= len;
	*buffer		= '\0';
	path		= buffer;

	/* Use a copy of the args to avoid modifying the consumer's data */
	va_copy(localargs, args);

	vstrcatf(path, len, &needed, pathf, localargs);
	if (needed <= len) {
		/* Success */
		return(path);
	}

	/*
		They needed a larger path than the 1024 we put on the stack.
		Allocate one for them.
	*/
	path		= MemMallocWait(++needed);
	*path		= '\0';

	/* Use a copy of the args to avoid modifying the consumer's data */
	va_copy(localargs, args);

	vstrcatf(path, needed, &needed, pathf, localargs);

	return(path);
}

EXPORT WJElement WJEGetF(WJElement container, WJElement last, const char *pathf, ...)
{
	WJElement	ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = _WJEGet(container, path, last, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT XplBool WJEBoolF(WJElement container, WJEAction action, WJElement *last, XplBool value, const char *pathf, ...)
{
	XplBool		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEBool(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT char * WJEStringF(WJElement container, WJEAction action, WJElement *last, const char *value, const char *pathf, ...)
{
	char *		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEString(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT char * WJEStringNF(WJElement container, WJEAction action, WJElement *last, const char *value, size_t len, const char *pathf, ...)
{
	char *		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEStringN(container, path, action, last, value, len, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT WJElement WJEObjectF(WJElement container, WJEAction action, WJElement *last, const char *pathf, ...)
{
	WJElement	ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEObject(container, path, action, last, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT WJElement WJEArrayF(WJElement container, WJEAction action, WJElement *last, const char *pathf, ...)
{
	WJElement	ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEArray(container, path, action, last, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT WJElement WJENullF(WJElement container, WJEAction action, WJElement *last, const char *pathf, ...)
{
	WJElement	ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJENull(container, path, action, last, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT int32 WJEInt32F(WJElement container, WJEAction action, WJElement *last, int32 value, const char *pathf, ...)
{
	int32		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEInt32(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT uint32 WJEUInt32F(WJElement container, WJEAction action, WJElement *last, uint32 value, const char *pathf, ...)
{
	uint32		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEUInt32(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT int64 WJEInt64F(WJElement container, WJEAction action, WJElement *last, int64 value, const char *pathf, ...)
{
	int64		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEInt64(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT uint64 WJEUInt64F(WJElement container, WJEAction action, WJElement *last, uint64 value, const char *pathf, ...)
{
	uint64		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEUInt64(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

EXPORT double WJEDoubleF(WJElement container, WJEAction action, WJElement *last, double value, const char *pathf, ...)
{
	double		ret;
	va_list		args;
	size_t		needed;
	char		*path;
	char		buffer[1024];

	va_start(args, pathf);

	path = buffer;
	path = WJEPathF(buffer, sizeof(buffer), pathf, args);

	va_end(args);

	ret = __WJEDouble(container, path, action, last, value, __FILE__, __LINE__);

	if (path != buffer) {
		MemRelease(&path);
	}

	return(ret);
}

