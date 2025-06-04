CC       := clang
CFLAGS   := -I. -I../include -Wall -Wextra -O3 -fPIC

LIBPCM   := ../lib/libpcm.so

all: libreno_accum.so libreno_std.so app_main

libreno_accum.so: reno_accumulated.c reno.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -shared -o $@ reno_accumulated.c -L../lib -lpcm

libreno_std.so: reno_standard.c reno.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -shared -o $@ reno_standard.c -L../lib -lpcm

main.o: main.c reno.h
	$(CC) $(CFLAGS) -c $< -o $@

app_main: main.o libreno_accum.so libreno_std.so $(LIBPCM)
	$(CC) main.o -L../lib -lpcm -o $@

clean:
	rm -f *.o *.so app_main

.PHONY: all clean
