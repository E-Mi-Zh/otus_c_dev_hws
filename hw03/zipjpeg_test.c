#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#define JPEG_HEADER_LEN 2
#define ZIP_HEADER_LEN 2
#define ZIP_HEADER_TYPE_LEN 2
#define MAX_FILENAME_LENGTH 256
#define LOCAL_HEADER_SKIP_BYTES 22
#define ZIP_FILENAME_LEN 2

enum zip_record {
	LOCAL_FILE_HEADER,
	END_OF_CENTRAL_DIRECTORY
};

void print_usage(void);
bool signature_search(FILE* f, unsigned char* signature, unsigned int length);
int check_zip_header(FILE* f);
int get_filename(FILE* f, unsigned char* fname);

int main(int argc, char* argv[])
{
	FILE* f;
	unsigned char jpeg_header[JPEG_HEADER_LEN] = { 0xFF, 0xD8 };
	unsigned char jpeg_end[JPEG_HEADER_LEN] = { 0xFF, 0xD9 };
	unsigned char fname[MAX_FILENAME_LENGTH];
	unsigned char zip_header[ZIP_HEADER_LEN] = { 0x50, 0x4b };

	if (argc < 2) {
		fprintf(stderr, "Input file not specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	f = fopen(argv[1], "rb");
	if (f == NULL) {
		fprintf(stderr, "Error opening file %s: %s! Exiting...\n", argv[1], strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("Opened file %s, starting analyze...\n", argv[1]);

	if (signature_search(f, jpeg_header, JPEG_HEADER_LEN)) {
		printf("JPEG signature detected! Trying to reach end...\n");
		if (signature_search(f, jpeg_end, JPEG_HEADER_LEN)) {
			printf("JPEG end signature detected!\n");
		} else {
			printf("JPEG file end not detected! Possibly broken image, exiting...\n");
			fclose(f);
			exit(EXIT_FAILURE);
		}
	} else {
		printf("Hmm, JPEG header not found, maybe simple archive?..\n");
		rewind(f);
	}

	printf("Begin archive signature search.\n");
	while ((!feof(f)) && (signature_search(f, zip_header, ZIP_HEADER_LEN))) {
		switch (check_zip_header(f)) {
			case LOCAL_FILE_HEADER:
				if (!get_filename(f, fname)) {
					printf("Found ZIP with file %s\n", fname);
				}
				break;
			case END_OF_CENTRAL_DIRECTORY:
				printf("ZIP file end (EOCD  record) found.\n");
				fclose(f);
				exit(EXIT_SUCCESS);
			case -1:
				break;
			default:
				break;
		}
	}

	fclose(f);
	exit(EXIT_SUCCESS);
}

bool signature_search(FILE* f, unsigned char signature[], unsigned int length) {
	unsigned int pos = 0;
	bool found = false;
	int c;

	while ((!found) && ((c = fgetc(f)) != EOF)) {
		if (signature[pos] == (unsigned char) c) {
			pos++;
		} else {
			pos = 0;
		}
		if (pos == length) {
			found = true;
		}
	}
	if (feof(f)) {
		fprintf(stderr, "End of file reached.\n");
	}

	return(found);
}

int check_zip_header(FILE* f) {
	unsigned char buf[ZIP_HEADER_TYPE_LEN];
	unsigned char zip_file_header[ZIP_HEADER_LEN] = { 0x03, 0x04 };
	unsigned char zip_eocd_record[ZIP_HEADER_LEN] = { 0x05, 0x06 };

	if (fread(buf, 1, ZIP_HEADER_TYPE_LEN, f) < ZIP_HEADER_TYPE_LEN) {
		fprintf(stderr, "End of file reached.\n");
		return(EXIT_FAILURE);
	}
	if (!memcmp(buf, zip_file_header, ZIP_HEADER_TYPE_LEN)) {
		return(LOCAL_FILE_HEADER);
	} else {
		if (!memcmp(buf, zip_eocd_record, ZIP_HEADER_TYPE_LEN)) {
			return(END_OF_CENTRAL_DIRECTORY);
		}
	}

	fseek(f, -ZIP_HEADER_TYPE_LEN, SEEK_CUR);
	return(EXIT_FAILURE);
}

void print_usage(void) {
	printf("Usage: zipjpeg_test filename\n");
}

int get_filename(FILE* f, unsigned char* fname) {
	uint16_t size;
	int res;

	fseek(f, LOCAL_HEADER_SKIP_BYTES, SEEK_CUR);
	res = fread(&size, 1, ZIP_FILENAME_LEN, f);
	if (res < ZIP_FILENAME_LEN) {
		fprintf(stderr, "Failed to read zip filename size: wanted %d, get only %d!\n", ZIP_FILENAME_LEN, res);
		return(EXIT_FAILURE);
	}
	if (size >= MAX_FILENAME_LENGTH) {
		size = MAX_FILENAME_LENGTH - 1;
	}
	if (fseek(f, 2, SEEK_CUR)) {
		fprintf(stderr, "Error seeking in file: %s.\n", strerror(errno));
		return(EXIT_FAILURE);
	}
	res = fread(fname, 1, size, f);
	if (res < size) {
		fprintf(stderr, "Failed to read zip filename: wanted %d, get only %d!\n", size, res);
		return(EXIT_FAILURE);
	}
	fname[res] = '\0';
	return(EXIT_SUCCESS);
}
