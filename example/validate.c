/*
  validate.c: a proof-of-concept json-schema validator
  thanks to xiaoping.x.liu@intel.com

  after installing libwjelement (and running ldconfig if needed)...
  gcc -o validate -lwjelement -lwjreader validate.c
    or
  gcc -o validate `pkg-config --libs wjelement` validate.c
*/


#include <wjelement.h>
#include <wjreader.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
  callback: load more schema from files based on "name" and a pattern argument
*/
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
		path = malloc(strlen(format) + strlen(name));
		sprintf(path, format, name);

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
		free(path);
	}

	return schema;
}

/*
  callback: cleanup/free open schema
*/
static void schema_free(WJElement schema, void *client) {
	WJECloseDocument(schema);
	return;
}

/*
  callback: plop validation errors to stderr
*/
static void schema_error(void *client, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}


int main(int argc, char **argv) {
	FILE *jsonfile;
	FILE *schemafile;
	WJReader readjson;
	WJReader readschema;
	WJElement json;
	WJElement schema;
	char *format;

	if(argc != 3 && argc != 4) {
		printf("usage:\n");
		printf("\t%s <json-file> <schema-file>\n", argv[0]);
		printf("\t%s <json-file> <schema-file> <schema-pattern>\n", argv[0]);
		printf("<schema-pattern>: \"path/to/%%s.json\" additional schemas\n");
		return 255;
	}

	if(!(jsonfile = fopen(argv[1], "r"))) {
		fprintf(stderr, "json file not found: '%s'\n", argv[1]);
		return 1;
	}
	if(!(schemafile = fopen(argv[2], "r"))) {
		fprintf(stderr, "schema file not found: '%s'\n", argv[2]);
		return 2;
	}
	if(argc == 4) {
		format = argv[3];
	} else {
		format = NULL;
	}

	if(!(readjson = WJROpenFILEDocument(jsonfile, NULL, 0)) ||
	   !(json = WJEOpenDocument(readjson, NULL, NULL, NULL))) {
		fprintf(stderr, "json could not be read.\n");
		return 3;
	}
	if(!(readschema = WJROpenFILEDocument(schemafile, NULL, 0)) ||
	   !(schema = WJEOpenDocument(readschema, NULL, NULL, NULL))) {
		fprintf(stderr, "schema could not be read.\n");
		WJECloseDocument(json);
		return 4;
	}

	WJEDump(json);
	printf("json: %s\n", readjson->depth ? "bad" : "good");
	WJEDump(schema);
	printf("schema: %s\n", readschema->depth ? "bad" : "good");

	if(WJESchemaValidate(schema, json, schema_error, schema_load, schema_free,
						 format)) {
		printf("validation: PASS\n");
	} else {
		printf("validation: FAIL\n");
	}

	WJECloseDocument(json);
	WJECloseDocument(schema);
	WJRCloseDocument(readjson);
	WJRCloseDocument(readschema);
	fclose(jsonfile);
	fclose(schemafile);
	return 0;
}
