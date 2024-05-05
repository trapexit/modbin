# MODBIN

The original `modbin` was a tool included in the 3DO SDK and is used
to modify 3DO executable's slightly custom AIF header. It was also the
name of a more advanced tool used by the 3DO Company to modify AIF
headers more extensively and in combination with RSA signing tool to
enable additional feature such as running libraries and tasks in a
privileged mode.

This tool offers all the same features found in the original `modbin`
tools and the RSA signing tool with a few minor extras. Should
compile with any C99 C compiler.


# USAGE

```
Usage: modbin [options]... <input-file> [<output-file>]

  modbin is used to set 3DO AIF header values and sign executables.

  -h --help                 print this help message and exit
  -V                        print modbin version
     --debug                enable debugging
     --nodebug              disable debugging
     --subsystype=UNSIGNED  set folio subtype
     --type=UNSIGNED        set folio node type
     --pri=UNSIGNED         set priority
     --version=UNSIGNED     set version number
     --flags=UNSIGNED       set app flags
     --osversion=UNSIGNED   set OS_version number
     --osrevision=UNSIGNED  set OS_revision number
     --stack=UNSIGNED       set stack size
     --freespace=UNSIGNED   set freespace
     --maxusecs=UNSIGNED    set maximum usecs
     --name=STRING          executable name
     --time                 set time
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
$ make release
```

### Windows (mingw)

Same as Linux's `make release`. Uses an Alpine container to cross compile.


# LINKS

* https://3dodev.com
* https://3dodev.com/documentation/file_formats/3do_aif_header
* https://3dodev.com/documentation/development/opera/pf25/tktfldr/dbgfldr/bdbga
