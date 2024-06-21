#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "hashtable.h"


/* Возвращает индекс ячейки для заданного слова */
/* с использованием хеш-функции. */
/* Ячейка либо свободная, либо содержащая это же слово. */
/* В случае коллизии используется линейное пробирование. */
static unsigned int search_word(hashtable_t* hashtable, char* w);

/* Возвращает индекс в массиве слов */
static unsigned int get_ind(char* word, unsigned int size);

/* Хеш-функция от строки */
static unsigned long long hash(char* w);

/* Увеличивает таблицу в EXTEND_FACTOR раз  */
static hashtable_t* hashtable_extend(hashtable_t* hashtable);

/* Проверят, свободна ли ячейка или нет */
static bool empty_cell(wcnt_t* words, char* w, unsigned int ind);

/* Добавляет слово в заданную ячейку массива */
static void add_word(wcnt_t* words, char* w, unsigned int sz);

/* Расширяет таблицу, копируя старую в новую */
static hashtable_t* hashtable_extend(hashtable_t* hashtable);


hashtable_t* hashtable_new(unsigned int size)
{
	hashtable_t* hashtable;

	hashtable = malloc(sizeof(hashtable_t));
	if (!hashtable) {
		fprintf(stderr, "Can't allocate hashtable: %s! Exiting...\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	hashtable->words = malloc(size * sizeof(wcnt_t));
	if (!hashtable->words) {
		fprintf(stderr, "Can't allocate hashtable words array: %s! Exiting...\n", strerror(errno));
		free(hashtable);
		exit(EXIT_FAILURE);
	}

	hashtable->size = size;
	hashtable->occ = 0;
	hashtable->max_occ = size * MAX_LOAD_FACTOR;

	for (unsigned int i = 0; i < size; i++) {
		hashtable->words[i].cnt = 0;
		hashtable->words[i].w[0] = '\0';
	}

	return hashtable;
}


void hashtable_print(hashtable_t* hashtable)
{
	for (unsigned int i = 0; i < hashtable->size; i++) {
		if (hashtable->words[i].cnt > 0) {
			printf("Word \"%s\" appears %d times.\n", hashtable->words[i].w, hashtable->words[i].cnt);
		}
	}
}


void hashtable_destroy(hashtable_t* hashtable)
{
	if (hashtable->words) {
		free(hashtable->words);
		hashtable->words = NULL;
	}

	if (hashtable) {
		free(hashtable);
		hashtable = NULL;
	}
}


hashtable_t* hashtable_add(hashtable_t* hashtable, char* word)
{
	unsigned int ind;

	/* Ищем индекс для слова в массиве */
	ind = search_word(hashtable, word);
	/* Сохраняем слово */
	add_word(hashtable->words, word, ind);

	hashtable->occ = hashtable->occ + 1;
	if (hashtable->occ == hashtable->max_occ) {
		/* Увеличиваем таблицу */
		hashtable = hashtable_extend(hashtable);
	}

	return hashtable;
}

static unsigned int search_word(hashtable_t* hashtable, char* w)
{
	unsigned int ind;
	unsigned int i, j;

	ind = get_ind(w, hashtable->size);

	i = ind;
	j = 0;

	while (j < hashtable->size) {
		/* Начинаем с индекса от хеш-функции */
		/* Если ячейка свободна, то используем её */
		if (empty_cell(hashtable->words, w, i)) {
			return i;
		}
		/* Ячейка занята, берём следующую */
		i++;
		if (i == hashtable->size) {
			/* Дошли до конца массива, продолжаем поиск с начала */
			i = 0;
		}
		j++;
	}
	fprintf(stderr, "Can't find free cell in table!\n");
	exit(EXIT_FAILURE);
}


static unsigned int get_ind(char* word, unsigned int size)
{
	return (hash(word) % size);
}


static unsigned long long hash(char* w)
{
	unsigned long long hash = 0;
	unsigned long long p_pow = 1;
	size_t str_len;

	str_len = strlen(w);
	for (size_t i = 0; i < str_len; i++) {
		hash = hash + (w[i] - HASH_BASE_SYMBOL) * p_pow;
		p_pow = p_pow * HASH_P;
	}
	return hash;
}


static bool empty_cell(wcnt_t* words, char* w, unsigned int ind)
{
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


static void add_word(wcnt_t* words, char* w, unsigned int ind)
{
	if (words[ind].w[0] == '\0') {
		/* Пустая ячейка, копируем слово */
		strcpy(words[ind].w, w);
	}
	words[ind].cnt++;
}

static hashtable_t* hashtable_extend(hashtable_t* hashtable)
{
	hashtable_t* new_hashtable;
	unsigned int ind;

	new_hashtable = hashtable_new(hashtable->size * EXTEND_FACTOR);

	for (unsigned int i = 0; i < hashtable->size; i++) {
		if (hashtable->words[i].w[0] != '\0') {
			/* Ячейка в старой таблице непуста, копируем её в новую */
			ind = search_word(new_hashtable, hashtable->words[i].w);
			add_word(new_hashtable->words, hashtable->words[i].w, ind);
			new_hashtable->words[ind].cnt = hashtable->words[i].cnt;
		}
	}

	hashtable_destroy(hashtable);

	return new_hashtable;
}