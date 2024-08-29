#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#define N_ARGS 4
/* argv[1] - db */
/* argv[2] - table */
/* argv[3] - column */

void print_usage(void) {
	printf("dbstat - print SQLite3 database statistics.\n");
	printf("Usage: dbstat database_name table_name column_name\n");
	printf("Example: ./dbstat db.sqlite oscar id\n");
}

void parse_args(int argc) {
	if (argc < N_ARGS) {
		fprintf(stderr, "Not all parameters are specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}
}

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

int get_double(void* data, int argc, char** argv, char** col_name)
{
    (void) argc;
    (void) col_name;
    errno = 0;
    *((double*) data) = strtod(argv[0], NULL);
    if (errno !=0) {
        fprintf(stderr, "Error converting %s to double: %s", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int get_long(void* data, int argc, char** argv, char** col_name)
{
    (void) argc;
    (void) col_name;
    errno = 0;
    *((long*) data) = strtol(argv[0], NULL, 10);
    if (errno !=0) {
        fprintf(stderr, "Error converting %s to long: %s", argv[0], strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#define GET_STAT(x) do { \
    sqlsz = snprintf(NULL, 0, "SELECT %s(%s) FROM %s", STRINGIFY(x), argv[3], argv[2]); \
    sql = malloc(sqlsz + 1); \
    snprintf(sql, sqlsz + 1, "SELECT %s(%s) FROM %s", STRINGIFY(x), argv[3], argv[2]); \
    res = sqlite3_exec(db, sql, get_double, &x, &err_msg); \
    if (res != SQLITE_OK ) { \
        fprintf(stderr, "Failed to select data\n"); \
        fprintf(stderr, "SQL error: %s\n", err_msg); \
        sqlite3_free(err_msg); \
        sqlite3_close(db); \
        exit(EXIT_FAILURE); \
    } \
    free(sql); \
} while (0)

typedef struct array_t {
    unsigned int n;
    double* values;
} array_t;

int get_disp(void *data, int argc, char **argv, char **colname)
{
    double val;
    array_t* array = (array_t*) data;

    (void) colname;
    for (int i = 0; i < argc; i++) {
        errno = 0;
        val = strtod(argv[0], NULL);
        if (errno !=0) {
            fprintf(stderr, "Error converting %s to double: %s", argv[0], strerror(errno));
            return EXIT_FAILURE;
        }
    }
    array->values[array->n] = val;
    array->n++;

    return EXIT_SUCCESS;
}

// static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
//    (void) NotUsed;
//    for(int i = 0; i<argc; i++) {
//       printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
//    }
//    printf("\n");
//    return 0;
// }



int main(int argc, char* argv[])
{
    sqlite3 *db;
    char *err_msg;
    int res;
    char *sql;
    int sqlsz;
    double avg;
    double max;
    double min;
    double sum;
    double disp;
    long colsize;
    array_t array;

    parse_args(argc);
    res = sqlite3_open(argv[1], &db);
    if (res != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    
    GET_STAT(avg);
    GET_STAT(max);
    GET_STAT(min);
    GET_STAT(sum);

    sqlsz = snprintf(NULL, 0, "SELECT count(%s) FROM %s", argv[3], argv[2]);
    sql = malloc(sqlsz + 1);
    snprintf(sql, sqlsz + 1, "SELECT count(%s) FROM %s", argv[3], argv[2]);
    res = sqlite3_exec(db, sql, get_long, &colsize, &err_msg);
    if (res != SQLITE_OK ) {
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    free(sql);

    array.n = 0;
    array.values = malloc(colsize * sizeof(double));

    sqlsz = snprintf(NULL, 0, "SELECT %s FROM %s", argv[3], argv[2]);
    sql = malloc(sqlsz + 1);
    snprintf(sql, sqlsz + 1, "SELECT %s FROM %s", argv[3], argv[2]);
    res = sqlite3_exec(db, sql, get_disp, &array, &err_msg);
    if (res != SQLITE_OK ) {
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    free(sql);

    disp = 0;
    for (long i = 0; i < colsize; i++) {
        //printf("array[%ld]=%f\n", i, array.values[i]);
        disp = disp + ((array.values[i] - avg) * (array.values[i] - avg));
    }
    //printf("colsize = %ld\t n = %d\n", colsize, array.n);
    disp = disp / colsize;

    printf("Avg=%f\tMin=%f\tMax=%f\tSum=%f\tDisp=%f\n", avg, min, max, sum, disp);

    sqlite3_close(db);
    exit(EXIT_SUCCESS);
}