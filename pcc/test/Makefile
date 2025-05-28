CC       := clang
CFLAGS   := -I. -I../include -Wall -Wextra -O3 -fPIC

LIBPCM   := ../lib/libpcm.so

all: libreno.so reno_main

libreno.so: reno.c reno.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -shared -o $@ reno.c -L../lib -lpcm

main.o: main.c reno.h
	$(CC) $(CFLAGS) -c $< -o $@

reno_main: main.o libreno.so $(LIBPCM)
	$(CC) main.o -L. -lreno -L../lib -lpcm -o $@

clean:
	rm -f *.o *.so reno_main

.PHONY: all clean
