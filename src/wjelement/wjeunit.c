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


#include <xpl.h>
#include <hulautil.h>
#include <memmgr.h>

#include <wjreader.h>
#include <wjwriter.h>
#include <wjelement.h>

/*
	The JSON document below will be provided to each of the test functions.

	Note that it is actually not valid JSON because it uses single quotes, but
	the loader will convert all single quotes into double quotes before parsing
	the document.  This makes it much easier to see in the C source file.
*/
char *json = \
"{																			\n"
"	'one':1, 'two':2, 'three':3,											\n"
"	'digits':[ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ],								\n"
"	'bools':[ true, false ],												\n"
"																			\n"
"	'a':{																	\n"
"		'ant':true, 'aardvark':true, 'balloon':false, 'car':false,			\n"
"		'names':[ 'a' ],													\n"
"		'b':{																\n"
"			'ant':false, 'aardvark':false, 'balloon':true, 'car':false,		\n"
"			'names':[ 'a', 'b' ],											\n"
"			'c':{															\n"
"				'ant':false, 'aardvark':false, 'balloon':false, 'car':true, \n"
"				'names':[ 'a', 'b', 'c' ],									\n"
"				'd':{														\n"
"					'names':[ 'a', 'b', 'c', 'd' ],							\n"
"					'e':{													\n"
"						'names':[ 'a', 'b', 'c', 'd', 'e' ],				\n"
"						'f':{												\n"
"							'names':[ 'a', 'b', 'c', 'd', 'e', 'f' ]		\n"
"						}													\n"
"					}														\n"
"				}															\n"
"			}																\n"
"		}																	\n"
"	},																		\n"
"	'space balls': {														\n"
"		'the movie': true,													\n"
"		'the toliet paper': true,											\n"
"		'the what ': [ false ]												\n"
"	},																		\n"
"	'strings':[																\n"
"		'This is some text',												\n"
"		'and so is this',													\n"
"		'and this',															\n"
"	],																		\n"
"	'string': 'This is a single string',									\n"
"	'sender':{																\n"
"		'address':'foo@bar.com'												\n"
"	},																		\n"
"	'empty': null															\n"
"}";

typedef int (* wjetest)(WJElement doc);

static int SelfTest(WJElement doc)
{
	WJElement		a, b;

	/* Tests methods of refering to "self" */
	if (doc != WJEGet(doc, NULL,	NULL))					return(__LINE__);
	if (doc != WJEGet(doc, "",		NULL))					return(__LINE__);
	if (doc != WJEGet(doc, ".",		NULL))					return(__LINE__);

	/*
		Often in code a WJElement is accessed like this:
			if ((msg->extra = WJEObject(msg->extra, NULL, WJE_NEW))) {
				...

		The idea is that since passing a NULL refers to self that msg->extra
		will be returned if it exists, and created if it does not.	Since this
		is such a common case, verify it's behavior properly.
	*/
	if (!(a = WJEObject(NULL,	NULL, WJE_NEW)))			return(__LINE__);
	if (!(b = WJEObject(a,		NULL, WJE_NEW)))			return(__LINE__);
	if (a != b)												return(__LINE__);

	return(0);
}

/* Various methods of referencing an element by name */
static int NamesTest(WJElement doc)
{
	WJElement		e;

	/* The main doc can't have a name, since it has no parent */
	if (doc->name)											return(__LINE__);

	/* Various ways of referencing the same element */
	if (!(e = WJEChild(doc, "one", WJE_GET)))				return(__LINE__);
	if (e != WJEGet(doc, "one",			NULL))				return(__LINE__);
	if (e != WJEGet(doc, "[\"one\"]",	NULL))				return(__LINE__);
	if (e != WJEGet(doc, "[\'one\']",	NULL))				return(__LINE__);

	return(0);
}

/* Test basic use of offsets */
static int OffsetTest(WJElement doc)
{
	WJElement		e, l;

	/* Get the first element */
	if (!(e = WJEGet(doc, "digits[0]", NULL)))				return(__LINE__);
	if (e->prev)											return(__LINE__);
	if (WJEGet(doc, "digits[0]", e))						return(__LINE__);

	/* Get the last element */
	if (!(e = WJEGet(doc, "digits[-1]", NULL)))				return(__LINE__);
	if (e->next)											return(__LINE__);
	if (WJEGet(doc, "digits[-1]", e))						return(__LINE__);

	/* Get an element in the middle */
	if (!(e = WJEGet(doc, "digits[5]", NULL)))				return(__LINE__);
	if (!e->next || !e->prev)								return(__LINE__);
	if (WJEGet(doc, "digits[5]", e))						return(__LINE__);

	/* Grab the last item, then append a new item and verify the change */
	if (!(e = WJEGet(doc, "digits[-1]", NULL)))				return(__LINE__);
	l = NULL;
	if (-1 != _WJENumber(doc, "digits[$]", WJE_NEW, &l, -1))return(__LINE__);
	if (!l || l->next || l->prev != e || e->next != l)		return(__LINE__);

	return(0);
}

/* Test basic use of lists */
static int ListTest(WJElement doc)
{
	WJElement		t, e;

	/* Every element */
	if (!(t = WJEGet(doc, "digits[0]", NULL)))				return(__LINE__);
	e = NULL;
	while ((e = WJEGet(doc, "digits[0,1,2,3,4,5,6,7,8,9]", e))) {
		if (e != t)											return(__LINE__);
		t = t->next;
	}
	if (t)													return(__LINE__);


	/*
		Every other element.  Note that order in the query does not effect the
		order in which the results are returned.
	*/
	if (!(t = WJEGet(doc, "digits[1]", NULL)))				return(__LINE__);
	e = NULL;
	while ((e = WJEGet(doc, "digits[1,7,5,3]", e))) {
		if (!t || e != t)									return(__LINE__);
		t = t->next->next;
	}

	return(0);
}

static int PathsTest(WJElement doc)
{
	WJElement		e;

	/* Compare multiple methods of referencing deep elements */
	if ((e = WJEGet(doc, "a.b.c.d.e.f", NULL)) == NULL)		return(__LINE__);

	if (e != WJEGet(doc, "a[\"b\"].c.d.e.f",	NULL))		return(__LINE__);
	if (e != WJEGet(doc, "a['b'].c.d.e.f",		NULL))		return(__LINE__);
	if (e != WJEGet(doc, "a.b[\"c\"].d.e.f",	NULL))		return(__LINE__);
	if (e != WJEGet(doc, "a.b['c'].d.e.f",		NULL))		return(__LINE__);
	if (e != WJEGet(doc, "a['b'].c['d'].e.f",	NULL))		return(__LINE__);
	if (e != WJEGet(doc, "a['b']['c']['d'].e.f",NULL))		return(__LINE__);

	/* Do the same thing with paths that don't exist yet */
	if (!(e = WJEObject(doc, "z[\"y\"].x.w.v.u",	WJE_NEW)) ||
		!WJECloseDocument(e))								return(__LINE__);
	if (!(e = WJEObject(doc, "z['y'].x.w.v.u",		WJE_NEW)) ||
		!WJECloseDocument(e))								return(__LINE__);
	if (!(e = WJEObject(doc, "z.y[\"x\"].w.v.u",	WJE_NEW)) ||
		!WJECloseDocument(e))								return(__LINE__);
	if (!(e = WJEObject(doc, "z.y['x'].w.v.u",		WJE_NEW)) ||
		!WJECloseDocument(e))								return(__LINE__);
	if (!(e = WJEObject(doc, "z['y'].x['w'].v.u",	WJE_NEW)) ||
		!WJECloseDocument(e))								return(__LINE__);
	if (!(e = WJEObject(doc, "z['y']['x']['w'].v.u",WJE_NEW)) ||
		!WJECloseDocument(e))								return(__LINE__);

	/* Test paths that contain spaces, and dashes */
	if (!WJEGet(doc, "space balls", NULL))					return(__LINE__);
	if (!WJEGet(doc, "space balls.the toliet paper", NULL))	return(__LINE__);
	if ( WJEGet(doc, "space balls.the what ", NULL))		return(__LINE__);
	if (!WJEGet(doc, "space balls.the what []", NULL))		return(__LINE__);

	return(0);
}

static int GetValueTest(WJElement doc)
{
	char	*v;

	if (1 != WJENumber(doc, "one",			WJE_GET, -1))	return(__LINE__);
	if (2 != WJENumber(doc, "two",			WJE_GET, -1))	return(__LINE__);
	if (3 != WJENumber(doc, "three",		WJE_GET, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "four",			WJE_GET, -1))	return(__LINE__);

	if (0 != WJENumber(doc, "digits[0]",	WJE_GET, -1))	return(__LINE__);
	if (1 != WJENumber(doc, "digits[1]",	WJE_GET, -1))	return(__LINE__);
	if (9 != WJENumber(doc, "digits[-1]",	WJE_GET, -1))	return(__LINE__);
	if (9 != WJENumber(doc, "digits[$]",	WJE_GET, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "digits[10]",	WJE_GET, -1))	return(__LINE__);


	if (!WJEBool(doc, "a.b.balloon",		WJE_GET, 0))	return(__LINE__);
	if ( WJEBool(doc, "a.b.aardvark",		WJE_GET, 1))	return(__LINE__);
	if (!WJEBool(doc, "a.b.bull",			WJE_GET, 1))	return(__LINE__);
	if ( WJEBool(doc, "a.b.another",		WJE_GET, 0))	return(__LINE__);

	if (!(v = WJEString(doc, "a.names[-1]",		WJE_GET, NULL)) ||
		stricmp(v, "a"))									return(__LINE__);
	if (!(v = WJEString(doc, "a.b.names[-1]",	WJE_GET, NULL)) ||
		stricmp(v, "b"))									return(__LINE__);
	if ( (v = WJEString(doc, "a.b.nonames[-1]",	WJE_GET, NULL)))
															return(__LINE__);

	return(0);
}

static int GetValuesTest(WJElement doc)
{
	char		*v;
	WJElement	e;
	int			r, c;

	/* Walk through digits 0-9 */
	e = NULL; c = 0;
	while (-1 != (r = _WJENumber(doc, "digits[]", WJE_GET, &e, -1)) && e) {
		if (r != c++)										return(__LINE__);
	}
	if (r != -1 || e || c != 10)							return(__LINE__);

	/* Walk through digits, and the bools.	Multi level enumeration. */
	e = NULL; c = 0;
	while (-1 != (r = _WJENumber(doc, "['digits', 'bools'][]", WJE_GET, &e, -1)) && e) {
		if (c == 10 &&	r) r = c;
		if (c == 11 && !r) r = c;

		if (r != c++)										return(__LINE__);
	}
	if (r != -1 || e || c != 12)							return(__LINE__);

	/* Walk through strings, "a" through "f" */
	e = NULL; c = 0;
	while ((v = _WJEString(doc, "a.b.c.d.e.f.names[]", WJE_GET, &e, NULL)) && e) {
		if (('a' + c++) != *v || *(v + 1))					return(__LINE__);
	}
	if (v || e || c != 6)									return(__LINE__);

	return(0);
}

static int SetValueTest(WJElement doc)
{
	char	*v;

	/*
		WJE_SET will update the value if it already exists, or create the
		element if it doesn't exist.

		Do a WJE_SET on values that do and do not already exist, and verify the
		result.  Then do a WJE_GET to verify that the element does exist.
	*/

	/* Replace existing number values */
	if (-1!= WJENumber(doc, "one",			WJE_SET, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "one",			WJE_GET,  1))	return(__LINE__);
	if (-1!= WJENumber(doc, "two",			WJE_SET, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "two",			WJE_GET,  2))	return(__LINE__);
	if (-1!= WJENumber(doc, "three",		WJE_SET, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "three",		WJE_GET,  3))	return(__LINE__);

	/* Insert a new number value */
	if (-1!= WJENumber(doc, "four",			WJE_SET, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "four",			WJE_GET,  4))	return(__LINE__);

	/* Replace existing boolean values */
	if ( WJEBool(doc, "a.b.balloon",		WJE_SET, 0))	return(__LINE__);
	if ( WJEBool(doc, "a.b.balloon",		WJE_GET, 1))	return(__LINE__);
	if (!WJEBool(doc, "a.b.aardvark",		WJE_SET, 1))	return(__LINE__);
	if (!WJEBool(doc, "a.b.aardvark",		WJE_GET, 0))	return(__LINE__);

	/* Insert a new boolean value */
	if (!WJEBool(doc, "a.b.bull",			WJE_SET, 1))	return(__LINE__);
	if (!WJEBool(doc, "a.b.bull",			WJE_GET, 0))	return(__LINE__);

	/* Replace an existing string value */
	if (!(v = WJEString(doc, "a.names[-1]",	WJE_SET, "aardvark")))
															return(__LINE__);
	if (!(v = WJEString(doc, "a.names[-1]",	WJE_GET, NULL)) ||
		stricmp(v, "aardvark"))
															return(__LINE__);

	/* Insert a new string value */
	if (!(v = WJEString(doc, "a.x.q",		WJE_SET, "queen")))
															return(__LINE__);
	if (!(v = WJEString(doc, "a.x.q",		WJE_GET, NULL)) ||
		stricmp(v, "queen"))
															return(__LINE__);
	return(0);
}

static int NewValueTest(WJElement doc)
{
	char	*v;

	/*
		WJE_NEW will not leave the original value if it already exists, or
		create the element if it doesn't.

		Do a WJE_NEW on values that do and do not already exist, and verify the
		result.  Then do a WJE_GET to verify that the creation of elements
		actually worked.
	*/

	/* Replace existing number values */
	if ( 1!= WJENumber(doc, "one",			WJE_NEW, -1))	return(__LINE__);
	if ( 2!= WJENumber(doc, "two",			WJE_NEW, -1))	return(__LINE__);
	if ( 3!= WJENumber(doc, "three",		WJE_NEW, -1))	return(__LINE__);

	/* Insert a new number value */
	if (-1!= WJENumber(doc, "four",			WJE_NEW, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "four",			WJE_GET,  4))	return(__LINE__);

	/* Replace existing boolean values */
	if (!WJEBool(doc, "a.b.balloon",		WJE_NEW, 0))	return(__LINE__);
	if ( WJEBool(doc, "a.b.aardvark",		WJE_NEW, 1))	return(__LINE__);

	/* Insert a new boolean value */
	if (!WJEBool(doc, "a.b.bull",			WJE_NEW, 1))	return(__LINE__);
	if (!WJEBool(doc, "a.b.bull",			WJE_GET, 0))	return(__LINE__);

	/* Replace an existing string value */
	if (!(v = WJEString(doc, "a.names[-1]",	WJE_NEW, "aardvark")) ||
		stricmp(v, "a"))
															return(__LINE__);
	if (!(v = WJEString(doc, "a.names[-1]",	WJE_GET, NULL)) ||
		stricmp(v, "a"))
															return(__LINE__);

	/* Insert a new string value */
	if (!(v = WJEString(doc, "a.x.q",		WJE_NEW, "queen")) ||
		stricmp(v, "queen"))
															return(__LINE__);
	if (!(v = WJEString(doc, "a.x.q",		WJE_GET, NULL)) ||
		stricmp(v, "queen"))
															return(__LINE__);
	return(0);
}

static int PutValueTest(WJElement doc)
{
	char	*v;

	/*
		WJE_PUT will update the value if the element exists.  If it does not
		exist then it will NOT create it.

		Do a WJE_PUT on values that do and do not already exist, and verify the
		result.  Then use WJEGet() to verify that new elements where not
		created.
	*/

	/* Replace existing number values */
	if (-1!= WJENumber(doc, "one",			WJE_PUT, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "one",			WJE_GET,  1))	return(__LINE__);
	if (-1!= WJENumber(doc, "two",			WJE_PUT, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "two",			WJE_GET,  2))	return(__LINE__);
	if (-1!= WJENumber(doc, "three",		WJE_PUT, -1))	return(__LINE__);
	if (-1!= WJENumber(doc, "three",		WJE_GET,  3))	return(__LINE__);

	/* Insert a new number value */
	if (-1== WJENumber(doc, "four",			WJE_PUT, -1))	return(__LINE__);
	if (WJEGet(doc, "four", NULL))							return(__LINE__);

	/* Replace existing boolean values */
	if ( WJEBool(doc, "a.b.balloon",		WJE_PUT, 0))	return(__LINE__);
	if (!WJEBool(doc, "a.b.aardvark",		WJE_PUT, 1))	return(__LINE__);

	/* Insert a new boolean value */
	if ( WJEBool(doc, "a.b.bull",			WJE_PUT, 1))	return(__LINE__);
	if (WJEGet(doc, "a.b.bull", NULL))						return(__LINE__);

	/* Replace an existing string value */
	if (!(v = WJEString(doc, "a.names[-1]",	WJE_PUT, "aardvark")) ||
		stricmp(v, "aardvark"))
															return(__LINE__);
	if (!(v = WJEString(doc, "a.names[-1]",	WJE_GET, NULL)) ||
		stricmp(v, "aardvark"))
															return(__LINE__);

	/* Insert a new string value */
	if ((v = WJEString(doc, "a.x.q",		WJE_PUT, "queen")))
															return(__LINE__);
	if (WJEGet(doc, "a.x.q", NULL))							return(__LINE__);
	return(0);
}

/* Test appending elements to arrays */
static int AppendTest(WJElement doc)
{
	WJElement		e;

	/* Create a new array and append an item to it */
	e = NULL;
	if (1 != _WJENumber(doc, "foo.bar.smeg[$]", WJE_NEW, &e, 1) || !e ||
		!e->parent || e->parent->type != WJR_TYPE_ARRAY)	return(__LINE__);

	/* Append an item onto an existing array */
	e = NULL;
	if (2 != _WJENumber(doc, "foo.bar.smeg[$]", WJE_NEW, &e, 2) || !e ||
		!e->parent || e->parent->type != WJR_TYPE_ARRAY)	return(__LINE__);

	/* Append an item to an existing array by offset */
	e = NULL;
	if (3 != _WJENumber(doc, "foo.bar.smeg[2]", WJE_NEW, &e, 3) || !e ||
		!e->parent || e->parent->type != WJR_TYPE_ARRAY)	return(__LINE__);



	/* Create a new array and assign an item to the first spot */
	e = NULL;
	if (1 != _WJENumber(doc, "foo.smeg.bar[0]", WJE_NEW, &e, 1) || !e ||
		!e->parent || e->parent->type != WJR_TYPE_ARRAY)	return(__LINE__);


	return(0);
}

static int ConditionsTest(WJElement doc)
{
	if (!WJEGet(doc, "digits[3] == 3", NULL))				return(__LINE__);
	if ( WJEGet(doc, "digits[3] != 3", NULL))				return(__LINE__);
	if (!WJEGet(doc, "digits[3] <= 3", NULL))				return(__LINE__);
	if (!WJEGet(doc, "digits[3] >= 3", NULL))				return(__LINE__);
	if ( WJEGet(doc, "digits[3] <  2", NULL))				return(__LINE__);
	if ( WJEGet(doc, "digits[3] >  4", NULL))				return(__LINE__);

	if (!WJEGet(doc, "strings[0] == '* some *'", NULL))		return(__LINE__);
	if ( WJEGet(doc, "strings[0] == '* what *'", NULL))		return(__LINE__);
	if ( WJEGet(doc, "strings[0] != '* some *'", NULL))		return(__LINE__);
	if (!WJEGet(doc, "strings[0] != '* what *'", NULL))		return(__LINE__);

	if (!WJEGet(doc, "strings[2] == \"and this\"", NULL))	return(__LINE__);
	if ( WJEGet(doc, "strings[2] == \"not this\"", NULL))	return(__LINE__);
	if ( WJEGet(doc, "strings[2] != \"and this\"", NULL))	return(__LINE__);
	if (!WJEGet(doc, "strings[2] != \"not this\"", NULL))	return(__LINE__);

	if (!WJEGet(doc, "sender.address == 'foo*'", NULL))		return(__LINE__);

	return(0);
}

/*
	A | may be used to indicate that the remaining portion of the selector is
	optional if the match up to that point is valid and has no children.
*/
static int OptionalsTest(WJElement doc)
{
	/* Make sure simple things behave as expected */
	if (!WJEString(doc, "string",		WJE_GET, NULL))		return(__LINE__);
	if ( WJEString(doc, "strings",		WJE_GET, NULL))		return(__LINE__);

	if (!WJEString(doc, "strings[0]",	WJE_GET, NULL))		return(__LINE__);
	if (!WJEString(doc, "strings[$]",	WJE_GET, NULL))		return(__LINE__);
	if (!WJEString(doc, "strings[]",	WJE_GET, NULL))		return(__LINE__);

	if ( WJEString(doc, "string[0]",	WJE_GET, NULL))		return(__LINE__);
	if ( WJEString(doc, "string[$]",	WJE_GET, NULL))		return(__LINE__);
	if ( WJEString(doc, "string[]",		WJE_GET, NULL))		return(__LINE__);

	/*
		Now for the fun part, add the | to get the same results for an array of
		strings as we get for a single string.
	*/
	if (!WJEString(doc, "strings|[0]",	WJE_GET, NULL))		return(__LINE__);
	if (!WJEString(doc, "strings|[$]",	WJE_GET, NULL))		return(__LINE__);
	if (!WJEString(doc, "strings|[]",	WJE_GET, NULL))		return(__LINE__);

	if (!WJEString(doc, "string|[0]",	WJE_GET, NULL))		return(__LINE__);
	if (!WJEString(doc, "string|[$]",	WJE_GET, NULL))		return(__LINE__);
	if (!WJEString(doc, "string|[]",	WJE_GET, NULL))		return(__LINE__);

	return(0);
}



/* Test that default values are returned if property is missing */
static int GetDefaultTest(WJElement doc)
{
	char	*v;

	/* WJENumber */
	if (-1 != WJENumber(doc, "absent",		WJE_GET, -1))	return(__LINE__);
	if (0 != WJENumber(doc,	"absent",		WJE_GET, 0))	return(__LINE__);
	if (1 != WJENumber(doc,	"absent",		WJE_GET, 1))	return(__LINE__);
	if (-1 != WJENumber(doc, "empty",		WJE_GET, -1))	return(__LINE__);
	if (0 != WJENumber(doc,	"empty",		WJE_GET, 0))	return(__LINE__);
	if (1 != WJENumber(doc,	"empty",		WJE_GET, 1))	return(__LINE__);

	/* WJEDouble */
	if (-1.0 != WJEDouble(doc, "absent",	WJE_GET, -1.0)) return(__LINE__);
	if (0.0 != WJEDouble(doc, "absent",		WJE_GET, 0.0))	return(__LINE__);
	if (1.0 != WJEDouble(doc, "absent",		WJE_GET, 1.0))	return(__LINE__);
	if (-1.0 != WJEDouble(doc, "empty",		WJE_GET, -1.0)) return(__LINE__);
	if (0.0 != WJEDouble(doc, "empty",		WJE_GET, 0.0))	return(__LINE__);
	if (1.0 != WJEDouble(doc, "empty",		WJE_GET, 1.0))	return(__LINE__);

	/* WJEBool */
	if (1 != WJEBool(doc, "absent",			WJE_GET, 1))	return(__LINE__);
	if (0 != WJEBool(doc, "absent",			WJE_GET, 0))	return(__LINE__);

	/* NULL is treated as false */
	if (0 != WJEBool(doc, "empty",			WJE_GET, 1))	return(__LINE__);
	if (0 != WJEBool(doc, "empty",			WJE_GET, 0))	return(__LINE__);

	/* WJEString */
	if (!(v = WJEString(doc, "absent",		WJE_GET, "")) ||
		strcmp(v, "") != 0)									return(__LINE__);

	if (!(v = WJEString(doc, "absent",		WJE_GET, "foo")) ||
		strcmp(v, "foo") != 0)								return(__LINE__);

	if (NULL != WJEString(doc, "absent",	WJE_GET, NULL))	return(__LINE__);

	if (!(v = WJEString(doc, "empty",		WJE_GET, "")) ||
		strcmp(v, "") != 0)									return(__LINE__);

	if (!(v = WJEString(doc, "empty",		WJE_GET, "foo")) ||
		strcmp(v, "foo") != 0)								return(__LINE__);

	if (NULL != WJEString(doc, "empty",	WJE_GET, NULL))		return(__LINE__);


	/*
		TODO:
		- WJEInt32
		- WJEUInt32
		- WJEInt64
		- WJEUInt64
	*/

	return(0);
}

/* Test the F functions */
static int FormatStrTest(WJElement doc)
{
	char	*v;
	int		i;
	char	*longstr;

	for (i = 0; i <= 9; i++) {
		if (i != WJEInt32F(doc, WJE_GET, NULL, -1, "digits[%d]", i)) {
			return(__LINE__);
		}
	}

	if (!WJEBoolF(doc, WJE_GET, NULL, FALSE, "bools[%d]", 0))	return(__LINE__);
	if ( WJEBoolF(doc, WJE_GET, NULL, FALSE, "bools[%d]", 1))	return(__LINE__);

	/*
		Test with a VERY long result that should go over the default buffer size
		of 1024.
	*/
	longstr = MemMallocWait(2048);
	memset(longstr, ' ', 2048);
	longstr[2048 - 1] = '\0';

	if (!WJEBoolF(doc, WJE_GET, NULL, FALSE, "bools[%d]%s", 0, longstr)) {
		MemRelease(&longstr);
		return(__LINE__);
	}
	MemRelease(&longstr);

	return(0);
}



/*
	----------------------------------------------------------------------------
	End of tests
	----------------------------------------------------------------------------

	List each test with a name below, and then fill out the same information in
	the CMakeList.txt file in a add_test() line to cause the test to be called
	by 'make test'.
*/
struct {
	char		*name;
	wjetest		cb;
} tests[] = {
	{ "self",		SelfTest		},
	{ "names",		NamesTest		},
	{ "offsets",	OffsetTest		},
	{ "lists",		ListTest		},
	{ "paths",		PathsTest		},
	{ "getvalue",	GetValueTest	},
	{ "getvalues",	GetValuesTest	},
	{ "setvalue",	SetValueTest	},
	{ "newvalue",	NewValueTest	},
	{ "putvalue",	PutValueTest	},
	{ "append",		AppendTest		},
	{ "conditions",	ConditionsTest	},
	{ "optionals",	OptionalsTest	},
	{ "defaults",	GetDefaultTest	},
	{ "formatstr",	FormatStrTest	},

	/*
		TODO: Write the following tests
		- Escaped characters
		- ranges
		- complex lists, ie [ 1, "foo", -3, 1:3 ]
		- pattern matching

		- set value enum
		- new value enum
		- put value enum

		- get complex (objects, arrays, children of arrays)
		- set complex (objects, arrays, children of arrays)
		- new complex (objects, arrays, children of arrays)
		- put complex (objects, arrays, children of arrays)
	*/
	{ NULL,			NULL			}
};

int main(int argc, char **argv)
{
	int			r		= 0;
	WJElement	doc		= NULL;
	WJReader	reader;
	int			i, a, line;
	char		*j, *x;

	MemoryManagerOpen("wjeunit");

	/* Begin tests */
	for (a = 1; a < argc; a++) {
		for (i = 0; tests[i].name && tests[i].cb; i++) {
			if (!stricmp(argv[a], tests[i].name)) {
				break;
			}
		}

		if (!tests[i].cb) {
			fprintf(stderr, "Ignoring unknown test \"%s\"\n", argv[a]);
		} else {
			/* Reopen the JSON for each test in case a test modified it */
			if (!(doc = _WJEParse(json, '\''))) {
				fprintf(stderr, "error: Could not parse JSON document\n");
				MemoryManagerClose("wjeunit");
				return(1);
			}

			if ((line = tests[i].cb(doc))) {
				fprintf(stderr, "error: %s:%d: Failed test \"%s\"\n",
					__FILE__, line, tests[i].name);
				r = 1;
			}

			WJECloseDocument(doc);
		}
	}

	/* All done */
	MemoryManagerClose("wjeunit");
	return(r);
}
