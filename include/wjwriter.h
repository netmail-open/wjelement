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


#ifndef WARP_JSON_WRITER_H
#define WARP_JSON_WRITER_H

#include <xpl.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
	WARP JSON Writer

	The WARP JSON Writer provides the ability to easily write a JSON document to
	a stream.	All data formatting is done automatically.
*/
typedef size_t			(* WJWriteCallback)(char *data, size_t size, void *writedata);

typedef struct {
	XplBool				pretty;

	/*
		Base may be set to 8, 10 (default), or 16.  Using a base other than 10
		will result in a non standard JSON document which may not be read
		properly by all parsers.
	*/
	int					base;

	struct {
		void			*data;
		WJWriteCallback	cb;
	} write;

	struct {
		void			*data;
		void			(* freecb)(void *data);
	} user;
} WJWriterPublic;
typedef WJWriterPublic*	WJWriter;

/*
	Open a stream that is ready to accept a JSON document.	A callback function
	must be provided which will be used to write data as it is ready.

	A buffersize of 0 will disable write buffering entirely.  The write buffer
	used will be at least the size requested, but may be larger.
*/
#define WJWOpenDocument(p, c, d) _WJWOpenDocument((p), (c), (d), (3 * 1024))
EXPORT WJWriter			_WJWOpenDocument(XplBool pretty, WJWriteCallback callback, void *writedata, size_t buffersize);
EXPORT XplBool			WJWCloseDocument(WJWriter doc);

/*
	Open an array.	All objects that are direct children of the array MUST NOT
	be named.  A value of NULL should be passed as name for any such values.

	When all values of the array have been written it can be closed with
	WJWCloseArray().
*/
EXPORT XplBool			WJWOpenArray(char *name, WJWriter doc);
EXPORT XplBool			WJWCloseArray(WJWriter doc);

/*
	Open an object.  All objects that are direct children of the object MUST be
	named.	A value of NULL should NOT be passed as name for any such values.

	When all values of the object have been written it can be closed with
	WJWCloseObject().
*/
EXPORT XplBool			WJWOpenObject(char *name, WJWriter doc);
EXPORT XplBool			WJWCloseObject(WJWriter doc);

/*
	Write a value to the document.	If any values are written that are a direct
	child of an object then a non-NULL name MUST be provided.  If any values are
	written that are a direct child of an array then a non-NULL name MUST NOT be
	provided.
*/
EXPORT XplBool			WJWStringN(char *name, char *value, size_t length, XplBool done, WJWriter doc);
EXPORT XplBool			WJWString(char *name, char *value, XplBool done, WJWriter doc);
EXPORT XplBool			WJWBoolean(char *name, XplBool value, WJWriter doc);
EXPORT XplBool			WJWNull(char *name, WJWriter doc);

EXPORT XplBool			WJWInt32(char *name, int32 value, WJWriter doc);
EXPORT XplBool			WJWUInt32(char *name, uint32 value, WJWriter doc);
EXPORT XplBool			WJWInt64(char *name, int64 value, WJWriter doc);
EXPORT XplBool			WJWUInt64(char *name, uint64 value, WJWriter doc);
EXPORT XplBool			WJWDouble(char *name, double value, WJWriter doc);

/*
	Write a raw pre-formatted value to the document.  It is up to the caller to
	ensure that the data is properly formatted and complete.
*/
EXPORT XplBool			WJWRawValue(char *name, char *value, XplBool done, WJWriter doc);

/*
	An alternative method of opening a JSON document for writing, which will
	provide a proper callback for writing the data to an open FILE *.
*/
EXPORT size_t			WJWFileCallback(char *buffer, size_t length, void *data);
#define WJWOpenFILEDocument(pretty, file) _WJWOpenDocument((pretty), WJWFileCallback, (file), 0)

#ifdef __cplusplus
}
#endif

#endif /* WARP_JSON_WRITER_H */

