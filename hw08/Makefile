all: weather

weather: weather.o libparson.a
	gcc -o weather -Wall -Wextra -Wpedantic -std=c11 weather.o -lcurl -lm -L. -lparson

weather.o: weather.c
	gcc -c -Wall -Wextra -Wpedantic -std=c11 weather.c

parson.o: parson.c
	gcc -c -Wall -Wextra -Wpedantic -std=c11 parson.c

libparson.a: parson.o
	ar cr libparson.a parson.o

clean:
	rm -rf *.o *.a weather
