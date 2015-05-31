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
#include <memmgr.h>
#include <wjelement.h>

typedef int			(* cmdcb)(WJElement *document, WJElement *current, char *line);

typedef struct
{
	char			letter;
	char			*name;
	char			*description;
	cmdcb			cb;
	char			*args;
} WJECLIcmd;

typedef struct
{
	XplBool			pretty;
	int				base;

	char			*filename;

	XplBool			exiting;
} WJECLIGlobals;

/* wjecli.c */
char * nextField(char *value, char **end);
int runcmd(WJElement *doc, WJElement *current, char *line);
void usage(char *arg0);

