This is a *modified* version of John Bradley's original `XV` tool for image display and
processing. My modification is allows. The modification (kind of) makes `XV` work with
version 1.6 of `libpng` library. The modified version has been *very superficially* tested on Linux
Kubuntu 18.04, and no claim is made as to the usability of the result.

The source code is posted here with the kind permission from John Bradley. Please refer to
the
[original `XV` page](http://www.trilon.com/xv/)
and to the original [README](README) file
for information on copyrights and licensing of this program. Additional
copyright info can also be found inside the source code.


Note: the `Makefile` has been modified to avoid building TIFF module because I could not
get it built on my system. Which means that this version of `XV` will *not work with TIFF* format.
