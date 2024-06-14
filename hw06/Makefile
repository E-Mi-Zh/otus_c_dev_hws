all: wc

wc: wc.o hashtable.o
	gcc -o wc -Wall -Wextra -Wpedantic -std=c11 wc.o hashtable.o

wc.o: wc.c
	gcc -c -Wall -Wextra -Wpedantic -std=c11 wc.c

hashtable.o: hashtable.c
	gcc -c -Wall -Wextra -Wpedantic -std=c11 hashtable.c

clean:
	rm -rf *.o wc
