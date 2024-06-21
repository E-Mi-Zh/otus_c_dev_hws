#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
//#include <ctype.h>
#include <string.h>
#include <curl/curl.h>
#include "parson.h"
#include <math.h>

#define MAX_URL_LENGTH 255

/* Разбор аргументов командной строки */
char* parse_args(int argc, char** argv);

/* Вывод сообщения с синтаксисом вызова и краткой информацией о программе */
void print_usage(void);

/* CURL content read callback */
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

/* Get weather data via curl */
ssize_t read_data(char* place);

typedef struct {
	int temp, wind_spd;
	const char* desc;
	const char* wind_dir;
} weather_t;

void parse_data(const char* json, weather_t* weather_report);

/* Выводит прогноз погоды */
void print_result(const weather_t weather_report, const char* place);

struct MemoryStruct {
	char *memory;
	size_t size;
};


char url[MAX_URL_LENGTH];
 
struct MemoryStruct chunk;

int main(int argc, char* argv[])
{
	char* place;
	weather_t weather_report = {0};

	/* Анализируем аргументы */
	place = parse_args(argc, argv);
	
	/* Запрашиваем данные с сервера */
	if (!read_data(place)) {
		fprintf(stderr, "Got empty response from server, exiting.\n");
		exit(EXIT_FAILURE);
	}

	/* Анализируем ответ (парсим JSON) */
	parse_data(chunk.memory, &weather_report);

	/* Выводим результат (прогноз погоды) */
	print_result(weather_report, place);
 
	free(chunk.memory);

	exit(EXIT_SUCCESS);
}

char* parse_args(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "Target place not specified!\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	return argv[1];
}

void print_usage(void)
{
	printf("weather - print weather forecast for given place (data from wttr.in).\n");
	printf("Usage: weather place|city\n");
}

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
	char *ptr = realloc(mem->memory, mem->size + realsize + 1);
	if(!ptr) {
		/* out of memory! */
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}
 
	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;
 
	return realsize;
}

ssize_t read_data(char* place)
{
	CURL *curl_handle;
	CURLcode res;
	
	char* url;
	char* url_start = "https://ru.wttr.in/";
	char* url_end = "?format=j1";

	url = calloc(1, strlen(url_start)+strlen(place)+strlen(url_end) + 1);
	if (!url) {
		fprintf(stderr, "Failed to allocate memory for url! %ld bytes requested.\n", strlen(url_start)+strlen(place)+strlen(url_end) + 1);
		exit(EXIT_FAILURE);
	}
	url = strcpy(url, url_start);
	url = strcat(url, place);
	url = strcat(url, url_end);
	chunk.memory = malloc(1);  /* grown as needed by the realloc above */
	chunk.size = 0;    /* no data at this point */
	
	curl_global_init(CURL_GLOBAL_ALL);
 
	/* init the curl session */
	curl_handle = curl_easy_init();
 
	/* specify URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
 
	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
 
	/* some servers do not like requests that are made without a user-agent
	   field, so we provide one */
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
 
	/* get it! */
	res = curl_easy_perform(curl_handle);
 
	/* check for errors */
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
	} else {
		/*
		 * Now, our chunk.memory points to a memory block that is chunk.size
		 * bytes big and contains the remote file.
		 *
		 * Do something nice with it!
		 */
 		printf("Получено %lu байт.\n", (unsigned long)chunk.size);
	}

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);
	
	/* we are done with libcurl, so clean it up */
	curl_global_cleanup();

	return chunk.size;
}

void print_result(const weather_t weather_report, const char* place) {
	printf("Прогноз погоды в %s:\n", place);
	printf("%s\n", weather_report.desc);
	printf("Направление ветра %s\n", weather_report.wind_dir);
	printf("Скорость ветра %d м/с\n", weather_report.wind_spd);
	printf("Температура %d °C\n", weather_report.temp);
}

char* get_js_type_str(JSON_Value_Type typeval) {
	switch (typeval) {
		case JSONError:				/* -1 */
			return "JSONError";
		case JSONNull:				/* 1 */
			return "JSONNull";		
		case JSONString:			/* 2 */
			return "JSONString";
		case JSONNumber:			/* 3 */
			return "JSONNumber";
		case JSONObject:			/* 4 */
			return "JSONObject";
		case JSONArray:				/* 5 */
			return "JSONArray";
		case JSONBoolean:			/* 6 */
			return "JSONBoolean";
		default:
			return "JSONError";
	}
}
	
void parse_data(const char* json, weather_t* weather_report)
{
	JSON_Value *root_value;			/* { ... } */
    JSON_Object *root_object;		/* "current_condition": [], "nearest_area": [], "request": [], "weather": [] */
	JSON_Value *curr_cond_value;	/* [ { ... } ] - array of objects (value of parent current_condition element) */
	JSON_Array* curr_cond_array;	/* { ... } - actual array (currently with one member) */
    JSON_Value *value;				/* { ... } - object with current weather "key":"value" pairs */
    JSON_Object *values;			/* "k":"v", "k":"v", - key-val pairs object */
	JSON_Value *desc_array_value;	/* "weatherDesc": [ { ... } ] - weather description compose element */
	JSON_Array* desc_array;			/* [ ... ] - actual array from key "weatherDesc" */
    JSON_Object *desc_object;

	JSON_Value_Type typeval;

	size_t cnt;
	const char* str;
	char* str2;

	/* Parse JSON string and getting root value and object */
	root_value = json_parse_string(json);
	if (root_value == NULL) {
		fprintf(stderr, "Parse error: got empty root_value.\n");
		exit(EXIT_FAILURE);
	}
	if ((typeval = json_value_get_type(root_value)) != JSONObject) {
		fprintf(stderr, "Parse error: incorrect root_value. Expected JSONObject, got %s\n", get_js_type_str(typeval));
        exit(EXIT_FAILURE);
    }

	root_object = json_value_get_object(root_value);
	if ((cnt = json_object_get_count(root_object)) == 0) {
		fprintf(stderr, "Parse error: root_object has no values\n");
		exit(EXIT_FAILURE);
	}

	/* Getting current weather condition object */
	curr_cond_value = json_object_get_value(root_object, "current_condition");
	if (curr_cond_value == NULL) {
		fprintf(stderr, "Parse error: root_object has no current weather condition value.\n");
		exit(EXIT_FAILURE);
	}
	if ((typeval = json_value_get_type(curr_cond_value)) != JSONArray) {
		fprintf(stderr, "Parse error: incorrect current condition value. Expected JSONArray, got %s\n", get_js_type_str(typeval));
        exit(EXIT_FAILURE);
    }

	/* Getting array of objects (actually only one element) with current weather conditions */	
	curr_cond_array = json_value_get_array(curr_cond_value);
	if (curr_cond_array == NULL) {
		fprintf(stderr, "Parse error: current condition value doesn't contain array with weather values.\n");
		exit(EXIT_FAILURE);
	}
	if ((cnt = json_object_get_count(root_object)) == 0) {
		fprintf(stderr, "Parse error: root_object has no values\n");
		exit(EXIT_FAILURE);
	}

	/* First element of current condition array */
	value = json_array_get_value(curr_cond_array, 0);
	if (value == NULL) {
		fprintf(stderr, "Parse error: current weather condition array has no values.!\n");
		exit(EXIT_FAILURE);
	}
	if ((typeval = json_value_get_type(value)) != JSONObject) {
		fprintf(stderr, "Parse error: incorrect value of current condition array object. Expected JSONObject, got %s\n", get_js_type_str(typeval));
        exit(EXIT_FAILURE);
    }

	/* Actual array values (key-val pairs). */
	values = json_value_get_object(value);
	if (curr_cond_array == NULL) {
		fprintf(stderr, "Parse error: current condition array doesn't contain weather key-value pairs.\n");
		exit(EXIT_FAILURE);
	}
	if ((cnt = json_object_get_count(values)) == 0) {
		fprintf(stderr, "Parse error: current condition array doesn't contain any key-val pairs.\n");
		exit(EXIT_FAILURE);
	}

	str = json_object_get_string(values, "temp_C");
	if (str == NULL) {
		fprintf(stderr, "Parse error: can't get temperature value!\n");
		exit(EXIT_FAILURE);
	}
	weather_report->temp = atoi(str);

	str = json_object_get_string(values, "windspeedKmph");
	if (str == NULL) {
		fprintf(stderr, "Parse error: can't get wind speed value!\n");
		exit(EXIT_FAILURE);
	}
	weather_report->wind_spd = lround(atoi(str) * 1000.0 / 3600.0);

	str = json_object_get_string(values, "winddir16Point");
	if (str == NULL) {
		fprintf(stderr, "Parse error: can't get wind direction value!\n");
		exit(EXIT_FAILURE);
	}
	str2 = malloc((strlen(str) * sizeof("Ф")) + 1);
	str2[0] = '\0';
	for (size_t i = 0; i < strlen(str); i++) {
		switch (str[i]) {
			case 'N':
				str2 = strcat(str2, "С");
				break;
			case 'W':
				str2 = strcat(str2, "З");
				break;
			case 'E':
				str2 = strcat(str2, "В");
				break;
			case 'S':
				str2 = strcat(str2, "Ю");
				break;
			default:
				break;
		}
	}
	weather_report->wind_dir = str2;

	/* Getting array value with weather description */
	desc_array = json_object_get_array(values, "lang_ru");
	if (desc_array == NULL) {
		fprintf(stderr, "Parse error: weather description array doesn't contain value.\n");
		exit(EXIT_FAILURE);
	}
	if ((cnt = json_array_get_count(desc_array)) == 0) {
		fprintf(stderr, "Parse error: weather description array doesn't contain any objects.\n");
		exit(EXIT_FAILURE);
	}

	desc_array_value = json_array_get_value(desc_array, 0);
	if (desc_array_value == NULL) {
		fprintf(stderr, "Parse error: weather description array[0] doesn't contain values.\n");
		exit(EXIT_FAILURE);
	}
	if ((typeval = json_value_get_type(desc_array_value)) != JSONObject) {
		fprintf(stderr, "Parse error: incorrect desc_array_value. Expected JSONObject, got %s\n", get_js_type_str(typeval));
        exit(EXIT_FAILURE);
    }

	desc_object = json_value_get_object(desc_array_value);
	if (desc_object == NULL) {
		fprintf(stderr, "Got null!\n");
		exit(EXIT_FAILURE);
	}

	weather_report->desc = json_object_get_string(desc_object, "value");
	if (weather_report->desc == NULL) {
		fprintf(stderr, "Got null!\n");
		exit(EXIT_FAILURE);
	}
}
