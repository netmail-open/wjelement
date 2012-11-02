/*
    This file is part of WJElement.

    WJElement is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    WJElement is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with WJElement.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "wjecli.h"

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

static int WJECLILoad(WJElement *doc, WJElement *current, char *line)
{
	WJReader	reader	= NULL;
	WJElement	e		= NULL;
	FILE		*f		= stdin;
	char		*file;
	int			r		= 0;

	file = nextField(line, &line);

	if (nextField(line, &line)) {
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

	if (!(file = nextField(line, &line))) {
		file = wje.filename;
	}

	if (!file || nextField(line, &line)) {
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
	WJElement	e;
	int			r		= 0;

	if (!(e = *current) && !(e = *doc)) {
		fprintf(stderr, "No JSON document loaded\n");
		return(1);
	}

	selector = nextField(line, &line);

	if (nextField(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (selector) {
		if (!stricmp(selector, "..")) {
			if (e->parent) {
				e = e->parent;
			} else {
				fprintf(stderr, "Invalid arguments\n");
				return(3);
			}
		} else if (!(e = WJEGet(*current, selector, NULL))) {
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

		WJWCloseDocument(writer);
	}

	fprintf(stdout, "\n");
	return(r);
}

static int WJECLIPretty(WJElement *doc, WJElement *current, char *line)
{
	char		*arg;

	arg = nextField(line, &line);

	if (nextField(line, &line)) {
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

static int WJECLIBase(WJElement *doc, WJElement *current, char *line)
{
	char		*arg;

	if (!(arg = nextField(line, &line)) ||
		nextField(line, &line)
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

	if (!(selector = nextField(line, &line))) {
		*current = *doc;
		return(0);
	}

	if (nextField(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!stricmp(selector, "..")) {
		if (*current && (*current)->parent) {
			*current = (*current)->parent;
		} else {
			*current = *doc;
		}
		return(0);
	}

	if (!(e = WJEGet(*current, selector, NULL))) {
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

	if (!(selector = nextField(line, &line))) {
		selector = "[]";
	}

	if (nextField(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	if (!stricmp(selector, "..")) {
		selector = "[]";
		if (e->parent) {
			e = e->parent;
		} else {
			return(0);
		}
	}

	m = NULL;
	while ((m = WJEGet(e, selector, m))) {
		printPath(e, m);
	}

	return(0);
}

static int WJECLIpwd(WJElement *doc, WJElement *current, char *line)
{
	if (nextField(line, &line)) {
		fprintf(stderr, "Invalid arguments\n");
		return(2);
	}

	return(WJECLIList(doc, current, "."));
}

WJECLIcmd WJECLIcmds[] =
{
	{
		"load",			"Load a new JSON document, replacing the document "	\
						"that is currently loaded. If a filename is not "	\
						"specified then the document will be read from "	\
						"standard in.",
		WJECLILoad,		"[<filename>]"
	},
	{
		"save",			"Write the currently selected portion of the JSON "	\
						"document to the specified file. If a filename is " \
						"not specified then the last loaded or saved "		\
						"filename will be used.",
		WJECLISave,		"[<filename>]"
	},
	{
		"print",		"Print the currently selected portion of the JSON "	\
						"document.",
		WJECLIPrint,	"[<selector>]"
	},
	{
		"pretty",		"Toggle pretty printing",
		WJECLIPretty,	"[on|off]"
	},
	{
		"base",			"Change the base to use for numbers when printing "	\
						"a JSON document. A base other than 10 will result "\
						"in a non standard JSON document which may not be "	\
						"readable by all parser.",
		WJECLIBase,		"8|10|16"
	},

	{
		"cd",			"Select a child of the currently selected element." \
						"If a selector is not specified then the top of "	\
						"document will be selected. A special case "		\
						"selector of \"..\" will select the parent of the "	\
						"currently selected element.",
		WJECLISelect,	"<selector>"
	},

	{
		"ls",			"List the names of all children of the currently "	\
						"selected element, if it is an object.",
		WJECLIList,		"[<selector>]"
	},

	{
		"pwd",			"Print the path of the currently selected object.",
		WJECLIpwd,		NULL
	},

	{ NULL, NULL, NULL, NULL }
};

