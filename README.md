WJElement - JSON manipulation in C
==================================

WJElement is a very flexible JSON library developed by Messaging Architects.
It was created for MA's "WARP" webserver ("Warp Json Elements"), and is built
on top of the lower-level WJReader and WJWriter libraries (also included).

See the [wiki](https://github.com/netmail-open/wjelement/wiki) for more information,
[example code](https://github.com/netmail-open/wjelement/wiki/WJElement-Example)
and full [API](https://github.com/netmail-open/wjelement/wiki/WJElement-API) reference.

WJReader and WJWriter are optimized for speed and memory-efficiency.
WJElement focuses on flexibility and handy features, allowing C code to
manipulate JSON documents with as few statements (fewer, sometimes!) as
JavaScript itself.  WJElement is also capable of json-schema validation.

WJElement has grown into a generally-useful library, and is used across
Messaging Architects' netmail and related projects.  It is loved enough by
MA's developers that we desire to use it elsewhere too, and we think others
will enjoy it as well.  So, here it is, ready to be consumed in any project,
open or closed, as outlined by the GNU LGPL (any version).  Include it as-is
and link to it from your code, massage it into your own statically-linked
package, or use it in ways we haven't thought of.  Read the docs/headers, have
fun, and if you use it for something awesome, let us know about it!  :^)


* Owen Swerkstrom <<oswerkstrom@revealdata.com>> - community/repo admin, WJESchema
* Micah N Gorrell <<wje@minego.net>> - primary author of WJElement
