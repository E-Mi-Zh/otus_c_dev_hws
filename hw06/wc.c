#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

/* Максимальная длина слова */
/* По статистике в русском и английском слова не более, чем 54-55 символов; взято с запасом */
#define MAX_WORD_LENGTH 64

/* Первоначальный размер таблицы с количеством слов */
#define WORDS 1 

void parse_args(int argc);
void print_usage(void);
FILE* open_file(char* fname);

typedef struct {
	char w[MAX_WORD_LENGTH];
	unsigned int cnt;
} wcnt_t;

wcnt_t* alloc_table(unsigned int sz);
unsigned int search_word(char* w, wcnt_t* words, unsigned int sz);
void print_table(wcnt_t* words, unsigned int sz);
unsigned int add_word(wcnt_t* *words, char* w, unsigned int sz); 
wcnt_t* extend_table(wcnt_t* words, unsigned int* sz); 

int main(int argc, char* argv[])
{
	FILE* f;
	char w[MAX_WORD_LENGTH];
	unsigned int i = 0;
	unsigned int ind;
	int c;
	unsigned int tsize;

	/* В первой версии программы будем использовать массив */
	wcnt_t* words;

	/* Анализируем аргументы */
	parse_args(argc);
	/* Открываем файл */
	f = open_file(argv[1]);
	/* Создаём массив слов */
	tsize = WORDS;
	words = alloc_table(tsize);
	/* Для каждого слова из файла */
	do {
		c = fgetc(f);
		/* Определяем и сохраняем слово */
		if (!isspace(c) && (c != EOF) && (i < MAX_WORD_LENGTH-1)) {
			w[i] = c;
			i++;
			continue;
		}
		w[i] = '\0';
		if (i > 0) {
			/* Ищем слово в массиве */
			ind = search_word(w, words, tsize);
			if (ind < tsize) {
				/* Если в массиве уже есть это слово, то увеличиваем счётчик */
				words[ind].cnt++;
			} else {
			/* Если нет, то сохраняем слово и увеличиваем счётчик */
				tsize = add_word(&words, w, tsize);
			}
		}
		i = 0;
	} while (c != EOF);
	/* Закрываем файл */
	fclose(f);

	/* Печатаем таблицу */
	print_table(words, tsize);
	free(words);

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
	FILE* f;
	f = fopen(fname, "r");
	if (f == NULL) {
		fprintf(stderr, "Error opening input file %s: %s! Exiting...\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return f;
}

void print_table(wcnt_t* words, unsigned int sz)
{
	for (unsigned int i = 0; i < sz; i++) {
		if (words[i].cnt > 0) {
			printf("Word %s appears %d times.\n", words[i].w, words[i].cnt);
		}
	}
}

wcnt_t* alloc_table(unsigned int sz)
{
	wcnt_t* w;

	w = malloc(sz * sizeof(wcnt_t));
	if (!w) {
		fprintf(stderr, "Can't allocate words table: %s! Exiting...\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (unsigned int i = 0; i < sz; i++) {
		w[i].cnt = 0;
	}

	return w;
}

unsigned int search_word(char* w, wcnt_t* words, unsigned int sz)
{
	unsigned int i;
	
	for (i = 0; i < sz; i++) {
		if (!strcmp(w, words[i].w)) {
			return i;
		}
	}
	return i;
}

unsigned int add_word(wcnt_t* *words, char* w, unsigned int sz)
{
	unsigned int i;
	wcnt_t* nw = *words;

	for (i = 0; i < sz; i++) {
		if (nw[i].cnt == 0) {
			strcpy(nw[i].w, w);
			nw[i].cnt++;
			return sz;
		}
	}
	nw = extend_table(nw, &sz);
	strcpy(nw[i].w, w);
	nw[i].cnt++;
	*words = nw;
	return sz;
}

wcnt_t* extend_table(wcnt_t* words, unsigned int* sz)
{
	words = (wcnt_t*) realloc(words, *sz * 2 * sizeof(wcnt_t));
	if (!words) {
		fprintf(stderr, "Failed to extend words table to size %d: %s! Exiting...", *sz * 2, strerror(errno));
		exit(EXIT_FAILURE);
	}
	for (unsigned int i = *sz; i < *sz * 2; i++) {
		words[i].cnt = 0;
	}
	*sz = *sz * 2;

	return words;
}