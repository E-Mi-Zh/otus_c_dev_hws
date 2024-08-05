all: gtk_tree

gtk_tree: gtk_tree.c
	$(CC) -o gtk_tree gtk_tree.c `pkg-config --libs gtk4 --cflags gtk4` -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o gtk_tree

.PHONY: all clean
