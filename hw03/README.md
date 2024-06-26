# Zipjpeg

## Задание
Написать программу, определяющую, является ли входной файл файлом **zipjpeg** (т.е. изображением,
в конец которого дописан архив) и выводящую список файлов из архива.

## Задачи:
1. Создать консольное приложение, принимающее аргументом командной строки имя входного файла.
2. Определить, есть ли после изображения архив или нет.
3. Вывести список файлов из архива.

## Требования к коду:
* сторонние библиотеки не использовать (разрешается стандартная библиотека C);
* успешная компиляция с флагами `-Wall -Wextra -Wpedantic -std=c11`.

## Вспомогательные материалы
1. [The structure of a PKZip file](https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip.html)
2. [Описание формата ZIP файла](https://blog2k.ru/archives/3391)
3. [Zip-файлы: история, объяснение и реализация](https://habr.com/ru/company/mailru/blog/490790/#17)

# Реализация

Требования: компилятор Си11, стандартная библиотека (проверялось в Linux, GCC 13.2.0, libc 2.38).

Сборка: `make` или `gcc -o zipjpeg_test -Wall -Wextra -Wpedantic -std=c11 zipjpeg_test.c`.

Использование: `zipjpeg_test file`.

## Описание работы
Программа сканирует входной файл на наличие сигнатуры заголовка JPEG файла, затем пропускает содержимое до конца JPEG
части. После начинает искать секции ZIP с именем файла, либо завершающий архив центральный каталог. Каждое найденное имя файла
в архиве выводится на печать.

Также программа распознаёт (и выводит имена файлов) обычные ZIP-архивы без предшествующего JPEG.

## Что можно улучшить
Сейчас чтение происходит посимвольно, что, вероятно, для больших файлов (либо их количества) не оптимально.
Можно было бы переписать с использованием буферизации (например по размеру сектора и поиском подстрок в буфере),
либо же отображать файл в память и искать в нём целиком. Также в этих случаях можно попробовать применить не
посимвольное сравнение, а какой-либо из алгоритмов поиска строк.
