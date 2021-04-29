#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "inc/btree.h"
#include "jrb.h"
#include <gdk/gdkkeysyms.h>

const gchar *a, *b;

GtkWidget *textView, *view1, *view2, *about_dialog, *entry_search;
GtkListStore *list;
BTA *book = NULL;

void TxtView(char *text) // set text in textview buffer
{
	GtkTextBuffer *buffer;
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView));
	if (buffer == NULL)
	{
		buffer = gtk_text_buffer_new(NULL); //creat new text buffer
	}
	gtk_text_buffer_set_text(buffer, text, -1);				   // clear current buffer, inserts text instead
	// If len is -1, text must be nul-terminated
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(textView), buffer); //sets buffer as the buffer being displayed
}
void nWT_to_list(JRB nextWordArray) //
{
	GtkTreeIter Iter;
	JRB tmp;
	int max = 8;
	jrb_traverse(tmp, nextWordArray)
	{
		gtk_list_store_append(GTK_LIST_STORE(list), &Iter);
		gtk_list_store_set(GTK_LIST_STORE(list), &Iter, 0, jval_s(tmp->key), -1);
		//set value row 0, terminated by -1
		if (max-- < 1)
			return;
	}
}
static char code[128] = {0};
void add_code(const char *s, int c)
{
	while (*s)
	{
		code[(int)*s] = code[0x20 ^ (int)*s] = c;
		s++;
	}
}

void init()
{
	static const char *cls[] =
		{"AEIOU", "", "BFPV", "CGJKQSXZ", "DT", "L", "MN", "R", 0};
	int i;
	for (i = 0; cls[i]; i++)
		add_code(cls[i], i - 1);
}

// returns a static buffer; user must copy if want to save
//   result across calls
const char *soundex(const char *s)
{
	static char out[5];
	int c, prev, i;

	out[0] = out[4] = 0;
	if (!s || !*s)
		return out;

	out[0] = *s++;

	// first letter, though not coded, can still affect next letter: Pfister
	prev = code[(int)out[0]];
	for (i = 1; *s && i < 4; s++)
	{
		if ((c = code[(int)*s]) == prev)
			continue;

		if (c == -1)
			prev = 0; // vowel as separator
		else if (c > 0)
		{
			out[i++] = c + '0';
			prev = c;
		}
	}
	while (i < 4)
		out[i++] = '0';
	return out;
}

int insert_suggest_soundex(char *soundexlist, char *newword, char *word, char *soundexWord)
{//did you mean . prev. next. sdword
	init();
	if ((strcmp(soundexWord, soundex(newword)) == 0) && (strcmp(newword, word) != 0))
	{
		strcat(soundexlist, newword);
		strcat(soundexlist, "\n\t");
		return 1;
	}
	else
	{
		return 0;
	}
}

int prefix(const char *big, const char *small) // tra lai 1 neu giong prefix, 0 neu nguoc lai
{
	int small_len = strlen(small);
	int big_len = strlen(big);
	int i;
	if (big_len < small_len)
	{
		return 0;
	}
	for (i = 0; i < small_len; i++)
	{
		if (big[i] != small[i])
		{
			return 0;
		}
	}
	return 1;
}

void warning(GtkWidget *parent, GtkMessageType type, char *mms, char *content) // dua ra thong bao
{
	GtkWidget *mdialog;
	mdialog = gtk_message_dialog_new(GTK_WINDOW(parent),
									 GTK_DIALOG_DESTROY_WITH_PARENT,
									 type,
									 GTK_BUTTONS_CLOSE, // set of button to use
									 "%s", mms);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(mdialog), "%s", content);
	gtk_dialog_run(GTK_DIALOG(mdialog));
	gtk_widget_destroy(mdialog);
}

int find_in_tree(char *word) // used in funtion search
{
	char mean[100000];
	int size;

	if (btsel(book, word, mean, 100000, &size) == 0)
	{	//msize. rsize
		TxtView(mean);
		return 1;
	}
	else
	{
		return 0;
	}
}

void suggest(char *word, gboolean Tab_pressed) // suggest as prefix
{
	init();
	char nextword[100], prevword[100];
	int i;
	int max;
	GtkTreeIter Iter; //structure for accessing a GtkTreeModel
	JRB nextWordTree = make_jrb();
	BTint value, existed = 0;
	strcpy(nextword, word);

	gtk_list_store_clear(GTK_LIST_STORE(list)); // remove all rows from list store
	if (bfndky(book, word, &value) == 0)
	{ // tim word trong book, value la gia tri cua 'word' tim duoc
		existed = 1;
		gtk_list_store_append(GTK_LIST_STORE(list), &Iter);				  //append a new rows to list_store
		gtk_list_store_set(GTK_LIST_STORE(list), &Iter, 0, nextword, -1); // set value in the row referenced by iter
																		  // 0 is column ,the list is terminated by -1
	}
	if (!existed)
		btins(book, nextword, "", 1);

	for (i = 0; i < 2000; i++)
	{
		bnxtky(book, nextword, &value);
		if (prefix(nextword, word))
		{															// tim nhung tu co prefix giong
			jrb_insert_str(nextWordTree, strdup(nextword), JNULL); // va chen vao tree nextword (de show ra list)
		}
		else
			break;
	}

	if (!existed && Tab_pressed)
	{
		if (jrb_empty(nextWordTree))
		{
			char suggest_soundex[1000] = "Did you mean:\n\t";
			char soundexWord[50];
			strcpy(soundexWord, soundex(word));
			bfndky(book, word, &value);
			max = 5;
			for (i = 0; (i < 10000) && max; i++)
			{
				if (bprvky(book, prevword, &value) == 0)
					if (insert_suggest_soundex(suggest_soundex, prevword, word, soundexWord))
						max--;
			}
			bfndky(book, word, &value);	
			max = 5;
			for (i = 0; (i < 10000) && max; i++)
			{
				if (bnxtky(book, nextword, &value) == 0)
					if (insert_suggest_soundex(suggest_soundex, nextword, word, soundexWord))
						max--;
			}
			TxtView(suggest_soundex);
		}
		else
		{
			strcpy(nextword, jval_s(jrb_first(nextWordTree)->key));
			int wordlen = strlen(nextword);
			gtk_entry_set_text(GTK_ENTRY(entry_search), nextword);//Sets the text in the widget to the given value, replacing the current contents.
			gtk_editable_set_position(GTK_EDITABLE(entry_search), wordlen);
		}
	}
	else
		nWT_to_list(nextWordTree);
	if (!existed)
		btdel(book, word);
	jrb_free_tree(nextWordTree);
}

static void search(GtkWidget *w, gpointer data)
{
	GtkEntry *entry1 = ((GtkEntry **)data)[0];
	GtkWidget *window1 = ((GtkWidget **)data)[1];

	a = gtk_entry_get_text(GTK_ENTRY(entry1));
	char word[50];

	strcpy(word, a);
	if (word[0] == '\0')
		warning(window1, GTK_MESSAGE_WARNING, "Cảnh báo!", "Cần nhập từ để tra.");
	else
	{
		int l = find_in_tree(word);
		if (l == 0)
			warning(window1, GTK_MESSAGE_ERROR, "Xảy ra lỗi!", "Không tìm thấy từ này trong từ điển.");
	}
	return;
}

gboolean search_suggest(GtkWidget *entry, GdkEvent *event, gpointer No)
{
	GdkEventKey *keyEvent = (GdkEventKey *)event;
	char word[50];
	int len;
	strcpy(word, gtk_entry_get_text(GTK_ENTRY(entry_search)));
	if (keyEvent->keyval == GDK_KEY_Tab)
	{
		suggest(word, TRUE);
	}
	else
	{
		if (keyEvent->keyval != GDK_KEY_BackSpace)
		{
			len = strlen(word);
			word[len] = keyEvent->keyval;
			word[len + 1] = '\0';
		}
		else
		{
			len = strlen(word);
			word[len - 1] = '\0';
		}
		suggest(word, FALSE);
	}
	return FALSE;
}

void close_window(GtkWidget *widget, gpointer window)
{
	gtk_widget_destroy(GTK_WIDGET(window));
}

void about(GtkWidget widget, gpointer window)
{
	about_dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_dialog), "Dictionary");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_dialog), "Nguyen Viet Hoang 20184110\n Nguyen Minh Hieu 20184100\n Ta Dang Huy 20184121");
	gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_dialog), NULL);
	gtk_dialog_run(GTK_DIALOG(about_dialog));
	gtk_widget_destroy(about_dialog);
}

static void add(GtkWidget *w, gpointer data)
{

	GtkTextBuffer *buffer1, *buffer2;
	GtkTextIter start, end, iter;
	char mean[10000], word[50];
	buffer1 = gtk_text_view_get_buffer(GTK_TEXT_VIEW(GTK_TEXT_VIEW(view1)));
	gtk_text_buffer_get_iter_at_offset(buffer1, &iter, 0);
	//Initializes iter to a position char_offset chars from the start of the entire buffer
	gtk_text_buffer_insert(buffer1, &iter, "", -1); //insert len byte of text at position iter . 
	gtk_text_buffer_get_bounds(buffer1, &start, &end);  //If len is -1, text must be nul-terminated and will be inserted in its entirety.
	//Retrieves the first and last iterators in the buffer, i.e. the entire buffer lies within the range [start ,end ).
	b = gtk_text_buffer_get_text(buffer1, &start, &end, FALSE);//Returns the text in the range [start ,end )

	strcpy(word, b);

	buffer2 = gtk_text_view_get_buffer(GTK_TEXT_VIEW(GTK_TEXT_VIEW(view2)));
	gtk_text_buffer_get_iter_at_offset(buffer2, &iter, 0);

	gtk_text_buffer_insert(buffer2, &iter, "", -1);
	gtk_text_buffer_get_bounds(buffer2, &start, &end);
	b = gtk_text_buffer_get_text(buffer2, &start, &end, FALSE);

	strcpy(mean, b);

	BTint x;

	if (word[0] == '\0' || mean[0] == '\0')
		warning(data, GTK_MESSAGE_WARNING, "Cảnh báo!", "Không được bỏ trống phần nào.");
	else if (bfndky(book, word, &x) == 0)
		warning(data, GTK_MESSAGE_ERROR, "Xảy ra lỗi!", "Từ vừa nhập đã có trong từ điển.");
	else
	{
		if (btins(book, word, mean, 10000))
			warning(data, GTK_MESSAGE_ERROR, "Xảy ra lỗi!", "Không thể thêm vào từ điển.");
		else
			warning(data, GTK_MESSAGE_INFO, "Thành công!", "Đã thêm vào từ điển.");
	}
}

void addwords(GtkWidget widget, gpointer window)
{
	//khai bao widget
	GtkWidget *fixed, *btadd;
	GtkWidget *btback, *window2, *label1, *label2;
	//tao cua so moi
	window2 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window2), "Add new words");
	gtk_window_set_default_size(GTK_WINDOW(window2), 600, 400);
	gtk_window_set_position(GTK_WINDOW(window2), GTK_WIN_POS_CENTER);
	//tao mau chua vi tri
	fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(window2), fixed);
	//tao label nhap tu
	label1 = gtk_label_new("Word: ");
	gtk_fixed_put(GTK_FIXED(fixed), label1, 150, 65);
	gtk_widget_set_size_request(label1, 40, 20);
	//tao viewtext cho tu
	view1 = gtk_text_view_new();
	//gtk_widget_set_size_request(view1, 300, 20);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view1), GTK_WRAP_WORD);

	gtk_fixed_put(GTK_FIXED(fixed), view1, 200, 65);
	gtk_widget_set_size_request(view1, 250, 30);
	//tao label nhap nghia
	label2 = gtk_label_new("Mean: ");
	gtk_fixed_put(GTK_FIXED(fixed), label2, 150, 150);
	gtk_widget_set_size_request(label2, 40, 20);
	//tao cua so cuon cho textview nghia
	view2 = gtk_text_view_new();
	gtk_widget_set_size_request(view2, 300, 300);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view2), GTK_WRAP_WORD);

	GtkWidget *scrolling = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolling), view2);

	gtk_fixed_put(GTK_FIXED(fixed), scrolling, 200, 150);
	gtk_widget_set_size_request(scrolling, 250, 200);
	//tao button add new words
	btadd = gtk_button_new_with_label("Add...");
	gtk_fixed_put(GTK_FIXED(fixed), btadd, 490, 15);
	gtk_widget_set_size_request(btadd, 50, 30);

	g_signal_connect(G_OBJECT(btadd), "clicked", G_CALLBACK(add), window2);
	//tao button back
	btback = gtk_button_new_with_label("Back");
	gtk_fixed_put(GTK_FIXED(fixed), btback, 50, 15);
	gtk_widget_set_size_request(btback, 50, 30);

	g_signal_connect(G_OBJECT(btback), "clicked", G_CALLBACK(close_window), window2);
	//kill window2
	g_signal_connect(G_OBJECT(window2), "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window2);

	gtk_main();

	return;
}

static void del(GtkWidget *w, gpointer data)
{
	GtkEntry *entry1 = ((GtkEntry **)data)[0];
	GtkWidget *window1 = ((GtkWidget **)data)[1];

	a = gtk_entry_get_text(entry1);
	char mean[10000], word[50];
	int size;
	BTint x;
	strcpy(word, a);
	if (word[0] == '\0')
		warning(window1, GTK_MESSAGE_WARNING, "Cảnh báo!", "Cần nhập từ muốn xoá.");
	else if (bfndky(book, word, &x) != 0)
		warning(window1, GTK_MESSAGE_ERROR, "Xảy ra lỗi!", "Từ vừa nhập không có trong từ điển.");
	else if (btsel(book, word, mean, 100000, &size) == 0)
	{
		btdel(book, word);
		warning(window1, GTK_MESSAGE_INFO, "Thành công!", "Đã xoá từ khỏi từ điển.");
	}
	else
		warning(window1, GTK_MESSAGE_ERROR, "Xảy ra lỗi!", "Không thể xoá từ khỏi từ điển.");
}

void delwords(GtkWidget widget, gpointer window)
{
	//khoi tao widget
	GtkWidget *window2, *btback, *btdel;
	GtkWidget *fixed, *label, *entry;
	//tao window moi
	window2 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window2), "Delete words");
	gtk_window_set_default_size(GTK_WINDOW(window2), 600, 400);
	gtk_window_set_position(GTK_WINDOW(window2), GTK_WIN_POS_CENTER);
	//tao mau chua vi tri
	fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(window2), fixed);
	//tao label del
	label = gtk_label_new("Delete: ");
	gtk_fixed_put(GTK_FIXED(fixed), label, 140, 150);
	gtk_widget_set_size_request(label, 40, 20);
	//nhap tu can xoa
	entry = gtk_entry_new();
	gtk_widget_set_size_request(entry, 270, 20);
	gtk_entry_set_max_length(GTK_ENTRY(entry), 100);

	gtk_fixed_put(GTK_FIXED(fixed), entry, 200, 150);

	//tao button xoa
	btdel = gtk_button_new_with_label("Delete");
	gtk_fixed_put(GTK_FIXED(fixed), btdel, 490, 15);
	gtk_widget_set_size_request(btdel, 50, 30);

	GtkWidget *data[2];
	data[0] = entry;
	data[1] = window2;

	g_signal_connect(G_OBJECT(btdel), "clicked", G_CALLBACK(del), data);

	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(del), data);
	//tao button back
	btback = gtk_button_new_with_label("Back");
	gtk_fixed_put(GTK_FIXED(fixed), btback, 50, 15);
	gtk_widget_set_size_request(btback, 50, 30);

	g_signal_connect(G_OBJECT(btback), "clicked", G_CALLBACK(close_window), window2);

	g_signal_connect(G_OBJECT(window2), "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window2);

	gtk_main();

	return;
}

int main(int argc, char *argv[])
{
	btinit();
	//book = btopn("tudienanhviet.dat", 0, 1);
	//book = btopn("words.dat", 0, 1);
	book = btopn("data.dat", 0, 1);

	//init();

	GtkWidget *window1;
	GtkWidget *fixed, *image;
	GtkWidget *btn1, *btadd, *btabout, *btdel, *btexit;
	gtk_init(&argc, &argv);

	//giao dien chinh
	window1 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window1), "Dictionary");
	gtk_window_set_default_size(GTK_WINDOW(window1), 750, 600);
	gtk_window_set_position(GTK_WINDOW(window1), GTK_WIN_POS_CENTER);

	fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(window1), fixed);

	//image
	image = gtk_image_new_from_file("rsz_abc.jpg");
	gtk_container_add(GTK_CONTAINER(fixed), image);

	//tao entry
	entry_search = gtk_entry_new();
	gtk_widget_set_size_request(entry_search, 340, 30);
	gtk_entry_set_max_length(GTK_ENTRY(entry_search), 100);

	gtk_fixed_put(GTK_FIXED(fixed), entry_search, 210, 50);

	GtkEntryCompletion *comple = gtk_entry_completion_new();
	gtk_entry_completion_set_text_column(comple, 0);
	list = gtk_list_store_new(10, G_TYPE_STRING, G_TYPE_STRING,
							  G_TYPE_STRING, G_TYPE_STRING,
							  G_TYPE_STRING, G_TYPE_STRING,
							  G_TYPE_STRING, G_TYPE_STRING,
							  G_TYPE_STRING, G_TYPE_STRING);
	gtk_entry_completion_set_model(comple, GTK_TREE_MODEL(list));
	gtk_entry_set_completion(GTK_ENTRY(entry_search), comple);

	btn1 = gtk_button_new_with_label("Search");
	gtk_fixed_put(GTK_FIXED(fixed), btn1, 340, 100);
	gtk_widget_set_size_request(btn1, 80, 30);

	textView = gtk_text_view_new();
	gtk_widget_set_size_request(textView, 1000, 800);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textView), GTK_WRAP_WORD);

	GtkWidget *scrolling = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolling), textView);

	gtk_fixed_put(GTK_FIXED(fixed), scrolling, 70, 170);
	gtk_widget_set_size_request(scrolling, 600, 400);
	gtk_widget_set_visual(scrolling, NULL);

	GtkWidget *data[3];
	data[0] = entry_search;
	data[1] = window1;
	data[2] = textView;

	g_signal_connect(G_OBJECT(entry_search), "key-press-event", G_CALLBACK(search_suggest), NULL);//signal is emitted when a key is pressed

	g_signal_connect(G_OBJECT(entry_search), "activate", G_CALLBACK(search), data);//gets emitted:enter

	g_signal_connect(G_OBJECT(btn1), "clicked", G_CALLBACK(search), data);//signal is emitted when button is clicked

	//thong tin nhom
	btabout = gtk_button_new_with_label("About");
	gtk_fixed_put(GTK_FIXED(fixed), btabout, 177, 10);
	gtk_widget_set_size_request(btabout, 40, 20);
	g_signal_connect(G_OBJECT(btabout), "clicked", G_CALLBACK(about), NULL);

	//them tu
	btadd = gtk_button_new_with_label("Add");
	gtk_fixed_put(GTK_FIXED(fixed), btadd, 71, 10);
	gtk_widget_set_size_request(btadd, 40, 20);
	g_signal_connect(G_OBJECT(btadd), "clicked", G_CALLBACK(addwords), NULL);

	//xoa tu
	btdel = gtk_button_new_with_label("Del");
	gtk_fixed_put(GTK_FIXED(fixed), btdel, 127, 10);
	gtk_widget_set_size_request(btdel, 40, 20);
	g_signal_connect(G_OBJECT(btdel), "clicked", G_CALLBACK(delwords), NULL);

	//thoat chuong trinh
	btexit = gtk_button_new_with_label("Exit");
	gtk_fixed_put(GTK_FIXED(fixed), btexit, 247, 10);
	gtk_widget_set_size_request(btexit, 40, 20);
	g_signal_connect(G_OBJECT(btexit), "clicked", G_CALLBACK(close_window), window1);

	g_signal_connect(G_OBJECT(window1), "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window1); //Recursively shows a widget, and any child widgets (if the widget is a container).

	gtk_main();

	btcls(book);

	return 0;
}
