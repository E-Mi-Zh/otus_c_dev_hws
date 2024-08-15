#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include "hashtable.h"

/* Разбор аргументов командной строки */
void parse_args(int argc);

/* Вывод сообщения с синтаксисом вызова и краткой информацией о программе */
void print_usage(void);

/* Открывает файл на чтение*/
FILE* open_file(char* fname);

int main(int argc, char* argv[])
{
	FILE* input_file;
	char word[MAX_WORD_LENGTH];
	unsigned int i = 0;
	int c;

	/* Хеш-таблица с массивом слов */
	hashtable_t* hashtable;

	/* Анализируем аргументы */
	parse_args(argc);
	/* Открываем файл */
	input_file = open_file(argv[1]);
	/* Создаём массив слов */
	hashtable = hashtable_new(HASHTABLE_SIZE);
	/* Для каждого слова из файла */
	do {
		c = fgetc(input_file);
		/* Определяем и сохраняем слово */
		if (!isspace(c) && (c != EOF) && (i < MAX_WORD_LENGTH-1)) {
			word[i] = c;
			i++;
			continue;
		}
		word[i] = '\0';
		if (i > 0) {
			hashtable = hashtable_add(hashtable, word);
		}
		i = 0;
	} while (c != EOF);
	/* Закрываем файл */
	fclose(input_file);

	/* Печатаем таблицу */
	hashtable_print(hashtable);
	hashtable_destroy(hashtable);

	exit(EXIT_SUCCESS);
}

void parse_args(int argc)
{
	if (argc < 2) {
		fprintf(stderr, "Input file not specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}
}

void print_usage(void)
{
	printf("wc - count words in input file.\n");
	printf("Usage: wc input_file\n");
}

FILE* open_file(char* fname)
{
	FILE* input_file;
	input_file = fopen(fname, "r");
	if (input_file == NULL) {
		fprintf(stderr, "Error opening input file %s: %s! Exiting...\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return input_file;
}

