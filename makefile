CC=gcc
#CC=clang
CFLAGS= -Wall -Wextra -Wpedantic -Werror -Wno-unused-result -O3 -ldm

all: torrentd

.PHONY:
install_deps:
	git submodule update --init
	make -C localnotify/ install
	make -C diskmap/ install

torrentd: torrentd.c
	$(CC) torrentd.c $(CFLAGS) -o torrentd

clean:
	rm -f torrentd
