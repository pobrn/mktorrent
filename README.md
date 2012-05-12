libmktorrent
============

> libmktorrent is a clone of esmil's mktorrent


About
-----
mktorrent is a nice C application to make a .torrent file for your data. I need mktorrent in context of a application, so i clone mktorrent and add a target for building a static library.

I added a few hacks to get it nice and silent while working as library in your program. 


Build & Install
---------------
As the Original: type 'make' to build the program.<br />
Do 'make install' to install the program to /usr/local/bin.

Do 'make aslib' to compile mktorrent as static library called libmktorrent.a<br /> 
The library and the header file will be located inside the /dist folder.


Example
-------
<pre><code>
#include <stdio.h>
#include <stdlib.h>
#include "./libmktorrent.h"

int main(int argc, char *argv[]) {

        metafile_t *m = getDefaultMetaStruc();
        unlink("testfile.torrent");
        puts("------------------------------------------------------");

        int st;
        st = mktorrent("/tmp/testfile", "http://localhost/anounce", m);
        printf("Result is: %d\n", st);

        
        puts("------------------------------------------------------");
        return EXIT_SUCCESS;
}
</code></pre>

run: 
<code>
> cc main.c -o test ./libmktorrent.a
> ./test
</code>

The output should be like this:
<pre>
------------------------------------------------------
Result is: 0
------------------------------------------------------
</pre>

There is a <b>testfile.torrent</b> in your directory now.


On last word
------------
Sadly, esmil put the mktorrent under the GNU GPL, Version 2 (see COPYING). As for all of my projects I want to give you this piece of code for any purpose you get, but i can't because it's gpl'ed. :-( 
So please respect the decision of esmil and DO NOT SPEND ME A BEER! 

