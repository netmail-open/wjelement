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
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

WJECLIGlobals wje;

extern WJECLIcmd WJECLIcmds[];

void usage(char *arg0)
{
	WJECLIcmd	*cmd;
	int			i;

	if (arg0) {
		printf("Command line JSON document browser\n");
		printf("Usage: %s [json file]\n\n", arg0);

		printf("Commands:\n");
	}

	for (i = 0; (cmd = &WJECLIcmds[i]) && cmd->name; i++) {
		if (!cmd->description) {
			continue;
		}

		printf("\t");

		if (cmd->letter != '\0') {
			printf("%c|", cmd->letter);
		}

		printf("%s %s\n", cmd->name, cmd->args ? cmd->args : "");
		printf("\t\t%s\n\n", cmd->description);
	}
}

char * nextField(char *value, char **end)
{
	char		*d, *s;

	if (end) {
		*end = NULL;
	}

	if (!value) {
		return(NULL);
	}

	value = skipspace(value);

	if (!*value) {
		return(NULL);
	}

	if ('"' == *value) {
		value++;

		d = s = value;
		while (s) {
			switch (*s) {
				case '\0':
				case '\n':
				case '\r':
					*d = '\0';
					return(*value ? value : NULL);

				case '"':
					*d = '\0';
					if (end) {
						*end = s + 1;
					}
					return(*value ? value : NULL);

				case '\\':
					switch (*(++s)) {
						case '"':
						case '\\':
							*(d++) = *(s++);
							break;

						default:
							*(d++) = '\\';
					}
					break;

				default:
					*(d++) = *(s++);
					break;
			}
		}
	} else {
		if ((d = strspace(value))) {
			*d = '\0';
			if (end) {
				*end = d + 1;
			}
		}

		return(*value ? value : NULL);
	}

	return(NULL);
}

int runcmd(WJElement *doc, WJElement *current, char *line)
{
	WJECLIcmd		*command;
	char			*cmd	= line;
	char			*args	= NULL;
	int				i;

	// cmd = nextField(line, &args);

	/* Look for a command using the letter, which does NOT require a space */
	for (i = 0; (command = &WJECLIcmds[i]) && command->name; i++) {
		if (command->letter != '\0' && *cmd == command->letter) {
			args = skipspace(++cmd);
			break;
		}
	}

	/* Look for a full command */
	if (!command || !command->name) {
		cmd = nextField(line, &args);

		for (i = 0; (command = &WJECLIcmds[i]) && command->name; i++) {
			if (!stricmp(cmd, command->name)) {
				break;
			}
		}
	}

	if (!*current) {
		*current = *doc;
	}

	if (command && command->name) {
		return(command->cb(doc, current, args));
	} else {
		fprintf(stderr, "Unknown command: %s\n", cmd);
		return(3);
	}
}

int main(int argc, char **argv)
{
	FILE		*in			= NULL;
	WJElement	doc			= NULL;
	WJElement	current		= NULL;
	int			r			= 0;
	WJReader	reader;
	char		*cmd;
	char		line[1024];

	/* Print pretty documents by default */
	wje.pretty = TRUE;

	/* Print base 10 by default */
	wje.base = 10;

	if (argc > 2) {
		/* Umm, we only allow one argument... a filename */
		usage(argv[0]);
		return(1);
	}

	MemoryManagerOpen("wje-cli");

	if (argc == 2) {
		if (!stricmp(argv[1], "--help") || !stricmp(argv[1], "-h")) {
			usage(argv[0]);
			MemoryManagerClose("wje-cli");
			return(0);
		}

		if (!(in = fopen(argv[1], "rb"))) {
			perror(NULL);
			MemoryManagerClose("wje-cli");
			return(2);
		}

		/*
			A filename was specified on the command line. Does this look
			like a script, or a JSON document?
		*/
		if (fgets(line, sizeof(line), in) &&
			!strncmp(line, "#!", 2)
		) {
			/* This looks like a script, read commands from this file */
			;
		} else {
			/* Assume it is a JSON document, rewind back to the start. */
			rewind(in);

			if ((reader = WJROpenFILEDocument(in, NULL, 0))) {
				doc = WJEOpenDocument(reader, NULL, NULL, NULL);
				WJRCloseDocument(reader);

				wje.filename = MemStrdup(argv[1]);
			}

			fclose(in);
			in = NULL;

			if (!doc) {
				fprintf(stderr, "Could not parse JSON document: %s\n", argv[1]);
			MemoryManagerClose("wje-cli");
				return(3);
			}
		}
	}

	if (!in) {
		/* Read commands from standard in */
		in = stdin;
	}

	if (!doc) {
		/* Start with an empty document if one wasn't specified */
		doc = WJEObject(NULL, NULL, WJE_SET);
	}
	current = doc;

	for (;;) {
		/* Read the next command */
		if (in == stdin && isatty(fileno(stdin))) {
			fprintf(stdout, "wje");

			if (r) {
				fprintf(stdout, " (%d)", r);
			}

			fprintf(stdout, "> ");
			fflush(stdout);
		}

		if (fgets(line, sizeof(line), in)) {
			cmd = skipspace(line);
		} else {
			cmd = NULL;
		}

		if (!cmd || !*cmd) {
			/* Ignore blank lines */
		} else {
			r = runcmd(&doc, &current, cmd);
		}
		cmd = NULL;

		if (feof(in) || wje.exiting) {
			break;
		}
	}

	if (doc) {
		WJECloseDocument(doc);
		doc = NULL;
	}

	if (in && in != stdin) {
		fclose(in);
		in = NULL;
	}

	if (wje.filename) {
		MemRelease(&wje.filename);
	}

	MemoryManagerClose("wje-cli");
	return(r);
}

