AS = nasm  # NOTE: yasm тоже подойдёт

all: asm-prog c-prog

asm-prog: main.o
	$(CC) -no-pie $^ -o $@ -g

main.o: main.asm
	$(AS) -felf64 $^ -g

c-prog: c-prog.c
	$(CC) -o c-prog c-prog.c -Wall -Wextra -Wpedantic -std=c11 -g

clean:
	$(RM) asm-prog c-prog *.o core

.PHONY: all clean
