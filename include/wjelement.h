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


#ifndef WARP_JSON_ELEMENT_H
#define WARP_JSON_ELEMENT_H

#include <wjreader.h>
#include <wjwriter.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
	WJElement Interfaces

	The WJElement interfaces allow you to load an entire JSON document into
	memory and to then access elements within it more easily.

	Elements within the hierarchy of a JSON document can be referenced using a
	path.  Multiple levels of hierarchy can be referenced to find any element
	below the provided container.  The following rules apply to any WJE
	functions that take a path argument.

	A child may be referenced with a alpha-numeric name, or a subscript within
	square brackets:
		foo
		["foo"]

	Additional levels of heirarchy can be referenced by appending an additional
	subscript, or appending a dot and an additional alpha-numeric name:
		one.two.three.four
		one["two"]["three"].four

	Subscripts may contain double quoted names.  Any special characters,
	(including .[]*?"'\) can be included by prefixing with a \.
		foo["bar.smeg"]
		foo["something with a \"quote\""]

	Subscripts may contain single quoted names, which behave as double quoted
	names but also allow for * and ? wild card substitution:
		foo['bar.*']

	Subscripts may reference an item by it's offset in an array (or object):
		foo[0]
		foo[3]

	Negative offsets are wrapped to the end of the array (or object) meaning
	that [-1] references the last item.

	Subscripts may reference a range of offsets by providing 2 offsets seperated
	by a colon:
		foo[2:5]

	Subscripts may reference a set of items by seperating offsets, offset,
	ranges, double quoted and single quoted values:
		foo[2,4:6,'bar.*', "smeg"]

	An empty subscript may be specified to reference all children.
		[]

	A subscript of $ may be specified in actions perform creations to reference
	the item after the end of an array.  This allows appending to an array.
		[$]

	A subscript may be prefixed by an | character to indicate that the subscript
	is optional. The portion of the selector from the | on will be ignored if
	there is a match and that match has no children.

	For example, if an element named foo may either be a string or an array of
	strings you can enumerate all values using a selector of:
		foo|[]

	A NULL path refers to the container itself.

	A path may end in a condition.  The condition consists of an operator and a
	value.  The value may be a number, a double quoted string, or a single
	quoted string.  If a single quoted string is used it may contain * and ?
	wild cards.

	The following operators are supported for any value:
		==, !=

	A number value may also use the following operators:
		<, >, <=, >=

	Example:
		foo.bar <= 3
		foo.bar != 'foo*'
		foo.bar != "one"

	A condition may be separated from a path with a ; character.

	In the following examples the object named "foo" will be returned if it has
	a child named "bar" that matches the condition.
		foo; bar <= 3
		foo; bar != 'foo*'
		foo; bar != "one"

*/

typedef struct WJElementPublic
{
	char							*name;
	WJRType							type;

	struct WJElementPublic			*next;
	struct WJElementPublic			*prev;

	struct WJElementPublic			*child;
	struct WJElementPublic			*parent;

	/* The number of children */
	int								count;

	/* The length if the type is WJR_TYPE_STRING */
	size_t							length;

	/*
		A count of changes that have been performed on this element, which can
		be reset by the consumer.
	*/
	int								changes;

	void							*client;

	/*
		If set then this freecb will be called before actually free'ing a
		WJElement.  If it returns FALSE then the WJElement will NOT be free'd.

		This can be used to allow caching of objects.  If used in this way then
		the consumer that set the callback is responsible for ensuring that it
		does get free'd correctly at the correct time.
	*/
	XplBool							(* freecb)(struct WJElementPublic *);
} WJElementPublic;
typedef WJElementPublic *			WJElement;

/*
	Parse the provided JSON document and return the new WJElement

	The _WJEParse version can be used to parse a document with a non-standard
	quote character. This allows easy parsing of simple documents directly in a
	C source file without having to escape double quote characters. Example:
		doc = _WJEParse("{ 'foo': true, 'bar': 'yup' }", '\'');
*/
#define				WJEParse(j) _WJEParse((j), '"')
EXPORT WJElement	_WJEParse(const char *json, char quote);

/*
	Load a WJElement object from the provided WJReader

	If a load callback is provided then it will be called before adding any new
	children, allowing the consumer to leave ignore specific elements of the
	hierarchy.
*/
typedef XplBool		(* WJELoadCB)(WJElement parent, char *path, void *data, const char *file, const int line);
EXPORT WJElement	_WJEOpenDocument(WJReader reader, char *where, WJELoadCB loadcb, void *data, const char *file, const int line);
#define				WJEOpenDocument(r, w, lcb, d) _WJEOpenDocument((r), (w), (lcb), (d), __FILE__, __LINE__)

/* Write a WJElement object to the provided WJWriter */
typedef XplBool		(* WJEWriteCB)(WJElement node, WJWriter writer, void *data);
EXPORT XplBool		_WJEWriteDocument(WJElement document, WJWriter writer, char *name,
						WJEWriteCB precb, WJEWriteCB postcb, void *data);
#define				WJEWriteDocument(d, w, n) _WJEWriteDocument((d), (w), (n), NULL, NULL, NULL)
/* Write a WJElement object to the provided FILE* */
EXPORT void WJEWriteFILE(WJElement document, FILE* fd);
/* Allocate and write a string (maxlength 0 = unlimited; remember to MemFree) */
EXPORT char * WJEWriteMEM(WJElement document, XplBool pretty, size_t maxlength);

/* Destroy a WJElement object */
EXPORT XplBool		WJECloseDocument(WJElement document);
/*
	WJECloseDocument is also used to delete/remove an item from a parent
	document:
	WJECloseDocument(WJEGet(...));
*/

/* Duplicate an existing WJElement */
typedef XplBool		(* WJECopyCB)(WJElement destination, WJElement object, void *data, const char *file, const int line);
EXPORT WJElement	_WJECopyDocument(WJElement to, WJElement from, WJECopyCB loadcb, void *data, const char *file, const int line);
#define				WJECopyDocument(t, f, lcb, d) _WJECopyDocument((t), (f), (lcb), (d), __FILE__, __LINE__)


/* Remove a WJElement from it's parent (and siblings) */
EXPORT XplBool		_WJEDetach(WJElement document, const char *file, const int line);
#define WJEDetach( d )	_WJEDetach( (d), __FILE__, __LINE__ )

/* Add a document to another document as a child */
EXPORT XplBool		WJEAttach(WJElement container, WJElement document);

/* Rename an element */
EXPORT XplBool		WJERename(WJElement document, const char *name);

/* Merge all fields from one object to another */
EXPORT XplBool		WJEMergeObjects(WJElement to, WJElement from, XplBool overwrite);

/*
	Find the first element within the hierarchy of a WJElement that matches the
	specified path.

	If 'last' is non-NULL then the next match will be returned instead, allowing
	enumeration of multiple matching elements.
*/
EXPORT WJElement	_WJEGet(WJElement container, char *path, WJElement last, const char *file, const int line);
#define WJEGet( c, p, l )	_WJEGet( (c), (p), (l), __FILE__, __LINE__ )

typedef enum {
	/*
		Return the value of an element.  If the element does not exist then the
		provided default value will be returned.
	*/
	WJE_GET = 0,

	/*
		Assign the specified value to an element.  If the element does not exist
		then it will be created.

		If the element can not be created then an appropriate value will be
		returned to indicate the error.

		When applicable a NULL will be returned.  Otherwise a value that does
		not match the requested value will be returned.
	*/
	WJE_SET,

	/*
		Create a new element and assign it the specified value.

		If an element already exists then it will not be modified, and the value
		of that existing element will be returned.
	*/
	WJE_NEW,

	/*
		Assign the specified value to an existing element, and return the value
		if successful.

		If the element does not exist then no elements are created or modified.
		When applicable a NULL will be returned.  Otherwise a value that does
		not match the requested value will be returned.
	*/
	WJE_MOD
} WJEAction;

/* Left for backwards compatability. WJE_PUT was renamed to WJE_MOD */
#define WJE_PUT WJE_MOD

#define _WJEString(	container, path, action, last,	value)		__WJEString(	(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEString(	container, path, action,		value)		__WJEString(	(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJEStringN(container, path, action, last,	value, len)	__WJEStringN(	(container), (path), (action), (last),	(value), (len),	__FILE__, __LINE__)
#define  WJEStringN(container, path, action,		value, len)	__WJEStringN(	(container), (path), (action), NULL,	(value), (len),	__FILE__, __LINE__)
#define _WJEObject(	container, path, action, last)				__WJEObject(	(container), (path), (action), (last),					__FILE__, __LINE__)
#define  WJEObject(	container, path, action)					__WJEObject(	(container), (path), (action), NULL,					__FILE__, __LINE__)
#define _WJEBool(	container, path, action, last,	value)		__WJEBool(		(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEBool(	container, path, action,		value)		__WJEBool(		(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJEArray(	container, path, action, last)				__WJEArray(		(container), (path), (action), (last),					__FILE__, __LINE__)
#define  WJEArray(	container, path, action)					__WJEArray(		(container), (path), (action), NULL,					__FILE__, __LINE__)
#define _WJENull(	container, path, action, last)				__WJENull(		(container), (path), (action), (last),					__FILE__, __LINE__)
#define  WJENull(	container, path, action)					__WJENull(		(container), (path), (action), NULL,					__FILE__, __LINE__)
#define _WJEInt32(	container, path, action, last,	value)		__WJEInt32(		(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEInt32(	container, path, action,		value)		__WJEInt32(		(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJENumber(	container, path, action, last,	value)		__WJEInt32(		(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJENumber(	container, path, action,		value)		__WJEInt32(		(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJEUInt32(	container, path, action, last,	value)		__WJEUInt32(	(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEUInt32(	container, path, action,		value)		__WJEUInt32(	(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJEInt64(	container, path, action, last,	value)		__WJEInt64(		(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEInt64(	container, path, action,		value)		__WJEInt64(		(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJEUInt64(	container, path, action, last,	value)		__WJEUInt64(	(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEUInt64(	container, path, action,		value)		__WJEUInt64(	(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)
#define _WJEDouble( container, path, action, last,	value)		__WJEDouble(	(container), (path), (action), (last),	(value),		__FILE__, __LINE__)
#define  WJEDouble( container, path, action,		value)		__WJEDouble(	(container), (path), (action), NULL,	(value),		__FILE__, __LINE__)

EXPORT XplBool		__WJEBool(		WJElement container, const char *path, WJEAction action, WJElement *last, XplBool value,				const char *file, const int line);
EXPORT char *		__WJEString(	WJElement container, const char *path, WJEAction action, WJElement *last, const char *value,			const char *file, const int line);
EXPORT char *		__WJEStringN(	WJElement container, const char *path, WJEAction action, WJElement *last, const char *value, size_t len,const char *file, const int line);
EXPORT WJElement	__WJEObject(	WJElement container, const char *path, WJEAction action, WJElement *last,								const char *file, const int line);
EXPORT WJElement	__WJEArray(		WJElement container, const char *path, WJEAction action, WJElement *last,								const char *file, const int line);
EXPORT WJElement	__WJENull(		WJElement container, const char *path, WJEAction action, WJElement *last,								const char *file, const int line);
EXPORT int32		__WJEInt32(		WJElement container, const char *path, WJEAction action, WJElement *last, int32 value,					const char *file, const int line);
EXPORT uint32		__WJEUInt32(	WJElement container, const char *path, WJEAction action, WJElement *last, uint32 value,					const char *file, const int line);
EXPORT int64		__WJEInt64(		WJElement container, const char *path, WJEAction action, WJElement *last, int64 value,					const char *file, const int line);
EXPORT uint64		__WJEUInt64(	WJElement container, const char *path, WJEAction action, WJElement *last, uint64 value,					const char *file, const int line);
EXPORT double		__WJEDouble(	WJElement container, const char *path, WJEAction action, WJElement *last, double value,					const char *file, const int line);

/*
	The following functions are identical to the non F variants except that the
	path argument has been moved to the end and is used as a format string. For
	example if you need a value by index, and that index is stored in the
	variable x:
		val = WJENumberF(doc, WJE_GET, NULL, 0, "foo[%d]", x);
*/
EXPORT WJElement	WJEGetF(	WJElement container, WJElement last,													const char *pathf, ...) XplFormatString(3, 4);
EXPORT XplBool		WJEBoolF(	WJElement container, WJEAction action, WJElement *last, XplBool value,					const char *pathf, ...) XplFormatString(5, 6);
EXPORT char *		WJEStringF(	WJElement container, WJEAction action, WJElement *last, const char *value,				const char *pathf, ...) XplFormatString(5, 6);
EXPORT char *		WJEStringNF(WJElement container, WJEAction action, WJElement *last, const char *value, size_t len,	const char *pathf, ...) XplFormatString(6, 7);
EXPORT WJElement	WJEObjectF(	WJElement container, WJEAction action, WJElement *last,									const char *pathf, ...) XplFormatString(4, 5);
EXPORT WJElement	WJEArrayF(	WJElement container, WJEAction action, WJElement *last,									const char *pathf, ...) XplFormatString(4, 5);
EXPORT WJElement	WJENullF(	WJElement container, WJEAction action, WJElement *last,									const char *pathf, ...) XplFormatString(4, 5);
EXPORT int32		WJEInt32F(	WJElement container, WJEAction action, WJElement *last, int32 value,					const char *pathf, ...) XplFormatString(5, 6);
EXPORT uint32		WJEUInt32F(	WJElement container, WJEAction action, WJElement *last, uint32 value,					const char *pathf, ...) XplFormatString(5, 6);
EXPORT int64		WJEInt64F(	WJElement container, WJEAction action, WJElement *last, int64 value,					const char *pathf, ...) XplFormatString(5, 6);
EXPORT uint64		WJEUInt64F(	WJElement container, WJEAction action, WJElement *last, uint64 value,					const char *pathf, ...) XplFormatString(5, 6);
EXPORT double		WJEDoubleF(	WJElement container, WJEAction action, WJElement *last, double value,					const char *pathf, ...) XplFormatString(5, 6);



/*
	Find, create or update an element by name instead of path.  This allows
	access to elements that would be difficult to reference by path.

	Type specific actions may be done by passing the resulting WJElement and a
	NULL path to WJEBool(), WJENumber(), WJEString(), WJEObject(), WJEArray() or
	WJENull().
*/
EXPORT WJElement _WJEChild(WJElement container, char *name, WJEAction action, const char *file, const int line);
#define WJEChild(c, n, a) _WJEChild((c), (n), (a), __FILE__, __LINE__)

/* Calculate a hash for a document */
typedef int (* WJEHashCB)(void *context, void *data, size_t size);
EXPORT void WJEHash(WJElement document, WJEHashCB update, void *context);

/* WJElement Schema-related stuff */
/*
  Validate or find selectors according to schema.
  WJESchemaLoadCB callbacks are used to fetch schema as needed.
  WJESchemaFreeCB are called when schema is no longer needed.
 */

typedef WJElement (* WJESchemaLoadCB)(const char *name, void *client, const char *file, const int line);
typedef void (* WJESchemaFreeCB)(WJElement schema, void *client);
typedef void (* WJESchemaMatchCB)(WJElement schema, const char *selector, void *client);
typedef void (* WJEErrCB)(void *client, const char *format, ...);

/*
  Validate a document against a given schema.  Additional schema will be loaded
  via the load callback if needed.  Any validation errors will be reported,
  printf-style, to errcb.
 */
EXPORT XplBool WJESchemaValidate(WJElement schema, WJElement document,
								 WJEErrCB err, WJESchemaLoadCB load,
								 WJESchemaFreeCB freecb, void *client);

/*
  Determine if a document does or does not implement a specific schema.
  Additional schema will be loaded via the load callback if needed.

  If a load callback is not provided then the object type will still be checked
  but it will not be considered a match if it is a type that extends the
  specifed type.
 */
EXPORT XplBool WJESchemaIsType(WJElement document, const char *type,
							   WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
							   void *client);
/*
  variation of WJESchemaIsType which acts on a schema name instead of a document
 */
EXPORT XplBool WJESchemaNameIsType(const char *describedby, const char *type,
								   WJESchemaLoadCB loadcb,
								   WJESchemaFreeCB freecb, void *client);
/*
  calls back matchcb for each WJElement selector which will fetch a property
  of a given type and format, from a given document.  The load callback will be
  used to load all necessary schema, starting with the document's "describedby".
  stripat-type wildcards may be used; "Date*" will find "date" and "date-time".
 */
EXPORT void WJESchemaGetSelectors(WJElement document,
								  char *type, char *format,
								  WJESchemaLoadCB load,
								  WJESchemaFreeCB freecb,
								  WJESchemaMatchCB matchcb, void *client);
/*
  variation of WJESchemaGetSelectors which provides selectors that _could_ exist
  in objects of the given "describedby" schema name
*/
EXPORT void WJESchemaGetAllSelectors(char *describedby,
									 char *type, char *format,
									 WJESchemaLoadCB load,
									 WJESchemaFreeCB freecb,
									 WJESchemaMatchCB matchcb, void *client);


EXPORT char * WJESchemaNameFindBacklink(char *describedby, const char *format,
								 WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								 void *client);

EXPORT char * WJESchemaFindBacklink(WJElement document, const char *format,
								 WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								 void *client);

EXPORT void WJESchemaFreeBacklink(char *backlink);


/* Debug function that will write a document to stdout */
EXPORT void WJEDump(WJElement document);

/* For compatibility with old code, due to a typo */
#define WJEDettach( d )	_WJEDetach( (d), __FILE__, __LINE__ )

#ifdef __cplusplus
}
#endif

#endif /* WARP_JSON_ELEMENT_H */
