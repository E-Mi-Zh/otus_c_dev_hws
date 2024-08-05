# gtk_tree

## Задание
Написать приложение с использованием GTK, отображающее файлы и директории в текущем каталоге.
Сторонние библиотеки (кроме GLib/GTK) не использовать.

## Задача:
Создать графическое приложение рекурсивно отображающее в GtkTreeView-виджете файлы и
директории, расположенные в директории, откуда оно было запущено.

## Требования к коду:
* сторонние библиотеки не использовать (разрешается стандартная библиотека C, GLib и GTK);
* успешная компиляция с флагами `-Wall -Wextra -Wpedantic -std=c11`;
* Предупреждения об устаревших API допускаются.

## Вспомогательные материалы
1. [Getting Started with GTK](https://docs.gtk.org/gtk4/getting_started.html)
2. [Gtk - 4.0: Tree and List Widget Overview](https://docs.gtk.org/gtk4/section-tree-widget.html)
3. [File Utilities: GLib Reference Manual](https://web.archive.org/web/20240105013344/https://developer-old.gnome.org/glib/stable/glib-File-Utilities.html)

# Реализация

Требования: компилятор Си11, стандартная библиотека (проверялось в Linux, GCC 13.2.0, libc 2.38), GTK4

Сборка: `make` или `gcc -o gtk_tree gtk_tree.c $(pkg-config --libs gtk4 --cflags gtk4) -Wall -Wextra -Wpedantic -std=c11`.

Использование: `./gtk_tree`.
