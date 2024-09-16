CC=gcc
#CC=clang
CFLAGS= -Wall -Wextra -Wpedantic -Werror -Wno-unused-result -O3 -ldm

all: torrentd

torrentd: torrentd.c
	$(CC) torrentd.c $(CFLAGS) -o torrentd

clean:
	rm -f torrentd
