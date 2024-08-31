all: aparser

aparser: aparser.c
	$(CC) -o aparser aparser.c `pkg-config --libs glib-2.0 --cflags glib-2.0` -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o aparser

.PHONY: all clean
