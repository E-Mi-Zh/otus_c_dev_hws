all: cube

cube: cube.c
	$(CC) -o cube cube.c -Wall -Wextra -Wpedantic -std=c11 -lglut -lGL -lGLU

clean:
	$(RM) *.o cube

.PHONY: all clean
