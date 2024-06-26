# wc

## Задание
Написать реализацию хэш-таблицы с открытой адресацией со строками в качестве ключей и целыми
числами в качестве значений. На основе полученной реализации написать программу, подсчитывающую
частоту слов в заданном файле. Сторонние библиотеки не использовать.

## Задачи:
1. Создать консольное приложение принимающее, аргументом командной строки входной файл.
2. Реализовать хеш-функцию и хеш-таблицу с открытой адресацией.
3. Используя хеш-таблицу подсчитать частоту встречаемости каждого слова во входном файле.

## Требования к коду:
* сторонние библиотеки не использовать (разрешается стандартная библиотека C);
* успешная компиляция с флагами `-Wall -Wextra -Wpedantic -std=c11`.

## Вспомогательные материалы
1. [Хеш-таблица — Википедия](https://ru.wikipedia.org/wiki/Хеш-таблица#Открытая_адресация)
2. [Алгоритмы хэширования в задачах на строки](http://e-maxx.ru/algo/string_hashes)

# Реализация

Требования: компилятор Си11, стандартная библиотека (проверялось в Linux, GCC 13.2.0, libc 2.38).

Сборка: `make` или `gcc -o wc -Wall -Wextra -Wpedantic -std=c11 wc.c hashtable.c`.

Использование: `wc input_file`.

## Описание работы
Программа проверяет наличие и корректность входных параметров. Побайтово считывает входной файл,
формирует слова (всё, что между пробельными символами) и добавляет их в хеш-таблицу.

Хеш-таблица представляет собой динамический массив из слов и количества их повторений. Индекс в массиве
задаётся хеш-функцией от вставляемого слова. В случае возникновения коллизии новая ячейка определяется
линейным зондированием. При заполнении трёх четвертей таблицы происходит её увеличение в два раза.

## Что можно улучшить
1. Буферизовать чтение файла.
2. При разбивке на слова учитывать знаки препинания.
3. Хеш-таблицу сделать универсальной, вынеся во внешние функции всё, что связано с самими данными
(сравнение строк, структура их хранения (хранить просто указатель)).
