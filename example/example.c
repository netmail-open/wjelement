/*
  example.c: an (incredibly basic) example of a WJElement consumer program

  after installing libwjelement (and running ldconfig if needed)...
  gcc -o example -lwjelement example.c
    or
  gcc -o example `pkg-config --libs wjelement` example.c

  the JSON operations performed in this example are parallel to the following
  javascript code.  notice how similar the code is in both size and clarity:

var doc = {
	"name": "Serenity",
	"class": "firefly",
	"crew": [
		{
			"name": "Malcolm Reynolds",
			"job": "captain",
			"born": 2468
		},
		{
			"name": "Kaywinnet Lee Fry",
			"job": "mechanic",
			"born": 2494
		},
		{
			"name": "Jayne Cobb",
			"job": "public relations"
			"born": 2485
		}
	],
	"cameo" : [
		"Battlestar Galactica",
		"Star Wars Evasive Action",
		"Dr. Horrible's Sing-Along Blog",
		"Ready Player One"
	],
	"shiny": true
};
var person = null;
var cameo = null;

for(i in doc.crew) {  // note: tedious...
	person = doc.crew[i];
	if(person.born == 2468) {
		person.born = 2486;
	}
}
delete(doc.shiny);

for(i in doc.crew) {
	person = doc.crew[i];
	console.log(person.name +" ("+ person.job +") is "+ (2517 - person.born));
}
for(i in doc.cameo) {
	cameo = doc.cameo[i];
	console.log("Cameo: " + cameo);
}

console.log(JSON.stringify(doc));
*/


#include <wjelement.h>
#include <inttypes.h>


int main(int argc, char **argv) {
	WJElement doc = NULL;
	WJElement person = NULL;
	WJElement cameo = NULL;

	doc = WJEObject(NULL, NULL, WJE_NEW);
	WJEString(doc, "name", WJE_SET, "Serenity");
	WJEString(doc, "class", WJE_SET, "firefly");
	WJEArray(doc, "crew", WJE_SET);

	WJEObject(doc, "crew[$]", WJE_NEW);
	WJEString(doc, "crew[-1].name", WJE_SET, "Malcolm Reynolds");
	WJEString(doc, "crew[-1].job", WJE_SET, "captain");
	WJEInt64(doc, "crew[-1].born", WJE_SET, 2468);

	WJEObject(doc, "crew[$]", WJE_NEW);
	WJEString(doc, "crew[-1].name", WJE_SET, "Kaywinnet Lee Fry");
	WJEString(doc, "crew[-1].job", WJE_SET, "mechanic");
	WJEInt64(doc, "crew[-1].born", WJE_SET, 2494);

	WJEObject(doc, "crew[$]", WJE_NEW);
	WJEString(doc, "crew[-1].name", WJE_SET, "Jayne Cobb");
	WJEString(doc, "crew[-1].job", WJE_SET, "public relations");
	WJEInt64(doc, "crew[-1].born", WJE_SET, 2485);

	WJEArray(doc, "cameo", WJE_SET);
	WJEString(doc, "cameo[$]", WJE_NEW, "Battlestar Galactica");
	WJEString(doc, "cameo[$]", WJE_NEW, "Star Wars Evasive Action");
	WJEString(doc, "cameo[$]", WJE_NEW, "Dr. Horrible's Sing-Along Blog");
	WJEString(doc, "cameo[$]", WJE_NEW, "Ready Player One");

	WJEBool(doc, "shiny", WJE_SET, TRUE);

	WJEInt64(doc, "crew[].born == 2468", WJE_SET, 2486);  /* note: awesome! */
	WJECloseDocument(WJEGet(doc, "shiny", NULL));

	while((person = _WJEObject(doc, "crew[]", WJE_GET, &person))) {
		printf("%s (%s) is %"PRId64"\n",
			   WJEString(person, "name", WJE_GET, ""),
			   WJEString(person, "job", WJE_GET, ""),
			   (2517 - WJEInt64(person, "born", WJE_GET, 0)));
	}
	while((cameo = WJEGet(doc, "cameo[]", cameo))) {
		printf("Cameo: %s\n", WJEString(cameo, NULL, WJE_GET, ""));
	}

	WJEDump(doc);
	WJECloseDocument(doc);
	return 0;
}
