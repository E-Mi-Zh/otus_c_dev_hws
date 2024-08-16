all: crc_mmap

crc_mmap: crc_mmap.c crc32.c
	$(CC) -o crc_mmap crc_mmap.c crc32.c -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o crc_mmap

.PHONY: all clean
