#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>


/* Разбор аргументов командной строки */
void parse_args(int argc);

/* Вывод сообщения с синтаксисом вызова и краткой информацией о программе */
void print_usage(void);

/* Открывает файл на чтение*/
int open_file(char* fname);

off_t get_file_size(const char* filename);

extern uint32_t calculate_crc32c(uint32_t crc32c, const unsigned char *buffer, unsigned int length);

int main(int argc, char* argv[]) {
    int input_file;
    off_t filesize;
    uint32_t crc32;
    unsigned char* region;

    /* Анализируем аргументы */
    parse_args(argc);
    
    /* Получаем размер файла */
    filesize = get_file_size(argv[1]);

    /* Открываем файл */
    input_file = open_file(argv[1]);
    
    region = mmap(NULL, filesize, PROT_READ, MAP_FILE|MAP_PRIVATE, input_file, 0);
    if (region != MAP_FAILED) {
        crc32 = calculate_crc32c(0, region, filesize);
        printf("crc32 = %#x\n", crc32);
    } else {
        fprintf(stderr, "Error mapping file %s (size %ld) to memory: %s! Exiting...\n", argv[1], filesize, strerror(errno));
    }

    munmap(region, filesize);
    close(input_file);

    exit(EXIT_SUCCESS);
}

void print_usage(void)
{   
    printf("crc_mmap - count CRC32 checksum of input file using mmap.\n");
    printf("Usage: crc_mmap input_file\n");
}

void parse_args(int argc)
{   
    if (argc < 2) {
        fprintf(stderr, "Input file not specified!\n");
        print_usage();
        exit(EXIT_FAILURE);
    }
}

int open_file(char* fname)
{
    int input_file;
    input_file = open(fname, O_RDONLY);
    if (input_file == -1) {
        fprintf(stderr, "Error opening input file %s: %s! Exiting...\n", fname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return input_file;
}

off_t get_file_size(const char* filename)
{
    struct stat file_status;
    if (stat(filename, &file_status) < 0) {
        fprintf(stderr, "Can't get size of file %s: %s, exiting...", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return file_status.st_size;
}
