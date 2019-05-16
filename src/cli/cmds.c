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


#include "wjecli.h"
#include <sys/stat.h>
#include <ctype.h>

extern WJECLIGlobals wje;

static size_t JSONstdinCB(char *buffer, size_t length, size_t seen, void *data)
{
	FILE		*in = data;

	DebugAssert(length);

	if (!fgets(buffer, length, in) || feof(in)) {
		return(0);
	}

	if (*buffer == '.') {
		switch (*(buffer + 1)) {
			case '\0':
			case '\n':
				return(0);

			case '.':
				memmove(buffer, buffer + 1, strlen(buffer));
				break;

			default:
				break;
		}
	}

	return(strlen(buffer));
}

/*
	Using the provided selector find the correct starting container element and
	return the remaining portion of the selector (if any).
*/
static char * _findElement(WJElement *e, char *selector)
{
	/* .. is considered a special case, and means the parent of the current object */
	if (!strcmp(selector, "..")) {
		*e = (*e)->parent;
		return(NULL);
	}

	if ('.' == *selector) {
		/*
			A leading dot (since dot is the separator) indicates the root of the
			document.
		*/
		selector++;

		while ((*e)->parent) {
			*e = (*e)->parent;
		}
	}

	return(selector);
}

/*
	Find the element that the provided selector argument is refering to. This
	function applies all the wjecli specific logic, and then uses WJEAny to
	actually find the element.
*/
static WJElement findElement(WJElement e, char *selector, WJElement last)
{
	selector = _findElement(&e, selector);

	if (last && !selector) {
		/*
			If there is a NULL selector then we can't be looking for a second
			match. It would find the same one it started with.
		*/
		return(NULL);
	}

	return(WJEAny(e, selector, wje.flags | WJE_GET, last));
}

static int WJECLILoad(WJElement *doc, WJElement *current, char *line)
{
	WJReader	reader	= NULL;
	WJElement	e		= NULL;
	FILE		*f		= stdin;
	char		*file;
	int			r		= 0;

	file = nextfield(line, &line);

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (file && !(f = fopen(file, "rb"))) {
		perror(NULL);
		return(3);
	}

	if (f == stdin) {
		fprintf(stdout, "Enter JSON document. Enter a . on it's own line when complete.\n");
		reader = WJROpenDocument(JSONstdinCB, f, NULL, 0);
	} else {
		reader = WJROpenFILEDocument(f, NULL, 0);
	}

	if (!reader) {
		fprintf(stderr, "Internal error, failed to open JSON reader\n");
		r = 4;
	} else if (!(e = WJEOpenDocument(reader, NULL, NULL, NULL))) {
		fprintf(stderr, "Failed to parse JSON document\n");
		r = 5;
	}

	WJRCloseDocument(reader);

	if (e) {
		if (*doc) {
			WJECloseDocument(*doc);
		}

		*doc = *current = e;
		if (file) {
			MemRelease(&wje.filename);
			wje.filename = MemStrdup(file);
		}
	}

	if (f && f != stdin) {
		fclose(f);
	}

	return(r);
}

static int WJECLIMerge(WJElement *doc, WJElement *current, char *line)
{
	WJReader	reader	= NULL;
	WJElement	e		= NULL;
	FILE		*f		= stdin;
	char		*file;
	int			r		= 0;

	file = nextfield(line, &line);

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (file && !(f = fopen(file, "rb"))) {
		perror(NULL);
		return(3);
	}

	if (f == stdin) {
		fprintf(stdout, "Enter JSON document. Enter a . on it's own line when complete.\n");
		reader = WJROpenDocument(JSONstdinCB, f, NULL, 0);
	} else {
		reader = WJROpenFILEDocument(f, NULL, 0);
	}

	if (!reader) {
		fprintf(stderr, "Internal error, failed to open JSON reader\n");
		r = 4;
	} else if (!(e = WJEOpenDocument(reader, NULL, NULL, NULL))) {
		fprintf(stderr, "Failed to parse JSON document\n");
		r = 5;
	}

	WJRCloseDocument(reader);

	if (e) {
		if (*doc) {
			WJEMergeObjects(*doc, e, TRUE);
			WJECloseDocument(e);
		} else {
			*doc = *current = e;
		}
	}

	if (f && f != stdin) {
		fclose(f);
	}

	return(r);
}

static int WJECLISave(WJElement *doc, WJElement *current, char *line)
{
	WJWriter	writer	= NULL;
	FILE		*f		= stdout;
	char		*file;
	WJElement	e;
	int			r		= 0;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}

	if (!(file = nextfield(line, &line))) {
		file = wje.filename;
	}

	if (!file || nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!(f = fopen(file, "wb"))) {
		perror(NULL);
		return(3);
	}

	if (!(writer = WJWOpenFILEDocument(wje.pretty, f))) {
		fprintf(stderr, "Internal error, failed to open JSON writer\n");
		r = 4;
	} else {
		writer->base = wje.base;

		if (!WJEWriteDocument(e, writer, NULL)) {
			fprintf(stderr, "Internal error, failed to save JSON document\n");
			r = 5;
		} else {
			printf("Wrote JSON document to %s\n", file);

			if (file != wje.filename) {
				MemRelease(&wje.filename);
				wje.filename = MemStrdup(file);
			}
		}

		WJWCloseDocument(writer);
	}

	fclose(f);
	return(r);
}

static int WJECLIPrint(WJElement *doc, WJElement *current, char *line)
{
	WJWriter	writer	= NULL;
	char		*selector;
	WJElement	e, c;
	int			r		= 0;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}
	c = e;

	selector = nextfield(line, &line);

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (selector) {
		if (!(e = findElement(c, selector, NULL))) {
			fprintf(stderr, "Could not find specified element: %s\n", selector);
			return(4);
		}
	}

	if (!(writer = WJWOpenFILEDocument(wje.pretty, stdout))) {
		fprintf(stderr, "Internal error, failed to open JSON writer\n");
		r = 4;
	} else {
		writer->base = wje.base;

		if (!WJEWriteDocument(e, writer, NULL)) {
			fprintf(stderr, "Internal error, failed to write JSON document\n");
			r = 5;
		}

		while (!r && selector && (e = findElement(c, selector, e))) {
			fprintf(stdout, "\n");

			if (!WJEWriteDocument(e, writer, NULL)) {
				fprintf(stderr, "Internal error, failed to write JSON document\n");
				r = 5;
			}
		}

		WJWCloseDocument(writer);
	}

	fprintf(stdout, "\n");
	return(r);
}

static int WJECLIDump(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	char		*value;
	WJElement	e, c;
	long		num;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}
	c = e;

	selector = nextfield(line, &line);

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (selector) {
		if (!(e = findElement(c, selector, NULL))) {
			fprintf(stderr, "Could not find specified element: %s\n", selector);
			return(4);
		}
	}

	if ((value = WJEString(e, NULL, wje.flags | WJE_GET, NULL))) {
		fprintf(stdout, "%s\n", value);
		return(0);
	}

	if (WJR_TYPE_NUMBER == e->type) {
		num = WJENumber(e, NULL, wje.flags | WJE_GET, 0);
		fprintf(stdout, "%ld\n", num);
		return(0);

	}

	fprintf(stderr, "Could not find a value for the specified element: %s\n", selector);
	return(4);
}

static int WJECLIPretty(WJElement *doc, WJElement *current, char *line)
{
	char		*arg;

	arg = nextfield(line, &line);

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!arg) {
		wje.pretty = !wje.pretty;
	} else if (!stricmp(arg, "on")) {
		wje.pretty = TRUE;
	} else if (!stricmp(arg, "off")) {
		wje.pretty = FALSE;
	} else {
		fprintf(stderr, "Argument must be \"on\" or \"off\"\n");
		return(3);
	}

	if (wje.pretty) {
		printf("Pretty printing has been enabled\n");
	} else {
		printf("Pretty printing has been disabled\n");
	}

	return(0);
}

static int WJECLIIgnoreCase(WJElement *doc, WJElement *current, char *line)
{
	char		*arg;

	arg = nextfield(line, &line);

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!arg) {
		if (wje.flags & WJE_IGNORE_CASE) {
			wje.flags &= ~WJE_IGNORE_CASE;
		} else {
			wje.flags |= WJE_IGNORE_CASE;
		}
	} else if (!stricmp(arg, "on")) {
		wje.flags |= WJE_IGNORE_CASE;
	} else if (!stricmp(arg, "off")) {
		wje.flags &= ~WJE_IGNORE_CASE;
	} else {
		fprintf(stderr, "Argument must be \"on\" or \"off\"\n");
		return(3);
	}

	if (wje.pretty) {
		printf("Selectors will now be treated as case insensitve\n");
	} else {
		printf("Selectors will now be treated as case sensitve\n");
	}

	return(0);
}

static int WJECLIBase(WJElement *doc, WJElement *current, char *line)
{
	char		*arg;

	if (!(arg = nextfield(line, &line)) ||
		nextfield(line, &line)
	) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!strcmp(arg, "8")) {
		wje.base = 8;
	} else if (!strcmp(arg, "10")) {
		wje.base = 10;
	} else if (!strcmp(arg, "16")) {
		wje.base = 16;
	} else {
		fprintf(stderr, "Argument must be 8, 10 or 16.");
		return(3);
	}

	printf("Numbers will now be printed using base %d\n", wje.base);

	return(0);
}

static int WJECLISelect(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	WJElement	e;

	if (!(selector = nextfield(line, &line))) {
		*current = *doc;
		return(0);
	}

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!(e = findElement(*current, selector, NULL))) {
		fprintf(stderr, "Could not find specified element: %s\n", selector);
		return(3);
	}

	*current = e;
	return(0);
}

/* Return TRUE if this portion printed */
static XplBool _printPath(WJElement base, WJElement e, WJElement last)
{
	int			i;
	char		*c;
	WJElement	p;
	XplBool		r	= FALSE;

	if (e && e->parent && e->parent != base) {
		r = _printPath(base, e->parent, last);
	}

	if (e->name) {
		for (c = e->name; *c; c++) {
			if (!isalnum(*c)) {
				break;
			}
		}

		if (!*c) {
			if (r) {
				/* The previous portion of the path printed a name */
				printf(".");
			}

			printf("%s", e->name);
		} else {
			printf("['");

			for (c = e->name; *c; c++) {
				switch (*c) {
					case '.':
					case '[':
					case ']':
					case '*':
					case '?':
					case '"':
					case '\'':
					case '\\':
						printf("\\%c", *c);
						break;

					default:
						printf("%c", *c);
						break;
				}
			}

			printf("']");
		}

		return(TRUE);
	} else if (e->parent) {
		for (i = 0, p = e->parent->child; p != e; i++, p = p->next) {
			;
		}

		if (p) {
			printf("[%d]", i);
			return(TRUE);
		}
	}

	return(FALSE);
}

static void printPath(WJElement base, WJElement e)
{
	_printPath(base, e, e);
	printf("\n");
}

static int WJECLIList(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	WJElement	e, m;

	e = *current;

	if ((selector = nextfield(line, &line))) {
		selector = _findElement(&e, selector);
	}

	if (!selector || !*selector) {
		selector = "[]";
	}

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	m = NULL;
	while ((m = WJEAny(e, selector, wje.flags | WJE_GET, m))) {
		printPath(e, m);
	}

	return(0);
}

static int WJECLIpwd(WJElement *doc, WJElement *current, char *line)
{
	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	return(WJECLIList(doc, current, "."));
}

static int WJECLISet(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	FILE		*file		= NULL;
	char		*contents	= NULL;
	WJReader	reader		= NULL;
	WJElement	n			= NULL;
	WJElement	t;
	struct stat	sb;
	int			r = 0;
	uint64		u;
	int64		i;
	double		d;

	if (!(selector = nextfield(line, &line)) || !line || !*line) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	line = chopspace(line);
	switch (*line) {
		case '#':
			line++;

			/* Treat the rest of the line as a path to a raw document */
			if (stat(line, &sb) || !(file = fopen(line, "r"))) {
				fprintf(stderr, "Could not open file: %s\n", line);
				r = 3;
			} else if (!(contents = MemMalloc(sb.st_size + 1))) {
				fprintf(stderr, "Could not allocate memory\n");
				r = 4;
			} else if ((sb.st_size != (off_t) fread(contents, sizeof(char), sb.st_size, file))) {
				fprintf(stderr, "Could not read file: %s\n", line);
				r = 3;
			} else if (!(n = WJEObject(NULL, NULL, wje.flags | WJE_SET))) {
				fprintf(stderr, "Could not allocate memory\n");
				r = 5;
			} else {
				WJEStringN(n, NULL, wje.flags | WJE_SET, contents, sb.st_size);
			}

			MemRelease(&contents);
			break;

		case '@':
			line++;

			/* Treat the rest of the line as a path to a JSON document */
			if (!(file = fopen(line, "r"))) {
				fprintf(stderr, "Could not open JSON document: %s\n", line);
				r = 3;
			} else if (!(reader = WJROpenFILEDocument(file, NULL, 0))) {
				fprintf(stderr, "Internal error, failed to open JSON reader\n");
				r = 3;
			} else if (!(n = WJEOpenDocument(reader, NULL, NULL, NULL))) {
				fprintf(stderr, "Failed to parse JSON document\n");
				r = 4;
			}
			break;

		default:
			/* Treat the rest of the line as JSON */
			if (!(reader = WJROpenMemDocument(line, NULL, 0))) {
				fprintf(stderr, "Internal error, failed to open JSON reader\n");
				r = 3;
			} else if (!(n = WJEOpenDocument(reader, NULL, NULL, NULL))) {
				fprintf(stderr, "Failed to parse JSON document\n");
				r = 4;
			}
			break;
	}

	if (n) {
		switch (n->type) {
			case WJR_TYPE_OBJECT:
				t = WJEObject(*current, selector, wje.flags | WJE_SET);
				WJEMergeObjects(t, n, TRUE);
				break;

			case WJR_TYPE_ARRAY:
				t = WJEArray(*current, selector, wje.flags | WJE_SET);
				WJEMergeObjects(t, n, TRUE);
				break;

			case WJR_TYPE_STRING:
				WJEString(*current, selector, wje.flags | WJE_SET,
						WJEString(n, NULL, wje.flags | WJE_GET, NULL));
				break;

			case WJR_TYPE_NUMBER:
				d = WJEDouble(n, NULL, wje.flags | WJE_GET, 0);

				if (d != (uint64) d) {
					WJEDouble(*current, selector, wje.flags | WJE_SET, d);
				} else {
					u = WJEUInt64(n, NULL, wje.flags | WJE_GET, 0);
					i = WJEInt64( n, NULL, wje.flags | WJE_GET, 0);

					if (i < 0) {
						WJEInt64(*current, selector, wje.flags | WJE_SET, i);
					} else {
						WJEUInt64(*current, selector, wje.flags | WJE_SET, u);
					}
				}

				break;

			case WJR_TYPE_BOOL:
			case WJR_TYPE_TRUE:
			case WJR_TYPE_FALSE:
				WJEBool(*current, selector, wje.flags | WJE_SET,
						WJEBool(n, NULL, wje.flags | WJE_GET, FALSE));
				break;

			case WJR_TYPE_NULL:
				WJENull(*current, selector, wje.flags | WJE_SET);
				break;

			default:
				break;
		}

		WJECloseDocument(n);
	}

	if (reader) {
		WJRCloseDocument(reader);
	}

	if (file) {
		fclose(file);
	}

	return(r);
}

static int WJECLIMove(WJElement *doc, WJElement *current, char *line)
{
	char		*selectora;
	char		*selectorb;
	char		*name;
	WJElement	e;
	WJElement	child;
	WJElement	parent;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}

	selectora	= nextfield(line, &line);
	selectorb	= nextfield(line, &line);
	name		= nextfield(line, &line);

	if (!selectora || !selectorb || nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!(child = findElement(e, selectora, NULL))) {
		fprintf(stderr, "Could not find specified element: %s\n", selectora);
		return(4);
	}

	if (!(parent = findElement(e, selectorb, NULL)) &&
		!(parent = WJEObject(e, selectorb, wje.flags | WJE_NEW))
	) {
		fprintf(stderr, "Could not find specified element: %s\n", selectorb);
		return(4);
	}

	WJEDetach(child);
	if (name) {
		WJERename(child, name);
	}
	WJEAttach(parent, child);

	return(0);
}

static int WJECLICopy(WJElement *doc, WJElement *current, char *line)
{
	char		*selectora;
	char		*selectorb;
	char		*name;
	WJElement	e;
	WJElement	child;
	WJElement	parent;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}

	selectora	= nextfield(line, &line);
	selectorb	= nextfield(line, &line);
	name		= nextfield(line, &line);

	if (!selectora || !selectorb || nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!(child = findElement(e, selectora, NULL))) {
		fprintf(stderr, "Could not find specified element: %s\n", selectora);
		return(4);
	}

	if (!(parent = findElement(e, selectorb, NULL)) &&
		!(parent = WJEObject(e, selectorb, wje.flags | WJE_NEW))
	) {
		fprintf(stderr, "Could not find specified element: %s\n", selectorb);
		return(4);
	}

	if (!(child = WJECopyDocument(NULL, child, NULL, NULL))) {
		fprintf(stderr, "Could not find copy element: %s\n", selectora);
		return(5);
	}

	if (name) {
		WJERename(child, name);
	}
	WJEAttach(parent, child);

	return(0);
}

static int wjeKeySortCB(const void *a, const void *b)
{
	WJElement	*doca	= (WJElement *) a;
	WJElement	*docb	= (WJElement *) b;
	char		*keya	= ((* doca) && (* doca)->name) ? (* doca)->name : "";
	char		*keyb	= ((* docb) && (* docb)->name) ? (* docb)->name : "";

	return(strcmp(keya, keyb));
}

static int wjeValueSortCB(const void *a, const void *b)
{
	WJElement	*doca	= (WJElement *) a;
	WJElement	*docb	= (WJElement *) b;
	char		*vala	= WJEString(*doca, NULL, wje.flags | WJE_GET, NULL);
	char		*valb	= WJEString(*docb, NULL, wje.flags | WJE_GET, NULL);
	char		linea[256];
	char		lineb[256];

	if (!vala && WJR_TYPE_NUMBER == (*doca)->type) {
		int64		tmp = WJEInt64(*doca, NULL, wje.flags | WJE_GET, 0);

		strprintf(linea, sizeof(linea), NULL, "%lld", (long long) tmp);
		vala = linea;
	}

	if (!valb && WJR_TYPE_NUMBER == (*docb)->type) {
		int64		tmp = WJEInt64(*docb, NULL, wje.flags | WJE_GET, 0);

		strprintf(lineb, sizeof(lineb), NULL, "%lld", (long long) tmp);
		valb = lineb;
	}

	return(strcmp(vala ? vala : "", valb ? valb : ""));
}

/* Sort the children of the provided object by name */
static void wjesort(WJElement doc, XplBool recursive, int (*compar)(const void *, const void *))
{
	WJElement	c;
	WJElement	*list;
	int			i;
	int			count;

	if (!doc || !doc->child) {
		return;
	}

	if (recursive) {
		for (c = doc->child; c; c = c->next) {
			wjesort(c, recursive, compar);
		}
	}

	/* Step 1: Pull the children off and put them in a list */
	list = MemCallocWait(doc->count + 1, sizeof(WJElement));
	count = doc->count;

	for (i = 0; i < count && (c = doc->child); i++) {
		WJEDetach(c);
		list[i] = c;
	}

	/* Step 2: Sort the list */
	qsort(&list[0], count, sizeof(WJElement), compar);

	/* Step 3: Reattach the children */
	for (i = 0; i < count; i++) {
		if (!list[i]) {
			continue;
		}

		WJEAttach(doc, list[i]);
		list[i] = NULL;
	}
	MemRelease(&list);
}

static int WJECLIKeySort(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	XplBool		recursive	= FALSE;
	WJElement	e, c;
	int			r			= 0;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}
	c = e;

	selector = nextfield(line, &line);

	/* Does the user want it to be recursive?  */
	if (selector && (!stricmp(selector, "-r") || !stricmp(selector, "--recursive"))) {
		recursive = TRUE;

		selector = nextfield(line, &line);
	}

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (selector) {
		if (!(e = findElement(c, selector, NULL))) {
			fprintf(stderr, "Could not find specified element: %s\n", selector);
			return(4);
		}
	}

	wjesort(e, recursive, wjeKeySortCB);
	return(r);
}

static int WJECLIValueSort(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	XplBool		recursive	= FALSE;
	WJElement	e, c;
	int			r			= 0;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}
	c = e;

	selector = nextfield(line, &line);

	/* Does the user want it to be recursive?  */
	if (selector && (!stricmp(selector, "-r") || !stricmp(selector, "--recursive"))) {
		recursive = TRUE;

		selector = nextfield(line, &line);
	}

	if (nextfield(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (selector) {
		if (!(e = findElement(c, selector, NULL))) {
			fprintf(stderr, "Could not find specified element: %s\n", selector);
			return(4);
		}
	}

	wjesort(e, recursive, wjeValueSortCB);
	return(r);
}

static int WJECLIRemove(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	WJElement	e, m;

	e = *current;

	if (!(selector = nextfield(line, &line)) ||
		nextfield(line, &line)
	) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	while ((m = WJEAny(e, selector, wje.flags | WJE_GET, NULL))) {
		WJECloseDocument(m);
	}

	return(0);
}

static int WJECLIEach(WJElement *doc, WJElement *current, char *line)
{
	char		*selector;
	char		*tmpline;
	WJElement	e, m, c;
	int			r = 0;

	if (!(selector	= nextfield(line, &line)) ||
		!line
	) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	e = *current;
	m = NULL;
	while ((m = WJEAny(e, selector, wje.flags | WJE_GET, m))) {
		c = m;

		/*
			Create a coppy of the args for each call, because some comamnd
			command callbacks modify the line.
		*/
		tmpline = MemStrdupWait(line);
		r = runcmd(doc, &c, tmpline);
		MemRelease(&tmpline);
	}

	return(r);
}

static WJElement schema_load(const char *name, void *client,
							 const char *file, const int line) {
	char *format;
	char *path;
	FILE *schemafile;
	WJReader readschema;
	WJElement schema;

	schema = NULL;
	if(client && name) {
		format = (char *)client;
		MemAsprintf(&path, format, name);

		if ((schemafile = fopen(path, "r"))) {
			if((readschema = WJROpenFILEDocument(schemafile, NULL, 0))) {
				schema = WJEOpenDocument(readschema, NULL, NULL, NULL);
			} else {
				fprintf(stderr, "json document failed to open: '%s'\n", path);
			}
			fclose(schemafile);
		} else {
			fprintf(stderr, "json file not found: '%s'\n", path);
		}
		MemRelease(&path);
	}

	return schema;
}
static void schema_error(void *client, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
static int WJECLIValidate(WJElement *doc, WJElement *current, char *line)
{
	WJReader reader = NULL;
	WJElement e = NULL;
	FILE *f = stdin;
	char *file;
	char *pat;
	int r = 0;

	if(!(file = nextfield(line, &line))) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}
	pat = nextfield(line, &line);

	if(!(f = fopen(file, "rb"))) {
		perror(NULL);
		return(3);
	}

	if(f == stdin) {
		fprintf(stdout, "Enter JSON document. " \
				"Enter a . on it's own line when complete.\n");
		reader = WJROpenDocument(JSONstdinCB, f, NULL, 0);
	} else {
		reader = WJROpenFILEDocument(f, NULL, 0);
	}

	if(!reader) {
		fprintf(stderr, "Internal error, failed to open JSON reader\n");
		r = 4;
	} else if (!(e = WJEOpenDocument(reader, NULL, NULL, NULL))) {
		fprintf(stderr, "Failed to parse JSON document\n");
		r = 5;
	}
	WJRCloseDocument(reader);

	if(e && *doc) {
		if(WJESchemaValidate(e, *doc, schema_error, schema_load, NULL, pat)) {
			fprintf(stdout, "Schema validation: PASS\n");
		} else {
			fprintf(stderr, "Schema validation: FAIL\n");
			r = 6;
		}
	}

	return(r);
}

static int WJECLIHelp(WJElement *doc, WJElement *current, char *line)
{
	usage(NULL);
	return(0);
}

static int WJECLINoop(WJElement *doc, WJElement *current, char *line)
{
	/* This is used for handling comments */
	return(0);
}

static int WJECLIExit(WJElement *doc, WJElement *current, char *line)
{
	wje.exiting = TRUE;
	return(0);
}

WJECLIcmd WJECLIcmds[] =
{
	{
		'?', "print",	"Print the currently selected portion of the JSON "	\
						"document.",
		WJECLIPrint,	"[<selector>]"
	},
	{
		'\0', "p",		NULL,
		WJECLIPrint,	NULL
	},
	{
		'+', "set",		"Set a JSON value",
		WJECLISet,		"<selector> <json|@filename.json|#filename.txt>"
	},

	{
		'-', "del",		"Delete a JSON value",
		WJECLIRemove,	"<selector>"
	},
	{
		'\0', "rm",		NULL,
		WJECLIRemove,	NULL
	},



	{
		'\0', "dump",	"Dump the value of the currently selected element of " \
						"the JSON document.",
		WJECLIDump,		"[<selector>]"
	},
	{
		'\0', "d",		NULL,
		WJECLIDump,		NULL
	},


	{
		'\0', "pretty",	"Toggle pretty printing",
		WJECLIPretty,	"[on|off]"
	},
	{
		'\0', "ignorecase",	"Toggle case sensitivity",
		WJECLIIgnoreCase,"[on|off]"
	},
	{
		'\0', "base",	"Change the base to use for numbers when printing "	\
						"a JSON document. A base other than 10 will result "\
						"in a non standard JSON document which may not be "	\
						"readable by all parser.",
		WJECLIBase,		"8|10|16"
	},

	{
		'\0', "cd",		"Select a child of the currently selected element." \
						"If a selector is not specified then the top of "	\
						"document will be selected. A special case "		\
						"selector of \"..\" will select the parent of the "	\
						"currently selected element.",
		WJECLISelect,	"<selector>"
	},

	{
		'\0', "ls",		"List the names of all children of the currently "	\
						"selected element, if it is an object.",
		WJECLIList,		"[<selector>]"
	},

	{
		'\0', "pwd",	"Print the path of the currently selected object.",
		WJECLIpwd,		NULL
	},

	{
		'\0', "each",	"Run a command for each object matching the "		\
						"specified selector.",
		WJECLIEach,		"<selector> <command with arguments>"
	},

	{
		'\0', "validate","Validate the current document against schema.  "	\
						"pattern example: \"path/to/%s.json\" more schemas",
		WJECLIValidate,	"<schema file> [<pattern>]"
	},

	{
		'\0', "mv",		"Move an element from it's current parent to a new parent",
		WJECLIMove,		"<selector> <selector> [<newname>]"
	},
	{
		'\0', "move",	NULL,
		WJECLIMove,		NULL
	},
	{
		'\0', "cp",		"Copy an element from it's current parent to a new parent",
		WJECLICopy,		"<selector> <selector> [<newname>]"
	},
	{
		'\0', "copy",	NULL,
		WJECLICopy,		NULL
	},
	{
		'\0', "sort",	"Sort the children of an object by their values.",
		WJECLIValueSort,"[-r|--recursive] [<selector>]"
	},
	{
		'\0', "keysort","Sort the children of an object by their keys.",
		WJECLIKeySort,	"[-r|--recursive] [<selector>]"
	},

	{
		'\0', "load",	"Load a new JSON document, replacing the document "	\
						"that is currently loaded. If a filename is not "	\
						"specified then the document will be read from "	\
						"stdin.",
		WJECLILoad,		"[<filename>]"
	},
	{
		'\0', "merge",	"Load a new JSON document, and merge it into the "	\
						"document that is currently loaded. If a filename "	\
						"is not specified then the document will be read "	\
						"from stdin.",
		WJECLIMerge,	"[<filename>]"
	},
	{
		'\0', "save",	"Write the currently selected portion of the JSON "	\
						"document to the specified file. If a filename is " \
						"not specified then the last loaded or saved "		\
						"filename will be used.",
		WJECLISave,		"[<filename>]"
	},

	{
		'\0', "help",	NULL,
		WJECLIHelp,		NULL
	},

	{
		'\0', "quit",	NULL,
		WJECLIExit,		NULL
	},
	{
		'\0', "exit",	NULL,
		WJECLIExit,		NULL
	},

	{
		'#', "noop",	NULL,
		WJECLINoop,		NULL
	},
	{ '\0', NULL, NULL, NULL, NULL }
};

