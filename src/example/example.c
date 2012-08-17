/*
  example.c: an (incredibly basic) example of a WJElement consumer program

  after installing libwjelement (and running ldconfig if needed)...
  gcc -o example -lwjelement example.c
*/


#include <wjelement.h>


int main(int argc, char **argv) {
	WJElement obj;

	obj = WJEObject(NULL, NULL, WJE_NEW);
	WJEString(obj, "name", WJE_SET, "sample object");
	WJEArray(obj, "values", WJE_SET);
	WJEInt64(obj, "values[$]", WJE_SET, 1);
	WJEInt64(obj, "values[$]", WJE_SET, 2);
	WJEInt64(obj, "values[$]", WJE_SET, 3);

	WJEDump(obj);
	WJECloseDocument(obj);
	return 0;
}
