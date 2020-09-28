# MODBIN

modbin is a tool that was included in the 3DO SDK and is used to modify the 3DO executable's slightly custom AIF header. A homebrew version was released but only supports setting the "work space" value and the stack size. This tool offers all the same features found in the original tool with a few minor extras. Should compile on any C99 C compiler.


# USAGE

```
Usage: ./modbin [options]... <file>

  modbin is used to set 3DO AIF header values.

  -h --help            print this help message and exit
  -V                   print modbin version
     --debug           enable debugging
     --nodebug         disable debugging
     --pri=INT         set priority
     --version=INT     set version number
     --flags=INT       set app flags
     --osversion=INT   set OS_version number
     --osrevision=INT  set OS_revision number
     --stack=INT       set stack size
     --freespace=INT   set freespace
     --maxusecs=INT    set maximum usecs
     --verbose         turn on verbose mode
     --reset           resets all values to default
```


# BUILD

### Linux / macOS

```
$ git clone https://github.com/trapexit/modbin.git
$ cd modbin
$ make
cc -Os -I. -o modbin modbin.c
strip --strip-all modbin
```

### Windows (mingw)

```
$ sudo apt install gcc-mingw-w64
$ git clone https://github.com/trapexit/modbin.git
$ cd modbin
$ make win
i686-w64-mingw32-gcc -static -Os -I. -o modbin_32.exe modbin.c
i686-w64-mingw32-strip --strip-all modbin_32.exe
x86_64-w64-mingw32-gcc -static -Os -I. -o modbin_64.exe modbin.c
x86_64-w64-mingw32-strip --strip-all modbin_64.exe
```


# LINKS

* https://3dodev.com
* https://3dodev.com/documentation/file_formats/3do_aif_header
* https://3dodev.com/ext/3DO/Portfolio_2.5/OnLineDoc/DevDocs/tktfldr/dbgfldr/Bdbga.html
