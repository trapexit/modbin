# MODBIN

modbin is a tool that was included in the 3DO SDK and is used to modify the 3DO executable's slightly custom AIF header. A homebrew version was released but only supports setting the "work space" value and the stack size. This tool offers all the same features found in the original tool with a few minor extras. Should compile on any C99 C compiler.


# USAGE

```
Usage: modbin [options]... <input-file> [<output-file>]

  modbin is used to set 3DO AIF header values and sign executables.

  -h --help                 print this help message and exit
  -V                        print modbin version
     --debug                enable debugging
     --nodebug              disable debugging
     --pri=UNSIGNED         set priority
     --version=UNSIGNED     set version number
     --flags=UNSIGNED       set app flags
     --osversion=UNSIGNED   set OS_version number
     --osrevision=UNSIGNED  set OS_revision number
     --stack=UNSIGNED       set stack size
     --freespace=UNSIGNED   set freespace
     --maxusecs=UNSIGNED    set maximum usecs
     --name=STRING          executable name
     --reset                resets all values to default
     --sign=app|3do         sign executable
```

To print out the current values of a 3DO AIF executable just include an input file. You can also combine that with the other options to confirm what gets set and their values. If you wish to create a new file set the output. The new file can be the same as the original if you wish to overwrite it. Be sure to re-sign if changing the values of a signed executable.


# BUILD

### Linux / macOS

```
$ git clone https://github.com/trapexit/modbin.git
$ cd modbin
$ make
```

### Windows (mingw)

```
$ sudo apt install gcc-mingw-w64
$ git clone https://github.com/trapexit/modbin.git
$ cd modbin
$ make -f Makefile.win32
$ make -f Makefile.win64
```


# LINKS

* https://3dodev.com
* https://3dodev.com/documentation/file_formats/3do_aif_header
* https://3dodev.com/ext/3DO/Portfolio_2.5/OnLineDoc/DevDocs/tktfldr/dbgfldr/Bdbga.html
