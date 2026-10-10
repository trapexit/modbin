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
     --compress             compress executable
     --decompress           decompress executable
     --workspace=UNSIGNED   set full AIF workspace word (0..4294967295)
     --revision=UNSIGNED    set revision number (0..255)
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

An explicit `--workspace` overrides the marker written by other header setters
and `--reset`, regardless of option position. `--workspace=0` clears the 3DO
marker. When signing, the requested workspace word is included in the hash and
preserved in the output; without this option, the existing marker behavior is
unchanged.

`--compress` / `--decompress` implement the 3DO AIF order-1 byte-predictor
compression format (the one used by the stock Opera privileged folios). They
may be combined with the other flags; compress/decompress runs first and the
remaining flags (including `--sign`) are applied to the resulting binary.
`--compress` is idempotent: if the input is already compressed it is
decompressed in-memory first, then re-compressed. It declines (writes the
input unchanged and warns on stderr) when the decompressor stub's scratch
requirement cannot be met or the result would not be smaller than the input.


# BUILD

### Native build

Requires GNU Make and a native C/C++ compiler toolchain. Zig is not required.

```sh
$ git clone https://github.com/trapexit/modbin.git
$ cd modbin
$ make
$ ./build/modbin --help
```

The default build uses `-Os -flto -static` and produces `build/modbin`.
Use `make clean && make DEBUG=1` for an unoptimized debug build without static
linking or LTO. `SANITIZE=1` enables the undefined-behavior sanitizer.

Run arithmetic boundary tests, compare actual CLI signatures against an
independent Python MD5/RSA calculation, check workspace precedence and re-signing,
and check release-mode key helpers:

```sh
$ make test
$ make test DEBUG=1 SANITIZE=1 TARGET=ubsan
```

The sanitizer build uses a separate object directory; sanitizer diagnostics
are fatal in all regression runners. Key helper tests disable assertions to
verify that invalid names still terminate rather than return an undefined
value. The CLI signing and workspace tests also accept a runner prefix:

```sh
$ python3 tests/signing_test.py qemu-aarch64 build/modbin_aarch64-linux-musl
$ python3 tests/workspace_test.py qemu-aarch64 build/modbin_aarch64-linux-musl
```

### Release builds

Release builds use Zig to cross-compile four targets sequentially. Docker and
separate MinGW toolchains are no longer required. If `zig` is on `PATH`, it is
used directly; its version is not pinned. Otherwise, optionally provision the
pinned `ziglang==0.17.0` package:

```sh
$ make zig-venv
$ make release
```

`make zig-venv` uses the system Zig when available and otherwise creates `.venv`.
Provisioning requires Python 3 with `venv`/pip support and network access to
download the package. You can instead supply your own Zig installation.
Override `ZIG` to select the executable, `PYTHON` to select the provisioning
interpreter, or `ZIG_VENV` to change the virtual-environment directory (default:
`.venv`). Without a system Zig or an explicit `ZIG` override, builds use
`$(ZIG_VENV)/bin/python-zig`.

`make release` checks Zig before cleaning `build/`, then produces:

* `build/modbin_x86_64-linux-musl`
* `build/modbin_aarch64-linux-musl`
* `build/modbin_x86_64-windows-gnu.exe`
* `build/modbin_aarch64-macos`

Each target has its own object directory under `build/`. Linux releases use
static linking, LTO, section garbage collection, and linker stripping. Windows
uses static linking, section garbage collection, and linker stripping without
LTO. macOS uses dynamic linking without LTO, with Mach-O dead-strip/strip flags.
Release builds use `DEBUG=0` even if the invoking make specifies `DEBUG=1`.
The former 32-bit Windows release is no longer built.

### Install release tools into 3do-devkit

```sh
$ source /path/to/3do-devkit/activate-env
$ make zig-venv
$ make install-release
```

`install-release` requires `TDO_DEVKIT_PATH` from the sourced environment.
It runs the full `make release` build, then replaces the devkit's tools with:

* x86-64 Linux: `$TDO_DEVKIT_PATH/bin/tools/linux/modbin`
* x86-64 Windows: `$TDO_DEVKIT_PATH/bin/tools/win/modbin.exe`

The AArch64 Linux and macOS binaries remain in `build/`; the devkit uses the
Linux and Windows tool directories above. `DESTDIR` can stage the installation
without changing the active devkit, for example
`make install-release DESTDIR=/tmp/stage`.

### Vendored arithmetic

RSA signing uses the five core files from
[BigDigits 2.8.0](https://di-mgt.com.au/bigdigits.html) (MPL-2.0).
Digits remain 32-bit; the default implementation uses 64-bit intermediates
for multiplication and division. `USE_32ONLY` selects 32-bit-only arithmetic.

Local patches omit the unused compile-time timestamp accessors for Zig's
reproducible-build checks, avoid full-width shifts when the bit remainder is
zero, and construct bit masks in the unsigned digit type. Whole-digit shifts
capture carry before overwriting an aliased source buffer. The undefined-shift
fixes prevent incorrect RSA signatures in optimized builds. AIF header words
are also assembled as unsigned 32-bit values to avoid signed-shift overflow.

Key helpers accept only the nonnull names `app` and `3do`. An invalid internal
name terminates even when `NDEBUG` disables assertions; the CLI validates the
name before invoking the helpers.


# LINKS

* https://3dodev.com
* https://3dodev.com/documentation/file_formats/3do_aif_header
* https://3dodev.com/documentation/development/opera/pf25/tktfldr/dbgfldr/bdbga
