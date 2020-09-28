all: modbin

modbin: modbin.c simple-opt.h
	$(CC) -Os -I. -o modbin modbin.c
	strip --strip-all modbin

win: modbin-win32 modbin-win64

modbin-win32:
	i686-w64-mingw32-gcc -static -Os -I. -o modbin_32.exe modbin.c
	i686-w64-mingw32-strip --strip-all modbin_32.exe

modbin-win64:
	x86_64-w64-mingw32-gcc -static -Os -I. -o modbin_64.exe modbin.c
	x86_64-w64-mingw32-strip --strip-all modbin_64.exe

static:
	$(CC) -Os -static -I. -o modbin modbin.c
	strip --strip-all modbin

clean:
	rm -vf modbin modbin_32.exe modbin_64.exe
