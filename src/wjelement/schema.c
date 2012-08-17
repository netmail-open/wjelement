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


#include "element.h"
#include <stdlib.h>


static char *CatString(char *prefix, char *str) {
	char *ret	= NULL;
	char *s		= NULL;

	if (isalpha(*str)) {
		for (s = str + 1; s && *s; s++) {
			if (!isalnum(*s)) {
				s = NULL;
				break;
			}
		}
	}

	if (s) {
		/* The name appears to be safe to use in a selector */
		if (*prefix) {
			MemAsprintf(&ret, "%s.%s", prefix, str);
		} else {
			ret = MemStrdup(str);
		}
	} else {
		/* The name needs to be wrapped */
		MemAsprintf(&ret, "%s[\"%s\"]", prefix, str);
	}
	return ret;
}

/* grunt work for WJESchemaGetSelectors */
static void ListSelectors(char *prefix, WJElement schema, WJElement document,
						  char *type, char *format,
						  WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
						  WJESchemaMatchCB matchcb, void *client,
						  XplBool exist) {
	WJElement subSchema = NULL;
	WJElement subDocument = NULL;
	WJElement last = NULL;
	int i = 0;
	char *str = NULL;
	char *sel = NULL;
	XplBool schemaGiven = TRUE;

	if(!schema) {
		schemaGiven = FALSE;
		/* fetch document's schema if we don't have it already */
		if((str = WJEString(document, "describedby", WJE_GET, NULL))) {
			schema = loadcb(str, client, __FILE__, __LINE__);
		}
	}
	if(schema) {
		/* swap in any $ref'erenced schema */
		if((str = WJEString(schema, "[\"$ref\"]", WJE_GET, NULL))) {
			if ((last = loadcb(str, client, __FILE__, __LINE__))) {
				if(schema && freecb && !schemaGiven) {
					freecb(schema, client);
					schema = NULL;
				}

				schema = last;
				schemaGiven = FALSE;
			}
		}

		/* look through any "extends" schema(s) */
		if((str = WJEString(schema, "extends", WJE_GET, NULL))) {
			if((subSchema = loadcb(str, client, __FILE__, __LINE__))) {
				ListSelectors(prefix, subSchema, document, type, format,
							  loadcb, freecb, matchcb, client, exist);
			}
			if(subSchema && freecb) {
				freecb(subSchema, client);
				subSchema = NULL;
			}
		}
		if(!str) {
			last = NULL;
			while((str = _WJEString(schema, "extends[]", WJE_GET, &last,
									NULL))) {
				if ((subSchema = loadcb(str, client, __FILE__, __LINE__))) {
					ListSelectors(prefix, subSchema, document, type, format,
								  loadcb, freecb, matchcb, client, exist);
				}
				if(subSchema && freecb) {
					freecb(subSchema, client);
					subSchema = NULL;
				}
			}
		}
	}

	if(schema) {
		sel = prefix;

		/* look through schema for matching type and/or format */
		if (format) {
			str = WJEString(schema, "format", WJE_GET, NULL);

			if (!str || stripat(str, format)) {
				sel = NULL;
			}
		}

		if (sel && type) {
			last = NULL;
			if ((str = _WJEString(schema, "type", WJE_GET, NULL, NULL)) ||
				(str = _WJEString(schema, "type[]", WJE_GET, &last, NULL))
			) {
				do {
					if (!stricmp(str, type)) {
						break;
					}
				} while ((str = _WJEString(schema, "type[]", WJE_GET, &last, NULL)));
			}

			if (!str) {
				/* There wasn't a matching type */
				sel = NULL;
			}
		}

		if(sel && *sel) {
			matchcb(schema, sel, client);
		}

		subSchema = NULL;
		subDocument = NULL;
		if(exist) {
			/* only call matchcb() with selectors found in the document */

			/* dig deeper: object properties */
			while((subSchema = WJEGet(schema, "properties[]", subSchema))) {
				if(subSchema->type == WJR_TYPE_OBJECT) {
					str = CatString(prefix, subSchema->name);
					if(WJEGet(document, str, subDocument)) {
						/* the selector matches something in the document */
						ListSelectors(str, subSchema, document, type, format,
									  loadcb, freecb, matchcb, client, exist);
					}
					if(str && strlen(str)) {
						MemFree(str);
					}
				}
			}

			/* dig deeper: array subscripts / schema "items" */
			if((subSchema = WJEObject(schema, "items", WJE_GET))) {
				subDocument = NULL;
				i = 0;
				MemAsprintf(&str, "%s[%d]", prefix, i);
				while((subDocument = WJEGet(document, str, subDocument))) {
					/* the selector matches something in the document */
					ListSelectors(str, subSchema, subDocument, type, format,
								  loadcb, freecb, matchcb, client, exist);
					MemFree(str);
					i++;
					MemAsprintf(&str, "%s[%d]", prefix, i);
				}
				MemFree(str);
				str = NULL;
			} else {
				i = 0;
				while((subSchema = WJEGet(schema, "items[]", subSchema))) {
					/* tuple-typing */
					if(subSchema->type == WJR_TYPE_OBJECT) {
						MemAsprintf(&str, "%s[%d]", prefix, i);
						subDocument = WJEGet(document, str, NULL);
						ListSelectors(str, subSchema, subDocument, type, format,
									  loadcb, freecb, matchcb, client, exist);
						MemFree(str);
						str = NULL;
						i++;
					}
				}
			}
		} else {
			/* call matchcb() with all selectors possible for the document */

			/* dig deeper: object properties */
			subSchema = NULL;
			while((subSchema = WJEGet(schema, "properties[]", subSchema))) {
				if(subSchema->type == WJR_TYPE_OBJECT) {
					str = CatString(prefix, subSchema->name);
					ListSelectors(str, subSchema, document, type, format,
								  loadcb, freecb, matchcb, client, exist);
					if(str && strlen(str)) {
						MemFree(str);
					}
				}
			}

			/* dig deeper: array subscripts / schema "items" */
			if((subSchema = WJEObject(schema, "items", WJE_GET))) {
				subDocument = NULL;

				MemAsprintf(&str, "%s[]", prefix);
				ListSelectors(str, subSchema, subDocument, type, format,
							  loadcb, freecb, matchcb, client, exist);

				MemFree(str);
				str = NULL;
			} else {
				i = 0;
				while((subSchema = WJEGet(schema, "items[]", subSchema))) {
					/* tuple-typing */
					if(subSchema->type == WJR_TYPE_OBJECT) {
						MemAsprintf(&str, "%s[%d]", prefix, i);
						subDocument = WJEGet(document, str, NULL);
						ListSelectors(str, subSchema, subDocument, type, format,
									  loadcb, freecb, matchcb, client, exist);
						MemFree(str);
						str = NULL;
						i++;
					}
				}
			}
		}

	}

	if(schema && freecb && !schemaGiven) {
		freecb(schema, client);
		schema = NULL;
	}
}

/* get selectors for a given type and/or format in a document */
EXPORT void WJESchemaGetSelectors(WJElement document, char *type, char *format,
								  WJESchemaLoadCB loadcb,
								  WJESchemaFreeCB freecb,
								  WJESchemaMatchCB matchcb, void *client) {
	ListSelectors("", NULL, document, type, format,
				  loadcb, freecb, matchcb, client, TRUE);
	return;
}

/* get all theoretical selectors for a given type and/or format */
EXPORT void WJESchemaGetAllSelectors(char *describedby,
									 char *type, char *format,
									 WJESchemaLoadCB loadcb,
									 WJESchemaFreeCB freecb,
									 WJESchemaMatchCB matchcb, void *client) {
	WJElement document = WJEObject(NULL, NULL, WJE_NEW);
	WJEString(document, "describedby", WJE_NEW, describedby);

	ListSelectors("", NULL, document, type, format,
				  loadcb, freecb, matchcb, client, FALSE);
	WJECloseDocument(document);
	return;
}


static XplBool ValidateType(WJElement value, char *type) {
	if(!stricmp(type, "string")) {
		return (value->type == WJR_TYPE_STRING);
	} else if(!stricmp(type, "number")) {
		return (value->type == WJR_TYPE_NUMBER);
	} else if(!stricmp(type, "integer")) {
		//NOTE: WJElement doesn't do floats
		return (value->type == WJR_TYPE_NUMBER);
	} else if(!stricmp(type, "boolean")) {
		return (value->type == WJR_TYPE_BOOL);
	} else if(!stricmp(type, "object")) {
		return (value->type == WJR_TYPE_OBJECT);
	} else if(!stricmp(type, "array")) {
		return (value->type == WJR_TYPE_ARRAY);
	} else if(!stricmp(type, "null")) {
		return (value->type == WJR_TYPE_NULL);
	} else if(!stricmp(type, "any")) {
		return (value->type != WJR_TYPE_UNKNOWN);
	}
	return FALSE;
}

static int CompareJson(WJElement obj1, WJElement obj2) {
	WJElement arr1 = NULL;
	WJElement arr2 = NULL;

	if(!obj1 || !obj2) {
		return -1;
	}
	if(obj1->type != obj2->type) {
		return -1;
	}
	if(obj1->name && obj2->name && strcmp(obj1->name, obj2->name)) {
		return -1;
	}
	switch(obj1->type) {
	case WJR_TYPE_OBJECT:
	case WJR_TYPE_ARRAY:
		if(obj1->count != obj2->count) {
			return -1;
		}
		while((arr1 = WJEGet(obj1, "[]", arr1))) {
			arr2 = WJEGet(obj2, "[]", arr2);
			if(CompareJson(arr1, arr2)) {
				return -1;
			}
		}
		break;
	case WJR_TYPE_STRING:
		return strcmp(WJEString(obj1, NULL, WJE_GET, ""),
					  WJEString(obj2, NULL, WJE_GET, ""));
		break;
	case WJR_TYPE_NUMBER:
		return (WJENumber(obj1, NULL, WJE_GET, 0) -
				WJENumber(obj2, NULL, WJE_GET, 0));
		break;
	default:
		break;
	}
	return 0;
}

static XplBool SchemaValidate(WJElement schema, WJElement document,
							  WJEErrCB err, WJESchemaLoadCB loadcb,
							  WJESchemaFreeCB freecb, void *client,
							  char *name) {
	WJElement memb = NULL;
	WJElement sub = NULL;
	WJElement arr = NULL;
	WJElement data = NULL;
	WJElement last = NULL;
	char *str = NULL;
	char *str2 = NULL;
	int num = 0;
	int val = 0;
	int i = 0;
	XplBool schemaGiven = TRUE;
	XplBool fail = FALSE;
	XplBool anyFail = FALSE;

	if(!schema) {
		schemaGiven = FALSE;
		/* fetch document's schema if we don't have it already */
		if(loadcb && (str = WJEString(document, "describedby", WJE_GET, NULL))) {
			schema = loadcb(str, client, __FILE__, __LINE__);
		}
	}
	if(schema && loadcb) {
		/* swap in any $ref'erenced schema */
		if((str = WJEString(schema, "$ref", WJE_GET, NULL))) {
			if(schema && freecb) {
				freecb(schema, client);
				schema = NULL;
			}
			schema = loadcb(str, client, __FILE__, __LINE__);
		}

		/* look through any "extends" schema(s) */
		if((str = WJEString(schema, "extends", WJE_GET, NULL))) {
			sub = loadcb(str, client, __FILE__, __LINE__);
			SchemaValidate(sub, document, err, loadcb, freecb, client, name);
			if(sub && freecb) {
				freecb(sub, client);
				sub = NULL;
			}
		} else {
			str = NULL;
			last = NULL;
			while((str = _WJEString(schema, "extends[]", WJE_GET, &last,
									NULL))) {
				sub = loadcb(str, client, __FILE__, __LINE__);
				SchemaValidate(sub, document, err, loadcb, freecb, client, name);
				if(sub && freecb) {
					freecb(sub, client);
					sub = NULL;
				}
			}
		}
	} else {
		return FALSE;
	}

	if(document && (!name || !strlen(name))) {
		name = document->name;
	}

	while((memb = WJEGet(schema, "[]", memb))) {
		fail = FALSE;

		if(!stricmp(memb->name, "type") ||
		   !stricmp(memb->name, "disallow")) {
			if(memb->type == WJR_TYPE_ARRAY) {
				arr = NULL;
				while((arr = WJEGet(memb, "[]", arr))) {
					if(arr->type == WJR_TYPE_STRING) {
						str = WJEString(memb, NULL, WJE_GET, NULL);
						if(!ValidateType(document, str)) {
							fail = TRUE;
						}
					}
				}
			} else if(memb->type == WJR_TYPE_STRING) {
				str = WJEString(memb, NULL, WJE_GET, NULL);
				if(!ValidateType(document, str)) {
					fail = TRUE;
				}
			}

			if(!stricmp(memb->name, "disallow")) {
				fail = !fail;
			}
			if(err && fail) {
				err(client, "%s is of incorrect type", document->name);
			}
			anyFail = anyFail || fail;

		} else if(!stricmp(memb->name, "properties")) {
			if(document && memb->type == WJR_TYPE_OBJECT) {
				arr = NULL;
				while((arr = WJEGet(memb, "[]", arr))) {
					data = WJEGet(document, arr->name, NULL);
					if(!SchemaValidate(memb, data, err, loadcb, freecb, client,
									   arr->name)) {
						fail = TRUE;
						if(err) {
							err(client, "% failed validation", arr->name);
						}
					}
				}
			}
			anyFail = anyFail || fail;

		} else if(!stricmp(memb->name, "patternProperties")) {
			/* TODO: verify this if we ever care about it */

		} else if(!stricmp(memb->name, "additionalProperties")) {
			sub = WJEObject(schema, "properties", WJE_GET);
			if(sub && document && document->type == WJR_TYPE_OBJECT) {
				if(memb->type == WJR_TYPE_FALSE) {
					data = NULL;
					while((data = WJEGet(document, "[]", data))) {
						if(!WJEGet(sub, data->name, NULL)) {
							fail = TRUE;
							if(err) {
								err(client,
									"%s: additional property '%s' found.",
									name, data->name);
							}
						}
					}
				} else if(memb->type == WJR_TYPE_OBJECT) {
					data = NULL;
					while((data = WJEGet(document, "[]", data))) {
						if(!WJEGet(sub, data->name, NULL)) {
							if(!SchemaValidate(memb, data, err, loadcb, freecb,
											   client, data->name)) {
								fail = TRUE;
								if(err) {
									err(client,
										"%s: extra property '%s' not valid.",
										name, data->name);
								}
							}
						}
					}
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "items")) {
			if(document && document->type == WJR_TYPE_ARRAY) {
				if(memb->type == WJR_TYPE_OBJECT) {
					val = 0;
					arr = NULL;
					while((arr = WJEGet(document, "[]", arr))) {
						MemAsprintf(&str, "%s[%d]", name, val);
						if(!SchemaValidate(memb, arr, err, loadcb, freecb,
										   client, str)) {
							fail = TRUE;
							if(err) {
								err(client, "%s failed vaidation.", str);
							}
						}
						MemFree(str);
						str = NULL;
						val++;
					}
				} else if(memb->type == WJR_TYPE_ARRAY) {
					/* tuple typing */
					val = 0;
					arr = NULL;
					while((arr = WJEGet(memb, "[]", arr))) {
						data = NULL;
						if(val < document->count) {
							MemAsprintf(&str, "[%d]", val);
							data = WJEGet(document, str, NULL);
							MemFree(str);
						}
						MemAsprintf(&str, "%s[%d]", name, val);
						if(!SchemaValidate(arr, data, err, loadcb, freecb,
										   client, str)) {
							fail = TRUE;
							if(err) {
								err(client, "%s failed tuple type vaidation.",
									str);
							}
						}
						MemFree(str);
						str = NULL;
						val++;
					}
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "additionalItems")) {
			if(document && document->type == WJR_TYPE_ARRAY) {
				if(memb->type == WJR_TYPE_FALSE) {
					data = WJEArray(schema, "items", WJE_GET);
					fail = (data && document->count > data->count);
					if(fail && err) {
						err(client, "%s: %d additional item(s) found", name,
							document->count - data->count);
					}
				} else if(memb->type == WJR_TYPE_OBJECT) {
					val = 0;
					arr = 0;
					while((arr = WJEGet(document, "[]", arr))) {
						MemAsprintf(&str, "%s[%d]", name, val);
						if(!SchemaValidate(memb, arr, err, loadcb, freecb,
										   client, str)) {
							fail = TRUE;
							if(err) {
								err(client,
									"additional item %s failed vaidation.",
									str);
							}
						}
						MemFree(str);
						str = NULL;
						val++;
					}
				}
			}
			anyFail = anyFail || fail;

		} else if(!stricmp(memb->name, "required")) {
			fail = (!document || document->type == WJR_TYPE_UNKNOWN);
			if(fail && err) {
				err(client, "required item '%s' not found.", name);
			}
			anyFail = anyFail || fail;

		} else if(!stricmp(memb->name, "dependencies")) {
			if(document && memb->type == WJR_TYPE_OBJECT) {
				arr = NULL;
				while((arr = WJEGet(memb, "[]", arr))) {
					if(arr->type == WJR_TYPE_OBJECT) {
						if(!SchemaValidate(arr, document, err, loadcb, freecb,
										   client, name)) {
							fail = TRUE;
							if(err) {
								err(client, "broken dependency schema: %s",
									arr->name);
							}
						}
					} else if(arr->type == WJR_TYPE_STRING) {
						str = WJEString(arr, NULL, WJE_GET, NULL);
						if(WJEGet(document, arr->name, NULL) &&
						   !WJEGet(document, str, NULL)) {
							fail = TRUE;
							if(err) {
								err(client, "%s: dependency '%s' not found",
									name, str);
							}
						}
					} else if(arr->type == WJR_TYPE_ARRAY) {
						sub = NULL;
						while((sub = WJEGet(arr, "[]", sub))) {
							str = WJEString(sub, NULL, WJE_GET,NULL);
							if(str && WJEGet(document, arr->name, NULL) &&
							   !WJEGet(document, str, NULL)) {
								fail = TRUE;
							}
							if(err) {
								err(client, "%s: dependency '%s' not found",
									name, str);
							}
						}
					}
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "minimum")) {
			if(document && memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				if(WJEBool(schema, "exclusiveMinimum", WJE_GET, FALSE)) {
					num = WJENumber(document, NULL, WJE_GET, 0);
					fail = (num < val && num != val);
				} else {
					fail = (num < val);
				}
				if(fail && err) {
					err(client, "%s: minimum value (%d) not met (%d).",
						name, val, num);
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "maximum")) {
			if(document && memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				if(WJEBool(schema, "exclusiveMaximum", WJE_GET, FALSE)) {
					num = WJENumber(document, NULL, WJE_GET, 0);
					fail = (num > val && num != val);
				} else {
					fail = (num > val);
				}
				if(fail && err) {
					err(client, "%s: maximum value (%d) not met (%d).",
						name, val, num);
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "exclusiveMinimum") ||
				  !stricmp(memb->name, "exclusiveMaximum")) {
			/* taken care of in "minimum" and "maximum" handlers above */

		} else if(!stricmp(memb->name, "minItems")) {
			if(document && document->type == WJR_TYPE_ARRAY &&
			   memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				fail = (document->count < val);
				if(fail && err) {
					err(client, "%s: minimum items (%d) not met (%d).",
						name, val, document->count);
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "maxItems")) {
			if(document && document->type == WJR_TYPE_ARRAY &&
			   memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				fail = (document->count > val);
				if(fail && err) {
					err(client, "%s: maximum items (%d) exceeded (%d).",
						name, val, document->count);
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "minLength")) {
			if(document && document->type == WJR_TYPE_STRING &&
			   memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				num = strlen(WJEString(document, NULL, WJE_GET, ""));
				fail = (num < val);
				if(fail && err) {
					err(client, "%s: minimum length (%d) not met (%d).",
						name, val, num);
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "maxLength")) {
			if(document && document->type == WJR_TYPE_STRING &&
			   memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				num = strlen(WJEString(document, NULL, WJE_GET, ""));
				fail = (num > val);
				if(fail && err) {
					err(client, "%s: maximum length (%d) exceeded (%d).",
						name, val, num);
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "uniqueItems")) {
			if(memb->type != WJR_TYPE_FALSE &&
			   document && document->type == WJR_TYPE_ARRAY) {
				val = 0;
				for(i = 0; i < document->count; i++) {
					for(num = i + 1; num < document->count; num++) {
						MemAsprintf(&str, "[%d]", i);
						MemAsprintf(&str2, "[%d]", num);

						if(!CompareJson(WJEGet(document, str, NULL),
										WJEGet(document, str2, NULL))) {
							val = 1;
							if(err) {
								err(client, "%s[%d] identical to %s[%d].",
									name, i, name, num);
							}
						}
					}
				}
				if(val) {
					fail = TRUE;
					if(err) {
						err(client, "%s: non-unique items found.", name);
					}
				}
			}
			anyFail = anyFail || fail;

		} else if(!stricmp(memb->name, "pattern")) {

		} else if(!stricmp(memb->name, "enum")) {
			if(document && memb->type == WJR_TYPE_ARRAY) {
				val = 0;
				arr = NULL;
				while((arr = WJEGet(memb, "[]", arr))) {
					if(!CompareJson(document, memb)) {
						/* found a match */
						val = 1;
					}
				}
				if(!val) {
					fail = TRUE;
					if(err) {
						err(client, "%s: no enum value found.", name);
					}
				}
				anyFail = anyFail || fail;
			}

		} else if(!stricmp(memb->name, "default") ||
				  !stricmp(memb->name, "title") ||
				  !stricmp(memb->name, "description")) {

		} else if(!stricmp(memb->name, "format")) {
			/* not required to validate, TODO: validate anyway */
			str = WJEString(memb, NULL, WJE_GET, NULL);
			if(!stricmp(str, "date-time")) {
			} else if(!stricmp(str, "date")) {
			} else if(!stricmp(str, "time")) {
			} else if(!stricmp(str, "utc-millisec")) {
			} else if(!stricmp(str, "regex")) {
			} else if(!stricmp(str, "color")) {
			} else if(!stricmp(str, "style")) {
			} else if(!stricmp(str, "phone")) {
			} else if(!stricmp(str, "uri")) {
			} else if(!stricmp(str, "email")) {
			} else if(!stricmp(str, "ip-address")) {
			} else if(!stricmp(str, "ipv6")) {
			} else if(!stricmp(str, "host-name")) {
			} else {
			}

		} else if(!stricmp(memb->name, "divisibleBy")) {
			if(document && document->type == WJR_TYPE_NUMBER &&
			   memb->type == WJR_TYPE_NUMBER) {
				val = WJENumber(memb, NULL, WJE_GET, 0);
				num = WJENumber(document, NULL, WJE_GET, 0);
				if(val) {
					fail = (num % val != 0);
				}
				if(fail && err) {
					err(client, "%s: %d not divisible evenly by %d.",
						name, num, val);
				}
				anyFail = anyFail || fail;
			}

		} else {
			/* unknown json-schema property, ignore */
		}
	}

	if(schema && freecb && !schemaGiven) {
		freecb(schema, client);
		schema = NULL;
	}

	return !anyFail;
}

EXPORT XplBool WJESchemaValidate(WJElement schema, WJElement document,
								 WJEErrCB err, WJESchemaLoadCB loadcb,
								 WJESchemaFreeCB freecb, void *client) {
	return SchemaValidate(schema, document, err, loadcb, freecb, client,
						  "(root)");
}

static XplBool ExtendsType(WJElement schema, const char *type,
								 WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								 void *client) {
	WJElement last, sub;
	char *str;
	XplBool match = FALSE;

	if ((str = WJEString(schema, "extends", WJE_GET, NULL))) {
		if (!strcmp(str, type)) {
			return(TRUE);
		}

		if ((sub = loadcb(str, client, __FILE__, __LINE__))) {
			match = ExtendsType(sub, type, loadcb, freecb, client);

			if (freecb) {
				freecb(sub, client);
			}
		}
	} else {
		last = NULL;
		while ((str = _WJEString(schema, "extends[]", WJE_GET, &last, NULL))) {
			if (!strcmp(str, type)) {
				return(TRUE);
			}

			if ((sub = loadcb(str, client, __FILE__, __LINE__))) {
				match = ExtendsType(sub, type, loadcb, freecb, client);

				if (freecb) {
					freecb(sub, client);
				}

				if (match) {
					return(TRUE);
				}
			}
		}
	}

	return(match);
}

EXPORT XplBool WJESchemaIsType(WJElement document, const char *type,
							   WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
							   void *client) {
	WJElement schema;
	char *str;
	XplBool match = FALSE;

	if ((str = WJEString(document, "describedby", WJE_GET, NULL)) && type &&
			!strcmp(type, str)) {
		return(TRUE);
	}

	if (loadcb && (schema = loadcb(str, client, __FILE__, __LINE__))) {
		match = ExtendsType(schema, type, loadcb, freecb, client);

		if (freecb) {
			freecb(schema, client);
		}
	}

	return(match);
}

EXPORT XplBool WJESchemaNameIsType(const char *describedby, const char *type,
								   WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								   void *client) {
	WJElement schema;
	XplBool match = FALSE;

	if(!strcmp(type, describedby)) {
		return TRUE;
	}

	if(loadcb && (schema = loadcb(describedby, client, __FILE__, __LINE__))) {
		match = ExtendsType(schema, type, loadcb, freecb, client);

		if(freecb) {
			freecb(schema, client);
		}
	}

	return match;
}

static char * FindBacklink(WJElement schema, const char *format,
								 WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								 void *client);

EXPORT char * WJESchemaNameFindBacklink(char *describedby, const char *format,
								 WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								 void *client) {
	WJElement sub, link;
	char *str;
	char *result;

	if (!(sub = loadcb(describedby, client, __FILE__, __LINE__))) {
		return(NULL);
	}

	/* Does this type have a backlink that matches the format specified? */
	if ((link = WJEGet(sub, "backlinks", NULL))) {
		for (link = link->child; link; link = link->next) {
			if ((str = WJEString(link, NULL, WJE_GET, NULL)) &&
				!strcmp(format, str)
			) {
				/* Found it */
				return(MemStrdup(link->name));
			}
		}
	}

	if ((result = FindBacklink(sub, format, loadcb, freecb, client))) {
		/*
			A backlink was found on some type that this type extends.  If this
			type has a backlink with the same name though then that name is not
			valid.
		*/
		if ((link = WJEGet(sub, "backlinks", NULL)) && WJEGet(link, result, NULL)) {
			MemRelease(&result);
		}
	}

	if (freecb) {
		freecb(sub, client);
	}

	return(result);
}

static char * FindBacklink(WJElement schema, const char *format,
								 WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
								 void *client) {

	WJElement last;
	char *str;
	char *result;

	if ((str = WJEString(schema, "extends", WJE_GET, NULL))) {
		return(WJESchemaNameFindBacklink(str, format, loadcb, freecb, client));
	} else {
		last = NULL;
		result = NULL;

		while ((str = _WJEString(schema, "extends[]", WJE_GET, &last, NULL))) {
			if ((result = WJESchemaNameFindBacklink(str, format, loadcb, freecb, client))) {
				break;
			}
		}

		return(result);
	}
}

EXPORT char * WJESchemaFindBacklink(WJElement document, const char *format,
									WJESchemaLoadCB loadcb, WJESchemaFreeCB freecb,
									void *client) {
	return(WJESchemaNameFindBacklink(WJEString(document, "describedby", WJE_GET, NULL),
						 format, loadcb, freecb, client));
}

EXPORT void WJESchemaFreeBacklink(char *backlink)
{
	MemRelease(&backlink);
}
