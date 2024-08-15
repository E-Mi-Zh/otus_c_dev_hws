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
static int search_word(hashtable_t* hashtable, char* word);

/* Возвращает индекс в массиве слов */
static unsigned int get_hash_index(char* word, unsigned int size);

/* Хеш-функция от строки */
static unsigned long long hash(char* w);

/* Проверят, свободна ли ячейка или нет */
static bool empty_cell(wcnt_t* words, char* word, unsigned int index);

/* Добавляет слово в заданную ячейку массива */
static void add_word(wcnt_t* words, char* word, unsigned int size);

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
	hashtable->occupied = 0;
	hashtable->max_occupied = size * MAX_LOAD_FACTOR;

	for (unsigned int i = 0; i < size; i++) {
		hashtable->words[i].count = 0;
		hashtable->words[i].word[0] = '\0';
	}

	return hashtable;
}


void hashtable_print(hashtable_t* hashtable)
{
	for (unsigned int i = 0; i < hashtable->size; i++) {
		if (hashtable->words[i].count > 0) {
			printf("Word \"%s\" appears %d times.\n", hashtable->words[i].word, hashtable->words[i].count);
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
	int index = -1;

	/* Ищем индекс для слова в массиве */
	while ((index = search_word(hashtable, word)) < 0) {
		/* не смогли найти свободной ячейки для слова, увеличиваем таблицу и пытаемся опять */
		hashtable = hashtable_extend(hashtable);
	}
	/* Сохраняем слово */
	add_word(hashtable->words, word, index);

	hashtable->occupied = hashtable->occupied + 1;
	if (hashtable->occupied == hashtable->max_occupied) {
		/* Увеличиваем таблицу */
		hashtable = hashtable_extend(hashtable);
	}

	return hashtable;
}

static int search_word(hashtable_t* hashtable, char* word)
{
	unsigned int index;
	unsigned int i = 0;

	index = get_hash_index(word, hashtable->size);

	while (i < hashtable->size) {
		/* Начинаем с индекса от хеш-функции */
		/* Если ячейка свободна, то используем её */
		if (empty_cell(hashtable->words, word, index)) {
			return index;
		}
		/* Ячейка занята, берём следующую */
		index++;
		if (index == hashtable->size) {
			/* Дошли до конца массива, продолжаем поиск с начала */
			index = 0;
		}
		i++;
	}
	fprintf(stderr, "Can't find free cell in table!\n");
	return -1;
}


static unsigned int get_hash_index(char* word, unsigned int size)
{
	return (hash(word) % size);
}


static unsigned long long hash(char* word)
{
	unsigned long long hash = 0;
	unsigned long long p_pow = 1;
	size_t str_len;

	str_len = strlen(word);
	for (size_t i = 0; i < str_len; i++) {
		hash = hash + (word[i] - HASH_BASE_SYMBOL) * p_pow;
		p_pow = p_pow * HASH_P;
	}
	return hash;
}


static bool empty_cell(wcnt_t* words, char* word, unsigned int index)
{
	if (words[index].word[0] == '\0') {
		/* пустая ячейка */
		return true;
	}

	if (!strcmp(word, words[index].word)) {
		/* ячейка с таким же словом */
		return true;
	}

	/* коллизия */
	return false;
}


static void add_word(wcnt_t* words, char* word, unsigned int index)
{
	if (words[index].word[0] == '\0') {
		/* Пустая ячейка, копируем слово */
		strcpy(words[index].word, word);
	}
	words[index].count++;
}

static hashtable_t* hashtable_extend(hashtable_t* hashtable)
{
	hashtable_t* new_hashtable;
	unsigned int index;

	new_hashtable = hashtable_new(hashtable->size * EXTEND_FACTOR);

	for (unsigned int i = 0; i < hashtable->size; i++) {
		if (hashtable->words[i].word[0] != '\0') {
			/* Ячейка в старой таблице непуста, копируем её в новую */
			index = search_word(new_hashtable, hashtable->words[i].word);
			add_word(new_hashtable->words, hashtable->words[i].word, index);
			new_hashtable->words[index].count = hashtable->words[i].count;
		}
	}

	hashtable_destroy(hashtable);

	return new_hashtable;
}