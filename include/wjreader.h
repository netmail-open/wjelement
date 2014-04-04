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


#ifndef WARP_JSON_READER_H
#define WARP_JSON_READER_H

#include <xpl.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
	WARP JSON Reader

	The WARP JSON Reader provides the ability to parse a JSON document as a
	stream.  All parsing is done with a portion of the document loaded into a
	buffer, and the buffer is reloaded as needed.

	This allows the parser to be very fast, but does require that data in the
	document is accessed in the order of the document.
*/

typedef struct {
	uint32					depth;
	uint32					maxdepth;
	void					*userdata;
} WJReaderPublic;
typedef WJReaderPublic *	WJReader;

typedef size_t				(* WJReadCallback)(char *data, size_t length, size_t seen, void *userdata);

/*
	Open a JSON document, and prepare to parse.  A callback function must be
	provided which will be used to read data as it is needed.	The callback is
	expected to load data into the provided buffer and return the number of
	bytes that where loaded.  If the number of bytes loaded is less than the
	requested length then it is assumed that the end of the document has been
	reached.

	If a buffer is provided to WJROpenDocument() then that buffer will be used,
	rather than allocating a new buffer.  The size of the buffer must be
	provided as buffersize.  If a buffer is not provided then one will be
	allocated of size buffersize.

	If a buffersize of 0 is passed to WJROpenDocument then the default size will
	be used (4KB).  Some data is kept in the buffer while parsing, such as
	element names and values.  If the buffer is not large enough for these then
	the document will fail to be parsed and will be aborted.
*/
EXPORT WJReader				_WJROpenDocument(WJReadCallback callback, void *userdata, char *buffer, size_t buffersize, uint32 maxdepth);
#define WJROpenDocument(c, u, b, s) \
							_WJROpenDocument((c), (u), (b), (s), 250)
EXPORT XplBool				WJRCloseDocument(WJReader doc);

/*
	Return a string, which contains the name of the next element of the
	specified parent, prefixed by a single character that represents the type.
	The pointer returned may be used as the parent argument in future calls to
	WJRNext() in order to return the children of the current object.

	If the object's name is larger than the maxnamelen argument then it will be
	truncated.
*/
typedef enum {
	WJR_TYPE_OBJECT			= 'O',
	WJR_TYPE_ARRAY			= 'A',

	WJR_TYPE_STRING			= 'S',
	WJR_TYPE_NUMBER			= 'N',

#ifdef WJE_DISTINGUISH_INTEGER_TYPE
	WJR_TYPE_INTEGER		= 'I',
#endif

	WJR_TYPE_BOOL			= 'B',
	WJR_TYPE_TRUE			= 'T',
	WJR_TYPE_FALSE			= 'F',
	WJR_TYPE_NULL			= '0',

	WJR_TYPE_UNKNOWN		= '?'
} WJRType;

EXPORT char *				WJRNext(char *parent, size_t maxnamelen, WJReader doc);

/*
	Return the value of the last object returned by WJRNext().	The calling
	application is expected to know the type of the value, and call the
	appropriate function.  When possible the value will be converted if it does
	not match the requested type.

	If the buffer is not large enough to contain the entire value then the
	WJRString() function may return a partial value.  It may be called multiple
	times until a NULL is returned to ensure that the entire value has been
	read.
*/
EXPORT char *	   			WJRStringEx(XplBool *complete, size_t *length, WJReader doc);
#define WJRString(c, d)		WJRStringEx((c), NULL, (d))
EXPORT XplBool				WJRBoolean(WJReader doc);

EXPORT int32				WJRInt32(WJReader doc);
EXPORT uint32				WJRUInt32(WJReader doc);
EXPORT int64				WJRInt64(WJReader doc);
EXPORT uint64				WJRUInt64(WJReader doc);
EXPORT double				WJRDouble(WJReader doc);

/*
	If the number type is not known, then attempt to read it as either a uint64
	or a double.

	If a decimal point is found in the number then TRUE will be returned and the
	value will be stored in *d. Otherwise FALSE will be returned and the value
	stored in *i.
*/
EXPORT XplBool				WJRIntOrDouble(WJReader doc, uint64 *i, double *d);

/*
	If the current element is a WJR_TYPE_NUMBER then this function can be used
	to determine if the value is negative or positive.  If negative then TRUE
	will be returned.
*/
EXPORT XplBool				WJRNegative(WJReader doc);


/*
	An alternative method of opening a JSON document, which will provide a
	proper callback for reading the data from an open FILE *.

	As with the standard WJROpenDocument() a buffersize of 0 will use the
	default buffer size.
*/
EXPORT size_t WJRFileCallback(char *buffer, size_t length, size_t seen, void *data);
#define WJROpenFILEDocument(jsonfile, buffer, buffersize) \
							WJROpenDocument(WJRFileCallback, (jsonfile), (buffer), (buffersize))

/*
	An alternative method of opening a JSON document, which will provide a
	proper callback for reading from a pre-allocated buffer in memory.

	As with the standard WJROpenDocument() a buffersize of 0 will use the
	default buffer size.
*/
EXPORT size_t				WJRMemCallback(char *buffer, size_t length, size_t seen, void *userdata);
#define WJROpenMemDocument(json, buffer, buffersize) \
							WJROpenDocument(WJRMemCallback, (json), (buffer), (buffersize))

#ifdef __cplusplus
}
#endif

#endif /* WARP_JSON_READER_H */

