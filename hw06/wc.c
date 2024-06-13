#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

/* Максимальная длина слова */
/* По статистике в русском и английском слова не более, чем 54-55 символов; взято с запасом */
#define MAX_WORD_LENGTH 64

/* Первоначальный размер таблицы с количеством слов */
#define WORDS 4 

/* Основание для полинома хеш функции */
/* Исходим из того, что входящие слова могут содержать печатные символы латиницы */
/* (большие и маленькие), печатные символы (цифры, знаки пунктуации) - всего 95 */
/* плюс 66 символов кириллицы (большие и маленькие) - всего 161 символ */
#define HASH_P 163

/* граница непечатных символов (пробел) */
#define HASH_BASE_SYMBOL 32


/* Разбор аргументов командной строки */
void parse_args(int argc);

/* Вывод сообщения с синтаксисом вызова и краткой информацией о программе */
void print_usage(void);

/* Открывает файл на чтение*/
FILE* open_file(char* fname);

/* Ячейка массива слов */
typedef struct {
	char w[MAX_WORD_LENGTH];
	unsigned int cnt;
} wcnt_t;

/* Выделяет память и возвращает инициализированный массив */
wcnt_t* alloc_table(unsigned int sz);

/* Возвращает индекс ячейки для заданного слова */
/* с использованием хеш-функции. */
/* Ячейка либо свободная, либо содержащая это же слово. */
/* В случае коллизии используется линейное пробирование. */
unsigned int search_word(wcnt_t* words, char* w, unsigned int sz);

/* Проверят, свободна ли ячейка или нет */
bool empty_cell(wcnt_t* words, char* w, unsigned int ind); 

/* Печатает массив слов */
void print_table(wcnt_t* words, unsigned int sz);

/* Добавляет слово в заданную ячейку массива */
void add_word(wcnt_t* words, char* w, unsigned int sz); 

/* Увеличивает в два раза массив слов */
wcnt_t* extend_table(wcnt_t* words, unsigned int* sz); 

/* Хеш-функция от строки */
unsigned long long hash(char* s);

/* Возвращает индекс в массиве слов */
int get_ind(char* word, unsigned int size);

int main(int argc, char* argv[])
{
	FILE* f;
	char w[MAX_WORD_LENGTH];
	unsigned int i = 0;
	unsigned int ind;
	int c;
	/* Размер массива слов */
	unsigned int tsize = WORDS;
	/* Сколько занято ячеек */
	unsigned int t_occ = 0;
	/* Максимально возможное количество занятых ячеек до расширения */
	unsigned int t_max_occ = 0.75 * tsize;
	

	/* Массив слов */
	wcnt_t* words;

	/* Анализируем аргументы */
	parse_args(argc);
	/* Открываем файл */
	f = open_file(argv[1]);
	/* Создаём массив слов */
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
			/* Ищем индекс для слова в массиве */
			ind = search_word(words, w, tsize);
			/* Сохраняем слово */
			add_word(words, w, ind);
			t_occ = t_occ + 1;
			if (t_occ == t_max_occ) {
				/* Увеличиваем таблицу */
				words = extend_table(words, &tsize);
				t_max_occ = 0.75 * tsize;
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
		w[i].w[0] = '\0';
	}

	return w;
}

unsigned int search_word(wcnt_t* words, char* w, unsigned int sz)
{
	unsigned int ind;
	unsigned int i, j;

	ind = get_ind(w, sz);

	i = ind;
	j = 0;

	while (j < sz) {
		/* Начинаем с индекса от хеш-функции */
		/* Если ячейка свободна, то используем её */
		if (empty_cell(words, w, i)) {
			return i;
		}
		/* Ячейка занята, берём следующую */
		i++;
		if (i == sz) {
			/* Дошли до конца массива, продолжаем поиск с начала */
			i = 0;
		}
		j++;
	}
	fprintf(stderr, "Can't find free cel in table!\n");
	exit(EXIT_FAILURE);
}

bool empty_cell(wcnt_t* words, char* w, unsigned int ind) {
	if (words[ind].w[0] == '\0') {
		/* пустая ячейка */
		return true;
	}
   
	if (!strcmp(w, words[ind].w)) {
		/* ячейка с таким же словом */
		return true;
	}
   
	/* коллизия */
	return false;
}

void add_word(wcnt_t* words, char* w, unsigned int ind)
{
	if (words[ind].w[0] == '\0') {
		/* Пустая ячейка, копируем слово */
		strcpy(words[ind].w, w);
	}
	words[ind].cnt++;
}

wcnt_t* extend_table(wcnt_t* words, unsigned int* sz)
{
	wcnt_t* nw;
	unsigned int ind;
	unsigned int nsz = *sz * 2;

	nw = alloc_table(nsz);

	for (unsigned int i = 0; i < *sz; i++) {
		if (words[i].w[0] != '\0') {
			/* Ячейка в старой таблице непуста, копируем её в новую */
			ind = search_word(nw, words[i].w, nsz);
			add_word(nw, words[i].w, ind);
			nw[ind].cnt = words[i].cnt;
		}
	}

	*sz = nsz;
	free(words);
	return nw;
}

unsigned long long hash(char* s)
{
	unsigned long long hash = 0;
	unsigned long long p_pow = 1;
	size_t str_len;

	str_len = strlen(s);
	for (size_t i = 0; i < str_len; i++) {
		hash = hash + (s[i] - HASH_BASE_SYMBOL) * p_pow;
		p_pow = p_pow * HASH_P;
	}
	return hash;
}

int get_ind(char* word, unsigned int size)
{
	return (hash(word) % size);
}
