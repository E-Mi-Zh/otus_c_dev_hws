all: dbstat

dbstat: dbstat.c
	$(CC) -o dbstat dbstat.c `pkg-config --libs sqlite3 --cflags sqlite3` -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o dbstat

.PHONY: all clean
