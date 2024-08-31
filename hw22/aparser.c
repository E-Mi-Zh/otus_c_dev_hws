#define _POSIX_C_SOURCE 200112L
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

#define N_ARGS 3
#define STRLENMAX 10240

char log_dir[STRLENMAX];
int threads_num;
FILE* F;

/* Информация из строчки лога */
typedef struct log_string_info_t {
    size_t bytes;
    char url[STRLENMAX];
    char referer[STRLENMAX];
} log_string_info_t;

/* Статистика по URL для хранения в хеш-таблице */
typedef struct url_stats_t {
    size_t url_bytes_count;
    char url[STRLENMAX];
} url_stats_t;

/* Статистика по referer для хранения в хеш-таблице */
typedef struct referer_stats_t {
    size_t referer_count;
    char url[STRLENMAX];
} referer_stats_t;

/* Статистика по лог-файлу */
typedef struct logfile_stats_t {
    size_t total_bytes;
    GHashTable* url_stats;
    GHashTable* referer_stats;
} logfile_stats_t;

typedef struct thread_info_t {
    pthread_t pid;
    GSList* logfiles;
    logfile_stats_t* logfile_stats;
} thread_info_t;

void parse_args(int argc, char* argv[]);
void print_usage(void);
FILE* open_file(const char* filename);
void close_file(FILE* file);

/* Парсит строчку формата Apache combined log */
/* Сохраняет количество загруженных байт, URL и referer. */
int parse_string(char* input_string, log_string_info_t* result);

/* Возвращает список файлов в каталоге */
GSList* get_lognames(const char log_dir[]);

/* Тестовая строка для проверки */
/* 176.193.24.191 - - [19/Jul/2020:07:41:30 +0000] "GET / HTTP/2.0" 304 0 "https://baneks.site/" "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.138 Safari/537.36" */
char sample_str[] = "176.193.24.191 - - [19/Jul/2020:07:41:30 +0000] \"GET / HTTP/2.0\" 304 15716 \"https://baneks.site/\" \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.138 Safari/537.36";

/* Функция обработки (парсинга) файлов */
void* process_files_task(void* arg);

/* Парсит заданный файл, возвращает статистику по файлу */
void parse_file(const char* filename, logfile_stats_t* logfile_stats);

/* Функции добавления статистики по URL и Referer в хеш-таблицу */
GHashTable* add_url_stats(GHashTable* hashtable, url_stats_t url_stats);
GHashTable* add_referer_stats(GHashTable* hashtable, referer_stats_t referer_stats);

/* Печатает хеш таблицу */
void print_table(gpointer key, gpointer value, gpointer userdata);

/* Добавляют пару ключ-значение в хеш таблицу (используются для слияние таблиц)*/
void copy_url_table(gpointer key, gpointer value, gpointer userdata);
void copy_referer_table(gpointer key, gpointer value, gpointer userdata);

/* Возвращают массивы пар ключ-значение размера array_size, отсортированные по убыванию */
/* байт на URL и количества рефереров */
void get_top_urls(GHashTable* hashtable, unsigned int array_size, url_stats_t* top_urls);
void get_top_referers(GHashTable* hashtable, unsigned int array_size, referer_stats_t* top_referers);

int main(int argc, char* argv[])
{
    GSList* logfiles = NULL;
    GSList* l = NULL;
    guint listlen = 0;
    //int res = 0;
    thread_info_t* threads_info;
    //void* pres;
    logfile_stats_t all_stats;

    parse_args(argc, argv);
    threads_info = malloc(threads_num * sizeof(thread_info_t));

    /* Получаем список файлов в каталоге */
    logfiles = get_lognames(log_dir);
    listlen = g_slist_length(logfiles);
    printf("Number of logfiles = %d\n", listlen);

    // printf("log_dir=%s\n", log_dir);
    // printf("threads_num=%d\n", threads_num);

    /* Разбиваем список на подсписки по количеству потоков */
    int i = 0;
    l = logfiles;
    while (l != NULL) {
        /* printf("i=%d\ti mod nthr=%d\n", i, i % threads_num); */
        i++;
        threads_info[i % threads_num].logfiles = g_slist_prepend(threads_info[i % threads_num].logfiles, l->data);
        l = l->next;
    }

    /* В цикле создаём нужное количество потоков, передавая им */
    /* список файлов и структуру для хранения */
    for (i = 0; i < threads_num; i++) {
        threads_info[i].logfile_stats = malloc(sizeof(logfile_stats_t));
        pthread_create(&threads_info[i].pid, NULL, process_files_task, (void*) &threads_info[i]);
//        process_files_task((void*) &threads_info[i]);
    }
    for (i = 0; i < threads_num; i++) {
        pthread_join(threads_info[i].pid, NULL);
//        process_files_task((void*) &threads_info[i]);
    }

    /* По завершению работы потоков проходим в цикле по структурам с результатами */
    /* и сохраняем агрегированную статистику */
    all_stats.total_bytes = 0;
    all_stats.url_stats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    all_stats.referer_stats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    for (i = 0; i < threads_num; i++) {
        all_stats.total_bytes = all_stats.total_bytes + threads_info[i].logfile_stats->total_bytes;
        g_hash_table_foreach(threads_info[i].logfile_stats->url_stats, copy_url_table, all_stats.url_stats);
        g_hash_table_foreach(threads_info[i].logfile_stats->referer_stats, copy_referer_table, all_stats.referer_stats);
    }

    printf("\n");
    printf("Total downloaded bytes: %ld\n", all_stats.total_bytes);
    printf("\n");
    /* Ищем топ 10 */
    int topsize = 10;
    url_stats_t* top_urls;
    referer_stats_t* top_referers;

    top_urls = calloc(topsize, sizeof(url_stats_t));
    top_referers = calloc(topsize, sizeof(referer_stats_t));

    get_top_urls(all_stats.url_stats, topsize, top_urls);
    get_top_referers(all_stats.referer_stats, topsize, top_referers);

    printf("Top 10 \"heaviest\" URLs:\n");
    printf(" Bytes\t\t\t|\t\tURL\n");
    printf("-------------------------------------------------------------------\n");
    for (int i = 0; i < topsize; i++) {
        printf("%ld\t\t|", top_urls[i].url_bytes_count);
        printf(" %s\n", top_urls[i].url);
    }
    printf("\n");

    printf("Top 10 most frequent referers:\n");
    printf(" Count\t|\t\tReferer\n");
    printf("-------------------------------------------------------------------\n");
    for (int i = 0; i < topsize; i++) {
        printf("%ld\t|", top_referers[i].referer_count);
        printf(" %s\n", top_referers[i].url);
    }
    //print_task((void*) &all_stats);

    // g_hash_table_foreach(all_stats.url_stats, print_table, NULL);
    // g_hash_table_foreach(all_stats.referer_stats, print_table, NULL);

    g_free(top_urls);
    g_free(top_referers);

    g_hash_table_destroy(all_stats.url_stats);
    g_hash_table_destroy(all_stats.referer_stats);
    
    for (i = 0; i < threads_num; i++) {
        //printf("i=%d\n", i);
        g_slist_free(threads_info[i].logfiles);
        g_hash_table_destroy(threads_info[i].logfile_stats->url_stats);
        g_hash_table_destroy(threads_info[i].logfile_stats->referer_stats);
        g_free(threads_info[i].logfile_stats);
    }
    g_free(threads_info);
    g_slist_free_full(g_steal_pointer(&logfiles), g_free);

    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[]) {
	if (argc < N_ARGS) {
		fprintf(stderr, "Not all parameters are specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

    strncpy(log_dir, argv[1], STRLENMAX-1);
    log_dir[STRLENMAX] = '\0';
    sscanf(argv[2], "%d", &threads_num);
    if (threads_num < 1) {
        fprintf(stderr, "Incorrect threads number (should be greater or equal than 1): %d\n", threads_num);
        exit(EXIT_FAILURE);
    }
}

void print_usage(void) {
	printf("aparse - count stats for Apache combined logs.\n");
	printf("Usage: aparse log_directory num_threads\n");
	printf("Example: ./aparse logs 4\n");
}

FILE* open_file(const char* filename) {
    FILE* file;

	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error opening input file %s: %s! Skipping...\n", filename, strerror(errno));
		//exit(EXIT_FAILURE);
	}

    return file;
}

void close_file(FILE* file) {
	fclose(file);
}

// 176.193.24.191 - - [19/Jul/2020:07:41:30 +0000] "GET / HTTP/2.0" 304 0 "https://baneks.site/" "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.138 Safari/537.36"
/* upon parse failure will leave url and referer empty, bytes=0 */
/* Failed on such strings: */
/* 161.35.153.215 - - [19/Jul/2020:08:24:10 +0000] "\x03\x00\x00\x13\x0E\xE0\x00\x00\x00\x00\x00\x01\x00\x08\x00\x00\x00\x00\x00" 400 150 "-" "-" "-" */
    /* 27.11
5.124.74 - - [22/Jul/2020:11:18:07 +0000] "l\x00\x0B\x00\x00\x00\x00\x00\x00\x00\x00\x00" 400 150 "-" "-" "-" */
int parse_string(char* input_string, log_string_info_t* result)
{
    char* token = NULL;
    char* saveptr;
    int res = 0;

#ifdef DEBUG
    char inpstr[STRLENMAX];
    strncpy(inpstr, input_string, STRLENMAX-1);
    inpstr[STRLENMAX-1] = '\0';
#endif


    /* Обнуляем счётчик и строки на случай сбоя парсинга */
    result->bytes = 0;

    result->url[0] = '\0';
    result->referer[0] = '\0';

    /* Часть до первой кавычки */
    token = strtok_r(input_string, "\"", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }

    /* Тип запроса */
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }

    /* URL */
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    strncpy(result->url, token, STRLENMAX-1);
    result->url[STRLENMAX-1] = '\0';


    /* Остаток до второй кавычки */
    token = strtok_r(NULL, "\"", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    
    /* Статус код */
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
   
    /* Размер ответа в байтах */
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    res = sscanf(token, "%zu", &result->bytes);
    if (res !=1) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse number %s in string %s: %s\n", token, input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    
    /* Пропускаем до 3-й кавычки */
    token = strtok_r(NULL, "\"", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    
    /* Пропускаем до 4-й кавычки */
    token = strtok_r(NULL, "\"", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    
    /* После 5-й начнётся реферерер */
    token = strtok_r(NULL, "\"", &saveptr);
    if (token == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Source string: %s\n", inpstr);
        fprintf(stderr, "Failed to parse string %s: %s\n", input_string, strerror(errno));
#endif
        return EXIT_FAILURE;
    }
    strncpy(result->referer, token, STRLENMAX-1);
    result->referer[STRLENMAX-1] = '\0';
    
    return EXIT_SUCCESS;
}

GSList* get_lognames(const char log_dir[])
{
    GDir *dir;
    GError *error = NULL;
    const gchar *filename = NULL;
    GSList* filelist = NULL;
    gchar* relative_filename = NULL;

    dir = g_dir_open(log_dir, 0, &error);
    if (error != NULL) {
        fprintf(stderr, "Can't open directory %s: %s\n", log_dir, error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }
    if (dir != NULL) {
        while ((filename = g_dir_read_name(dir))) {
            relative_filename = g_build_filename(log_dir, filename, NULL);
            filelist = g_slist_prepend(filelist, (gpointer) relative_filename);
        }
        g_dir_close(dir);
    }

    return filelist;
}

void print_table(gpointer key, gpointer value, gpointer userdata)
{
    printf("-------------------\n");
    printf("%s\n", (char*) key);
    printf("%d\n", *((int*) value));
    (void) userdata;
}

void copy_url_table(gpointer key, gpointer value, gpointer userdata)
{
    GHashTable* hashtable = (GHashTable*) userdata;
    url_stats_t url_stats = {0};

    url_stats.url_bytes_count = *((int*) value);
    strncpy(url_stats.url, (char*) key, STRLENMAX-1);
    url_stats.url[STRLENMAX-1] = '\0';

    hashtable = add_url_stats(hashtable, url_stats);
}

void copy_referer_table(gpointer key, gpointer value, gpointer userdata)
{
    GHashTable* hashtable = userdata;
    referer_stats_t referer_stats = {0};

    referer_stats.referer_count = *((int*) value);
    strncpy(referer_stats.url, (char*) key, STRLENMAX-1);
    referer_stats.url[STRLENMAX-1] = '\0';

    hashtable = add_referer_stats(hashtable, referer_stats);
}


GHashTable* add_url_stats(GHashTable* hashtable, url_stats_t url_stats)
{
    gpointer value = NULL;
    size_t* url_bytes_count;
    size_t* new_value;

    if ((value = g_hash_table_lookup(hashtable, url_stats.url)) != NULL) {
        url_bytes_count = (size_t*) value;
        *url_bytes_count = *url_bytes_count + url_stats.url_bytes_count;
    } else {
        new_value = malloc(sizeof(size_t));
        *new_value = url_stats.url_bytes_count;
        g_hash_table_insert(hashtable, g_strdup(url_stats.url), (gpointer) new_value);
    }

    return hashtable;
}

GHashTable* add_referer_stats(GHashTable* hashtable, referer_stats_t referer_stats)
{
    gpointer value = NULL;
    size_t* referer_count;
    size_t* new_value;

    if ((value = g_hash_table_lookup(hashtable, referer_stats.url)) != NULL) {
        referer_count = (size_t*) value;
        *referer_count = *referer_count + referer_stats.referer_count;
    } else {
        new_value = malloc(sizeof(size_t));
        *new_value = referer_stats.referer_count;
        g_hash_table_insert(hashtable, g_strdup(referer_stats.url), (gpointer) new_value);
    }

    return hashtable;
}

void parse_file(const char* filename, logfile_stats_t* logfile_stats)
{
    FILE* file;
    char logstring[STRLENMAX];
    log_string_info_t result = {0};
    url_stats_t url_stats = {0};
    referer_stats_t referer_stats = {0};

    file = open_file(filename);
    if (file == NULL)
        return;
    printf("Processing file %s\n", filename);
    /* Читаем файл построчно */
    while(fgets(logstring, STRLENMAX, file)) {
        /* Для каждой строчки вызываем парсинг строки */
        parse_string(logstring, &result);
        // printf("bytes=%ld\t", result.bytes);
        // printf("url=%s\t", result.url);
        // printf("referer=%s\n", result.referer);

        /* Добавляем результаты парсинга в хеш таблицу */
        logfile_stats->total_bytes = logfile_stats->total_bytes + result.bytes;

        url_stats.url_bytes_count = result.bytes;
        strncpy(url_stats.url, result.url, STRLENMAX-1);
        url_stats.url[STRLENMAX-1] = '\0';

        referer_stats.referer_count = 1;
        strncpy(referer_stats.url, result.referer, STRLENMAX-1);
        referer_stats.url[STRLENMAX-1] = '\0';

        logfile_stats->url_stats = add_url_stats(logfile_stats->url_stats, url_stats);
        logfile_stats->referer_stats = add_referer_stats(logfile_stats->referer_stats, referer_stats);
    }
    
    close_file(file);
}

void* process_files_task(void* arg)
{
    thread_info_t* thread_info = (thread_info_t*) arg;
    /* printf("Thread id = %ld\n", pthread_self()); */

    thread_info->logfile_stats->total_bytes = 0;
    thread_info->logfile_stats->url_stats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    thread_info->logfile_stats->referer_stats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    /* Перебираем файлы из списка и вызываем функцию парсинга для каждого. */
    g_slist_foreach(thread_info->logfiles, (GFunc) parse_file, thread_info->logfile_stats);

    /* g_hash_table_foreach(thread_info->logfile_stats->url_stats, print_table, NULL);
    g_hash_table_foreach(thread_info->logfile_stats->referer_stats, print_table, NULL); */

    return ((void*)0);
}

void* print_task(void* arg)
{
    thread_info_t* thread_info = (thread_info_t*) arg;

    g_hash_table_foreach(thread_info->logfile_stats->url_stats, print_table, NULL);
    g_hash_table_foreach(thread_info->logfile_stats->referer_stats, print_table, NULL);

    return ((void*)0);
}

/* Got algo from here - https://stackoverflow.com/a/59392186 */
/* In order to find the top 10 items I advise you to create an array of 10 items to store the best items you get so far.
   
   The first step is to copy the first 10 elements into your array.

   The second step is sort your array of 10 items descendingly, so you will always use the last item for comparison.

   Now you can loop the big structure and on each step, compare the current item with the last one of the array of ten elements.
   If it's lower, then do nothing. If it's higher, then find the highest ranked item in your array of 10 items which is smaller
   than the item you intend to insert due to higher quality. When you find that item, loop from the end until this item until
   your array of ten elements and on each step override the current element with the current one. Finally override the now duplicate element.

   Example: Assuming that your 7th element has lower quality than the one you intend to insert, but the 6th has higher quality
   override 9th element with the 8th, then the 8th with the 7th and then the 7th with the item you just found. Remember that
   array indexes start from 0. */
void get_top_urls(GHashTable* hashtable, unsigned int array_size, url_stats_t* top_urls)
{
    GHashTableIter iter;
    gpointer key, value;
    size_t val;
    unsigned int i;

    g_hash_table_iter_init (&iter, hashtable);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        val = *((size_t*) value);
        if (val < top_urls[array_size-1].url_bytes_count) {
            continue;
        }
        i = array_size-1;
        while ((val > top_urls[i].url_bytes_count) && i > 0) {
            top_urls[i].url_bytes_count = top_urls[i-1].url_bytes_count;
            strncpy(top_urls[i].url, top_urls[i-1].url, STRLENMAX-1);
            top_urls[i].url[STRLENMAX-1] = '\0';
            i--;
        }
        if (val >= top_urls[i].url_bytes_count) {
            /* Replace first element */
            top_urls[i].url_bytes_count = val;
            strncpy(top_urls[i].url, key, STRLENMAX-1);
            top_urls[i].url[STRLENMAX-1] = '\0';
        } else {
            /* Replace second element */
            if ((i+1) == array_size) {
                printf("Q!\n");
            }
            top_urls[i+1].url_bytes_count = val;
            strncpy(top_urls[i+1].url, key, STRLENMAX-1);
            top_urls[i+1].url[STRLENMAX-1] = '\0';
        }
    }
}

void get_top_referers(GHashTable* hashtable, unsigned int array_size, referer_stats_t* top_referers)
{
    GHashTableIter iter;
    gpointer key, value;
    size_t val;
    unsigned int i;

    g_hash_table_iter_init (&iter, hashtable);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        val = *((size_t*) value);
        if (val < top_referers[array_size-1].referer_count) {
            continue;
        }
        i = array_size-1;
        while ((val > top_referers[i].referer_count) && i > 0) {
            top_referers[i].referer_count = top_referers[i-1].referer_count;
            strncpy(top_referers[i].url, top_referers[i-1].url, STRLENMAX-1);
            top_referers[i].url[STRLENMAX-1] = '\0';
            i--;
        }
        if (val >= top_referers[i].referer_count) {
            /* Replace first element */
            top_referers[i].referer_count = val;
            strncpy(top_referers[i].url, key, STRLENMAX-1);
            top_referers[i].url[STRLENMAX-1] = '\0';
        } else {
            /* Replace second element */
            top_referers[i+1].referer_count = val;
            strncpy(top_referers[i+1].url, key, STRLENMAX-1);
            top_referers[i+1].url[STRLENMAX-1] = '\0';
        }
    }
}
