#include <stdlib.h>
#include <stdio.h>

long data[] = {4, 8, 15, 16, 23, 42};
size_t data_length = sizeof(data) / sizeof(data[0]);
char* int_format = "%ld ";
char* empty_str = "";

typedef struct node {
    long val;
    struct node* next;
} node_t;

void print_int(long number)
{
	printf(int_format, number);
	fflush(NULL);
}

long p(long number)
{
	return(number & 1);
}

node_t* add_element(long val, node_t* head)
{
	node_t* new_node;

	new_node = (node_t *) malloc(sizeof(node_t));
	if (new_node == NULL) {
        abort();
    }

    new_node->val = val;
    new_node->next = head;

    return new_node;
}

/* map - функция высшего порядка */
/* принимает аргументом список и функцию, которую нужно применить */
/* к каждому элементу списка */
void m(node_t* list, void (*function)(long))
{
	if (list == NULL) return;
	function(list->val);
	m(list->next, function);
}

/* Пытался сделать вариант с возвратом через результат функции */
/* Но в этом случае итоговый список оказывался с инверсным порядком */
/* Что не совпадало с выводом программы на ассемблере */
/*node_t* f(node_t* list, long (*function)(long))
{
    node_t* l;
	if (list == NULL) return NULL;

	if (function(list->val)) {
		return add_element(list->val, f(list->next, function));
	} else {
        return f(list->next, function);
    }
}*/

/* filter - функция высшего порядка */
/* принимает аргументом список и функцию, которую нужно применить */
/* к каждому элементу списка */
/* в качестве результата возвращает новый список, состоящий из тех членов исходного, */
/* для которых функция-аргумент дала true */
void f(node_t* list, node_t** result, long (*function)(long))
{
	if (list == NULL) return;

	if (function(list->val)) {
		*result = add_element(list->val, *result);
	}
	f(list->next, result, function);
}

void free_list(node_t* list)
{
	node_t* tmp;

	while (list->next != NULL) {
		tmp = list;
		list = list->next;
		free(tmp);
    }
}

int main(void)
{
	node_t* numbers_list = NULL;
	node_t* list2 = NULL;

	for (size_t i = data_length; i != 0; --i) {
	        numbers_list = add_element(data[i-1], numbers_list);
	}

	m(numbers_list, &print_int);
	puts(empty_str);

	f(numbers_list, &list2, &p);

	m(list2, &print_int);
	puts(empty_str);

	/* здесь была утечка, т.к. в исходном листинге списки не освобождались */
	free_list(numbers_list);
	free_list(list2);

	return 0;
}
