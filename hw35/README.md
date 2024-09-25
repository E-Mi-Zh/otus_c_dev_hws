# tetris

## Задание
Написать несложную игру, на выбор:
* тетрис
* сапёр
* 2048
* крестики-нолики
* четыре-в-ряд
* ping-pong
* space invaders
* pacman

## Задачи:
1. Создать игровое приложение с главным меню и основным gameplay loop.
2. Бонусные баллы за графическое оформление с использованием ассетов.
3. Бонусные баллы за звуковое оформление.
4. Бонусные баллы за таблицу лидеров.
5. Код компилируется без предупреждений с ключами компилятора -Wall -Wextra -Wpedantic -std=c11.


## Требования к коду:
* успешная компиляция с флагами `-Wall -Wextra -Wpedantic -std=c11`.


# Реализация

Требования: компилятор Си11, стандартная библиотека (проверялось в Linux, GCC 13.2.0, libc 2.38), наличие библиотеки Glut.

Сборка: `make` или 
```
gcc -o tetris tetris.c pkg-config allegro-5 allegro_font-5 allegro_primitives-5 allegro_audio-5 allegro_acodec-5 allegro_image-5 allegro_color-5 --libs --cflags` -lm  -Wall -Wextra -Wpedantic -std=c11
```
Использование: `./tetris`.

## Описание
В качестве примера был выбран Тетрис. Управление: стрелки ("вверх" - поворот блока), пробел, escape (выход).

## Благодарности
Andrew Deren - автор оригинального Тетриса на Аллегро3
SiegeLord - за Allegro5 Vivace tutorial
Black Squirrel - за рип спрайтов