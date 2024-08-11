all: demon

demon: demon.c
	$(CC) -o demon demon.c `pkg-config --cflags libconfuse --libs libconfuse` -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o demon

.PHONY: all clean
