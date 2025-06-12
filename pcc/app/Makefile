CC       := clang
CFLAGS   := -I. -I../include -Wall -Wextra -O3 -fPIC

LIBPCM   := ../lib/libpcm.so

all: libreno_accum.so libreno_std.so libdctcp.so libswift.so app_main

libreno_accum.so: reno_accumulated.c tcp.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -shared -o $@ reno_accumulated.c -L../lib -lpcm

libreno_std.so: reno_standard.c tcp.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -shared -o $@ reno_standard.c -L../lib -lpcm

libdctcp.so: dctcp.c tcp.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP -shared -o $@ dctcp.c -L../lib -lpcm

libswift.so: swift.c swift.h $(LIBPCM)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -shared -o $@ swift.c -L../lib -lpcm

main.o: main.c tcp.h
	$(CC) $(CFLAGS) -c $< -o $@

app_main: main.o libreno_accum.so libreno_std.so libdctcp.so libswift.so $(LIBPCM)
	$(CC) main.o -L../lib -lpcm -o $@

clean:
	rm -f *.o *.so app_main

.PHONY: all clean
