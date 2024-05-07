#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define JPEG_HEADER_LEN 2 
#define ZIP_HEADER_LEN 4 

int main(int argc, char* argv[])
{
	FILE* f;
	unsigned char jpeg_header[JPEG_HEADER_LEN] = { 0xFF, 0xD8 };
	unsigned char jpeg_end[JPEG_HEADER_LEN] = { 0xFF, 0xD9 };
	unsigned char buf[ZIP_HEADER_LEN];
	unsigned char zip_header[ZIP_HEADER_LEN] = { 0x50, 0x4b, 0x03, 0x04 };
	int res;

	if (argc < 2) {
		fprintf(stderr, "Input file not specified!\n");
		exit(EXIT_FAILURE);
	}

	f = fopen(argv[1], "rb");
	if (f == NULL) {
		fprintf(stderr, "Error opening file %s: %s! Exiting...\n", argv[1], strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Opened file %s, starting analyze...\n", argv[1]);

	res = fread(buf, 1, JPEG_HEADER_LEN, f);
	if (res < JPEG_HEADER_LEN) {
		fprintf(stderr, "File %s too short, read only %d bytes! Exiting...\n", argv[1], res);
		fclose(f);
		exit(EXIT_FAILURE);
	}

	res = memcmp(buf, jpeg_header, JPEG_HEADER_LEN);
	if (res == 0) {
		printf("JPEG signature detected! Trying to reach end...\n");
		do {
			buf[1] = buf[0];
			buf[0] = (unsigned char) fgetc(f);
#ifdef DEBUG
			printf("buf=%x%x\tpos=%ld\tfeof(f)=%d\tres=%d\n", buf[0], buf[1], ftell(f), feof(f), memcmp(buf, jpeg_end, JPEG_HEADER_LEN));
#endif
		} while ((res = memcmp(buf, jpeg_end, JPEG_HEADER_LEN) != 0) && !feof(f));
		if (res != 0) {
			printf("JPEG file end not detected! Possibly broken image, exiting...\n");
			fclose(f);
			exit(EXIT_FAILURE);
		} else {
			printf("JPEG end signature detected!\n");
		}
	printf("Begin archive signature search.\n");
	}


	exit(EXIT_SUCCESS);
}
