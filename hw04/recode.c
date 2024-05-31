#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "codepages.h"

/* Параметры командной строки: 0 - имя программы, 1 - входной файл */
/* 2 - кодировка входного файла, 3 - выходной файл. */
#define N_ARGS 4
enum args {
	FI = 1,
	ENC = 2,
	FO = 3
};

typedef enum {
	CP_1251,
	KOI8_R,
	ISO_8859_5
} encoding_t;

FILE* fi;
FILE* fo;
encoding_t encoding; 

void parse_args(int argc, char* argv[]);
void print_usage(void);
void open_files(char* argv[]);
uint8_t encode_symbol(uint8_t symbol, uint8_t utf8[4]);
uint32_t get_uni_symbol(uint8_t symbol);
uint8_t count_octets(uint32_t uni_symbol);
void set_higher_bits(uint8_t utf8[4], uint8_t n_octets);
void set_lower_bits(uint8_t utf8[4], uint8_t n_octets, uint32_t uni_symbol);
void close_files(void);

int main(int argc, char* argv[]) {
	int c;
	uint8_t utf8[4] = {0};
	uint8_t n_octets;
	int res;
	/* Обрабатываем ключи командной строки */
	parse_args(argc, argv);

	/* Открываем файлы */
	open_files(argv);

	/* Для каждого входного символа*/
	while ((c = fgetc(fi)) != EOF) {
		/* Перекодируем символ */
		n_octets = encode_symbol((uint8_t) c, utf8);

		/* Записываем выходные байты */
		res = fwrite(&utf8, sizeof(utf8[0]), n_octets, fo);
		if (res < n_octets) {
			fprintf(stderr, "Error writing output file, wanted %d wrote %d!\n", n_octets, res);
		}
	}

	/* Закрываем файлы */
	close_files();

	exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		if (!(strcmp(argv[i], "-h")) || !(strcmp(argv[i], "--help"))) {
			print_usage();
			exit(EXIT_SUCCESS);
		}
	}

	if (argc < N_ARGS) {
		fprintf(stderr, "Not all parameters are specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (!strcmp(argv[ENC], "KOI8-R")) {
		encoding = KOI8_R;
	} else if (!strcmp(argv[ENC], "CP-1251")) {
		encoding = CP_1251;
	} else if (!strcmp(argv[ENC], "ISO-8859-5")) {
		encoding = ISO_8859_5;
	} else {
		fprintf(stderr, "Invalid encoding specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}
}

void print_usage(void) {
	printf("Recode input file in given encoding to output file in UTF-8.\n");
	printf("Usage: recode input_file encoding output_file\n");
	printf("Supported input encodings: KOI8-R, CP-1251, ISO-8859-5\n");
	printf("Example: recode file1 KOI8-R file2\n");
}

void open_files(char* argv[]) {
	fi = fopen(argv[FI], "rb");
	if (fi == NULL) {
		fprintf(stderr, "Error opening input file %s: %s! Exiting...\n", argv[FI], strerror(errno));
		exit(EXIT_FAILURE);
	}

	fo = fopen(argv[FO], "wb");
	if (fo == NULL) {
		fprintf(stderr, "Error opening output file %s: %s! Exiting...\n", argv[FI], strerror(errno));
		exit(EXIT_FAILURE);
	}
}

uint8_t encode_symbol(uint8_t symbol, uint8_t utf8[4]) {
	uint32_t uni_symbol;
	uint8_t n_octets;

	/* Определяем символ Юникода по кодовой таблице для данной кодировки */
	uni_symbol = get_uni_symbol((uint8_t) symbol); 
	/* Определяем количество октетов, необходимых для записи */
	n_octets = count_octets(uni_symbol);
	/* Устанавливаем старшие биты октетов */
	set_higher_bits(utf8, n_octets);
	/* Устанавливаем значащие (младшие) биты октетов */
	set_lower_bits(utf8, n_octets, uni_symbol);
	return n_octets;
}

uint32_t get_uni_symbol(uint8_t symbol) {
	uint32_t* codepage_table;

	switch (encoding) {
		case CP_1251:
			codepage_table = &win_1251[0];
			break;
		case KOI8_R:
			codepage_table = &koi8_r[0];
			break;
		case ISO_8859_5:
			codepage_table = &iso_8859_5[0];
			break;
		default:
			codepage_table = &koi8_r[0];
			break;
	}
	if ((symbol - 128) >= 0) {
		return codepage_table[symbol-128];
	}
	return symbol;
}

uint8_t count_octets(uint32_t uni_symbol) {
	if (uni_symbol <= 0x7F) {
		return 1;
	} else if (uni_symbol <= 0x7FF) {
		return 2;
	} else if (uni_symbol <= 0xFFFF) {
		return 3;
	}
	return 4;
}

void set_higher_bits(uint8_t utf8[4], uint8_t n_octets) {
	switch (n_octets) {
		case 1:
			utf8[0] = 0;
			break;
		case 2:
			utf8[0] = 0xC0;
			utf8[1] = 0x80;
			break;
		case 3:
			utf8[0] = 0xE0;
			utf8[1] = 0x80;
			utf8[2] = 0x80;
			break;
		case 4:
			utf8[0] = 0xE0;
			utf8[1] = 0x80;
			utf8[2] = 0x80;
			utf8[3] = 0x80;
			break;
		default:
			fprintf(stderr, "%s: %d invalid number of octets!\n", __func__, __LINE__);
			exit(EXIT_FAILURE);
	}
}

void set_lower_bits(uint8_t utf8[4], uint8_t n_octets, uint32_t uni_symbol) {
	switch (n_octets) {
		case 1:
			utf8[0] = utf8[0] | (uni_symbol & 0x7F);
			break;
		case 2:
			utf8[0] = utf8[0] | ((uni_symbol >> 6) & 0x1F);
			utf8[1] = utf8[1] | (uni_symbol & 0x3F);
			break;
		case 3:
			utf8[0] = utf8[0] | ((uni_symbol >> 12) & 0xF);
			utf8[1] = utf8[1] | ((uni_symbol >> 6) & 0x3F);
			utf8[2] = utf8[2] | (uni_symbol & 0x3F);
			break;
		case 4:
			utf8[0] = utf8[0] | ((uni_symbol >> 18) & 0x7);
			utf8[1] = utf8[1] | ((uni_symbol >> 12) & 0x3F);
			utf8[2] = utf8[2] | ((uni_symbol >> 6) & 0x3F);
			utf8[3] = utf8[3] | (uni_symbol & 0x3F);
			break;
		default:
			fprintf(stderr, "%s: %d invalid number of octets!\n", __func__, __LINE__);
			exit(EXIT_FAILURE);
	}
}

void close_files(void) {
	fclose(fi);
	fclose(fo);
}
