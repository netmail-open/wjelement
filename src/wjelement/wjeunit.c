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
#include <nmutil.h>
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
"	'numbers':[ 0, 100, 0x200, 0x500 ],										\n"
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


	if (!WJEGet(doc, "numbers[1] == 100", NULL))			return(__LINE__);
	if ( WJEGet(doc, "numbers[1] != 100", NULL))			return(__LINE__);

	if (!WJEGet(doc, "numbers[1] == 0x64", NULL))			return(__LINE__);
	if ( WJEGet(doc, "numbers[1] != 0x64", NULL))			return(__LINE__);

	if (!WJEGet(doc, "numbers[2] == 0x200", NULL))			return(__LINE__);
	if ( WJEGet(doc, "numbers[2] != 0x200", NULL))			return(__LINE__);

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

	/* A 'null' object is returned as 0 */
	if (0.0 != WJEDouble(doc, "empty",		WJE_GET, -1.0)) return(__LINE__);
	if (0.0 != WJEDouble(doc, "empty",		WJE_GET, 0.0))	return(__LINE__);
	if (0.0 != WJEDouble(doc, "empty",		WJE_GET, 1.0))	return(__LINE__);

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

static char * GenerateBigJSONStr(int count)
{
	/* allocate buffer. */
	char	*item		= "{\"a\" : 1, \"b\": \"abc\"},";
	int		itemlen		= strlen(item);
	char	*out		= MemMalloc((count * itemlen) + 3);
	char	*walk		= out + 1;
	int		i;

	*out = '\0';
	strcpy(out, "[");

	for (i = 0; i < count; i++) {
		walk += sprintf(walk, "%s", item);
	}

	/* Remove the trailing comma */
	walk--;
	strcat(walk, "]");

	return(out);
}

static int BigDocTest(WJElement doc)
{
	char		*json;
	WJReader	r;
	WJElement	d;

	json = GenerateBigJSONStr(100000);
	r = WJROpenMemDocument(json, NULL, 0);
	d = WJEOpenDocument(r, NULL, NULL, NULL);
	WJRCloseDocument(r);
	WJECloseDocument(d);
	MemRelease(&json);

	return(0);
}

static int RealBigDocTest(WJElement doc)
{
	char		*json;
	WJReader	r;
	WJElement	d;

	json = GenerateBigJSONStr(1000000);
	r = WJROpenMemDocument(json, NULL, 0);
	d = WJEOpenDocument(r, NULL, NULL, NULL);
	WJRCloseDocument(r);
	WJECloseDocument(d);
	MemRelease(&json);

	return(0);
}

static int LargeDoc2Test(WJElement doc)
{
	int			result			= 0;
	char		large_json[]	= "{\"BeforeLargeList\":\"123\", \"LargeList\":\"939885|1157278|1296629|1178708|97069|1043125|1300908|1270232|1155448|1263160|871681|1163361|1301383|1283502|42588|68554|234022|759212|428474|1301969|1297102|1138831|1298159|89342|1191373|1151256|1139376|1301359|517236|1128823|1158484|1299600|1256903|303736|949111|472184|1301914|1212022|1227407|716390|1182637|1104734|1295203|679697|1058074|1012246|1290820|755601|254675|824505|697729|1301953|1211766|998718|1248163|460060|486542|405581|1301875|1302042|951938|770999|356894|90697|1295782|1058621|1297104|920706|858095|407433|126772|1255711|1014614|147012|1248711|796429|1301696|1259857|1301983|1291187|1195768|993064|1192559|1128938|1107914|1295671|1138650|1294147|1293446|1281496|1301915|954017|857049|550576|1199137|1227166|1196950|757883|1285141|771441|1212613|1271424|1055467|1124004|1053132|1053509|1062099|1273181|1301946|100936|1189434|1248264|1295205|688908|1070338|1301633|1289025|1249098|1280954|1301492|1268513|891174|1279140|467334|1105736|1294228|1301831|1062832|1258266|1270010|1207428|853700|1286559|1221342|872693|1160797|1292312|1243237|103934|1204446|1297895|83320|1000901|914019|1117550|1301993|1298674|1247437|983634|982737|1223072|1007442|968395|1268662|1276828|1277655|1302026|1103679|1284225|1205753|1295204|889705|1191401|1200436|1199151|851478|569613|888619|994579|107994|748779|1274214|1250704|382492|1166497|1300251|1299832|1283577|1191900|1292225|1182725|1294212|93915|1297778|1272804|1227712|1270251|124036|170874|1278829|1301337|45458|666641|1089462|2774|1281480|900584|1301986|1290139|1298560|516504|1157984|47996|653628|1289526|1029671|447092|1301241|1300273|1198688|652708|1230190|1171973|1124707|1299169|944119|1143102|738436|895649|1029247|1183460|878057|1293381|927095|1290755|1128419|1045068|766570|1131367|884502|1182827|1266603|1185678|786960|1014134|1132067|1246678|1244519|898058|475315|933531|1111878|873132|1301916|941712|1112534|648744|1300149|1225532|64190|1243988|1260643|1301083|829198|1012704|1278945|13022|1301877|874988|94814|1064634|1286722|1026002|11162|985256|1224769|681182|1073851|23144|1214138|869983|1186184|777764|1228525|1300412|1239022|1201294|905664|1292478|1258014|1291461|1222665|280490|1217944|1249031|1061970|1263093|1002415|844710|159469|1266086|760579|666299|88413|1219961|678144|1091530|1301511|977280|1238291|865355|1273635|1248176|994667|878576|74322|1026860|1251550|1177740|835657|987437|706274|1047876|1171782|1110476|1190173|1231247|1301306|1067388|1011268|1254957|1259972|1214183|1300419|1161915|1049067|1249404|1283178|882186|948621|642094|438277|771080|888703|800562|843845|1124144|1263997|894683|782875|1126517|1292677|885563|1260017|680070|993634|1290679|1238284|1226516|1153525|1306|1157554|1247728|1264683|1297786|1222034|1301949|976469|22270|413359|1294413|743329|778693|1145931|1288336|735721|709|1288453|1133211|1202357|1033620|1237308|1302037|1301932|661880|641296|1286053|553460|1263584|1301341|1276087|1299682|1291194|498297|730982|25906|1299586|1106427|706368|461045|1273147|381981|1260095|1300916|1086765|1267992|1277897|812543|1286975|1238357|1204406|880679|1001438|965377|956813|438781|1302028|50728|972672|815070|1300527|1292255|1271376|646023|1228195|1285397|142029|966992|1237953|995160|1083974|1060892|1300272|1300592|851195|1300502|1301145|1168853|1182560|1294384|1198950|1301931|1162528|931001|1002073|257267|1019483|1184937|554449|812077|1089410|1179217|1301984|47549|1189522|1268145|1283703|142431|1270168|1301535|967331|1189170|1266211|1269691|946793|38535|1274639|496514|752318|917110|1289751|1301985|781616|1297762|95564|1090453|545824|1274119|1100730|1281867|1302036|234215|15392|1302027|358039|980864|1294056|1165935|1301334|353797|1086682|907592|748960|1279279|1171479|1175742|985576|923611|1077471|679179|16612|1203504|1299259|787142|1295509|355630|949797|934049|1301307|542315|1248976|1028796|1282218|1017112|1288154|1085017|652160|1301374|831188|1077510|368165|1223767|1294472|1282320|1112567|1223567|1300789|1241268|1228776|1254890|794826|1287023|1301542|1299432|964535|1154135|726281|1254392|1250848|566568|43679|1189396|1246914|863151|1289220|1172752|1176838|974471|1132350|813875|1279593|1243604|1023135|845481|1301217|715692|1053117|1301836|1000260|1098758|381508|1282187|971331|1104076|1065206|1301936|1245020|1229744|69662|1301441|1161702|1301689|1267138|1021636|760954|839168|774842|550203|1299583|1301435|1054732|1214497|356509|1256484|146715|1094301|1277652|897270|1292339|1297526|1227432|1186202|1237960|299|42413|1279069|1289920|982910|1040630|1193330|886451|1179713|1280727|986326|1189997|1298694|148194|982277|1279722|1224395|908051|1277664|1109447|739573|984905|1301833|1299064|1297133|811208|1258547|1218229|921727|878763|1298619|1297819|1272705|1287710|994988|950673|1298156|1272835|1243326|974134|866853|1301782|1109487|1297991|723389|924189|1301863|1210319|521129|730877|754398|788466|459637|41952|18849|1190992|1283999|1033789|979521|1299801|1301765|1230972|1294289|1297808|1036279|1034656|1259099|1222209|627381|125488|545109|1292694|1301834|983935|550949|1204409|802377|1249602|1113624|1259776|1138958|1215939|1129089|1302020|1286224|846605|615995|134144|1012622|1299767|410946|992715|546479|1168395|1257030|1277459|1292785|1281177|1124239|873381|1256945|1024918|1247617|1298414|769811|684404|997533|695285|1299591|306608|1290586|423818|1156022|1049365|1201718\",\"Merge\":\"Evaluate\"}";
	WJReader	r				= WJROpenMemDocument(large_json, NULL, 0);
	WJElement	handle			= WJEOpenDocument(r, NULL, NULL, NULL);
	char		*Before			= WJEString(handle, "BeforeLargeList", WJE_GET | WJE_IGNORE_CASE, "");

	if (strcmp(Before, "123")) {
		printf("e: BeforeLargeList doesn't match expected value ('%s' != '%s') \n", Before, "123");

		result = __LINE__;
	}

	char		*Merge			= WJEString(handle, "Merge", WJE_GET | WJE_IGNORE_CASE, "");

	if (strcmp(Merge, "Evaluate")) {
		printf("e: Merge doesn't match expected value ('%s' != '%s') \n", Merge, "Evaluate");

		result = __LINE__;
	}

	WJECloseDocument(handle);
	WJRCloseDocument(r);

	return(result);
}

static int SchemaTest(WJElement doc)
{
	int			result			= 0;

	/* The schema is valid, and the document adheres to the schema. */
	{
		char		schema_json[]	= "{\"type\":\"object\",\"properties\":{\"string\":{\"type\":\"string\",\"pattern\":\"^This\"}}}";
		WJReader	schema_reader	= WJROpenMemDocument(schema_json, NULL, 0);
		WJElement	schema			= WJEOpenDocument(schema_reader, NULL, NULL, NULL);
		if (!WJESchemaValidate(schema, doc, NULL, NULL, NULL, NULL)) {
			printf("e: Expected document to adhere to schema, but it didn't\n");
			result = __LINE__;
		}
		WJECloseDocument(schema);
		WJRCloseDocument(schema_reader);
	}

	/* The schema is valid, and the document does not adhere to the schema. */
	{
		char		schema_json[]	= "{\"type\":\"object\",\"properties\":{\"string\":{\"type\":\"string\",\"pattern\":\"^That\"}}}";
		WJReader	schema_reader	= WJROpenMemDocument(schema_json, NULL, 0);
		WJElement	schema			= WJEOpenDocument(schema_reader, NULL, NULL, NULL);
		if (WJESchemaValidate(schema, doc, NULL, NULL, NULL, NULL)) {
			printf("e: Expected document to not adhere to schema, but it did\n");
			result = __LINE__;
		}
		WJECloseDocument(schema);
		WJRCloseDocument(schema_reader);
	}

	/* The schema is valid JSON but has an invalid regex. */
	{
		char		schema_json[]	= "{\"type\":\"object\",\"properties\":{\"string\":{\"type\":\"string\",\"pattern\":\"+\"}}}";
		WJReader	schema_reader	= WJROpenMemDocument(schema_json, NULL, 0);
		WJElement	schema			= WJEOpenDocument(schema_reader, NULL, NULL, NULL);
		if (WJESchemaValidate(schema, doc, NULL, NULL, NULL, NULL)) {
			printf("e: Expected validation to fail, but it succeeded\n");
			result = __LINE__;
		}
		WJECloseDocument(schema);
		WJRCloseDocument(schema_reader);
	}

	return(result);
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
	{ "bigdoc",		BigDocTest		},
	{ "realbigdoc",	RealBigDocTest	},
	{ "realbigdoc2",LargeDoc2Test	},
	{ "schema",		SchemaTest		},

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
			if ((j = MemStrdup(json))) {
				/* Correct the quotes */
				for (x = j; *x; x++) {
					if (*x == '\'') *x = '"';
				}

				// printf("JSON:\n%s\n", j);
				if ((reader = WJROpenMemDocument(j, NULL, 0))) {
					doc = WJEOpenDocument(reader, NULL, NULL, NULL);

					WJRCloseDocument(reader);
				}

				MemRelease(&j);
			}

			if (!doc) {
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

