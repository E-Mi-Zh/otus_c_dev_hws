#ifndef HASHTABLE_H
#define HASHTABLE_H

/* Максимальная длина слова */
/* По статистике в русском и английском слова не более, чем 54-55 символов; взято с запасом */
#define MAX_WORD_LENGTH 64

/* Первоначальный размер таблицы с количеством слов */
#define HASHTABLE_SIZE 4

/* Основание для полинома хеш функции */
/* Исходим из того, что входящие слова могут содержать печатные символы латиницы */
/* (большие и маленькие), печатные символы (цифры, знаки пунктуации) - всего 95 */
/* плюс 66 символов кириллицы (большие и маленькие) - всего 161 символ */
#define HASH_P 163

/* граница непечатных символов (пробел) */
#define HASH_BASE_SYMBOL 32

/* Доля допустимой заполненности таблицы */
#define MAX_LOAD_FACTOR 0.75

/* Во сколько раз увеличивать таблицу при росте */
#define EXTEND_FACTOR 2


/* Ячейка массива слов */
typedef struct {
	char word[MAX_WORD_LENGTH];
	unsigned int count;
} wcnt_t;

/* Хеш-таблица со словами */
typedef struct {
	/* Размер массива слов */
	unsigned int size;
	/* Сколько занято ячеек */
	unsigned int occupied;
	/* Максимально возможное количество занятых ячеек до расширения */
	unsigned int max_occupied;
	/* Массив со словами */
	wcnt_t* words;
} hashtable_t;


/* Инициализация таблицы */
hashtable_t* hashtable_new(unsigned int size);

/* Печатает массив слов */
void hashtable_print(hashtable_t* hashtable);

/* Добавляет слово в таблицу */
hashtable_t* hashtable_add(hashtable_t* hashtable, char* word);

/* Освобождает память */
void hashtable_destroy(hashtable_t* hashtable);

#endif
