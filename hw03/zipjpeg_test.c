#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int main(int argc, char* argv[])
{
	FILE* f;

	if (argc != 2) {
		fprintf(stderr, "Input file not specified!\n");
		exit(EXIT_FAILURE);
	}

	f = fopen(argv[1], "rb");
	if (f == NULL) {
		fprintf(stderr, "Error opening file %s: %s! Exiting...\n", argv[1], strerror(errno));
		exit(EXIT_FAILURE);
	}
	fclose(f);

	exit(EXIT_SUCCESS);
}
