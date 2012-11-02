#include <xpl.h>
#include <memmgr.h>
#include <wjelement.h>

typedef int			(* cmdcb)(WJElement *document, WJElement *current, char *line);

typedef struct
{
	char			*name;
	char			*description;
	cmdcb			cb;
	char			*args;
} WJECLIcmd;

typedef struct
{
	XplBool			pretty;
	int				base;
} WJECLIGlobals;

/* wjecli.c */
char * nextField(char *value, char **end);

