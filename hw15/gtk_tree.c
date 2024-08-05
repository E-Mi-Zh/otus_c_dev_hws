#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS enum {
	FILENAME_COLUMN,
	N_COLUMNS
};

/* Рекурсивная функция для добавления текущего файла в модель дерева */
gboolean populate_tree_model(GtkTreeStore *store, GFile *file)
{
	GError *error;
	/* Итераторы tree_store для уровня папки и файла. static для сохранения при рекурсии, */
	/* иначе пришлось бы передавать через параметры. */
	static GtkTreeIter iter1;
	static GtkTreeIter iter2;
	GFileType filetype;
	GFileEnumerator *enumerator = NULL;
	GFileInfo *fileinfo = NULL;
	const char *relative_path;

	filetype = g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL);
	if (filetype == G_FILE_TYPE_DIRECTORY) {
		/* Добавляем текущий каталог */
		gtk_tree_store_append(store, &iter1, NULL);
		gtk_tree_store_set(store, &iter1, FILENAME_COLUMN,
				   g_file_get_parse_name(file), -1);
		enumerator =
		    g_file_enumerate_children(file,
					      G_FILE_ATTRIBUTE_STANDARD_NAME,
					      G_FILE_QUERY_INFO_NONE, NULL,
					      &error);
		for (fileinfo =
		     g_file_enumerator_next_file(enumerator, NULL, &error);
		     fileinfo != NULL;
		     fileinfo =
		     g_file_enumerator_next_file(enumerator, NULL, &error)) {
			relative_path = g_file_info_get_name(fileinfo);
			/* для каждого файла в текущей папке рекурсивно вызываем сами себя */
			populate_tree_model(store,
					    g_file_resolve_relative_path(file,
									 relative_path));
		}
	} else if (filetype == G_FILE_TYPE_REGULAR) {
		/* обычный файл, добавляем в дерево, с учётом родительского итератора */
		gtk_tree_store_append(store, &iter2, &iter1);
		gtk_tree_store_set(store, &iter2, FILENAME_COLUMN,
				   g_file_get_basename(file), -1);
	}

	return TRUE;
}

static void activate(GtkApplication *app, gpointer user_data)
{
	GtkWidget *window;
	GtkTreeStore *store;
	GtkWidget *tree;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	(void)user_data;
	GFile *fl;

	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Window");
	gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

	/* Create a model.  We are using the store model for now, though we
	 * could use any other GtkTreeModel */
	store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING);

	/* custom function to fill the model with data */
	fl = g_file_new_for_path(".");
	populate_tree_model(store, fl);
	g_object_unref(fl);

	/* Create a view */
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	/* The view now holds a reference.  We can get rid of our own reference */
	g_object_unref(G_OBJECT(store));

	/* Create a cell render and arbitrarily make it blue for demonstration purposes */
	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "foreground", "blue", NULL);

	/* Create a column, associating the "text" attribute of the cell_renderer to the first column of the model */
	column = gtk_tree_view_column_new_with_attributes("Filename", renderer,
							  "text",
							  FILENAME_COLUMN,
							  NULL);

	/* Add the column to the view. */
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Now we can manipulate the view just like any other GTK widget */
	gtk_window_set_child(GTK_WINDOW(window), tree);
	/* expand all rows after the treeview widget has been realized */
	g_signal_connect(tree, "realize", G_CALLBACK(gtk_tree_view_expand_all),
			 NULL);
	gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
	GtkApplication *app;
	int status;

	app =
	    gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
