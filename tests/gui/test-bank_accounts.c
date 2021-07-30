/*
 * test-bank_accounts.c
 * 
 * Copyright 2021 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in 
 * the Software without restriction, including without limitation the rights to 
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
 * of the Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <curl/curl.h>
#include "utils.h"
#include "json-response.h"


enum BANK_CODES_COLUMN
{
	BANK_CODES_COLUMN_code,
	BANK_CODES_COLUMN_name,
	BANK_CODES_COLUMN_half_width_kana,
	BANK_CODES_COLUMN_full_width_kana,
	BANK_CODES_COLUMN_hiragana,
	BANK_CODES_COLUMNS_COUNT
};


struct shell_context
{
	GtkWidget * window;
	GtkWidget * header_bar;
	GtkWidget * accounts_dlg;
	GtkWidget * add_accounts_dlg;
	
	struct http_json_context http[1];
	
	json_object * jbanks_list;
	json_object * jbranches_list;
	
	GtkListStore * bank_codes_store;
};

static struct shell_context g_shell[1];
static void init_windows(struct shell_context * shell);
static int run(struct shell_context * shell);
static void shell_cleanup(struct shell_context * shell)
{
	return;
}

static int load_bank_infos(struct shell_context * shell);

int main(int argc, char **argv)
{
	struct shell_context * shell = g_shell;
	gtk_init(&argc, &argv);
	
	curl_global_init(CURL_GLOBAL_ALL);
	http_json_context_init(shell->http, shell);
	
	load_bank_infos(shell);
	
	init_windows(shell);
	run(shell);
	
	shell_cleanup(shell);
	curl_global_cleanup();
	return 0;
}

static void on_show_accounts_dlg(GtkButton * button, struct shell_context * shell)
{
	GtkWidget * dlg = shell->accounts_dlg;
	assert(dlg);
	
	gtk_widget_show_all(dlg);
	int response = gtk_dialog_run(GTK_DIALOG(dlg));
	switch(response) {
	case GTK_RESPONSE_CLOSE:
		fprintf(stderr, "closed\n");
		break;
	default:
		fprintf(stderr, "response: %d\n", response);
		break;
	}
	
	gtk_widget_hide(dlg);
	return;
}

static void on_bank_account_add(GtkButton * button, struct shell_context * shell)
{
	GtkWidget * dlg = shell->add_accounts_dlg;
	assert(dlg);
	
	gtk_widget_show_all(dlg);
	int response = gtk_dialog_run(GTK_DIALOG(dlg));
	switch(response) {
	case GTK_RESPONSE_APPLY:
		fprintf(stderr, "apply\n");
		break;
	default:
		fprintf(stderr, "response: %d\n", response);
		break;
	}
	
	gtk_widget_hide(dlg);
	return;
}

static void on_bank_account_remove(GtkButton * button, struct shell_context * shell)
{
	
	return;
}


static void init_windows(struct shell_context * shell)
{
	if(NULL == shell) shell = g_shell;
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget * grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(window), grid);
	
	GtkWidget * header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	GtkWidget * button = gtk_button_new_from_icon_name("start-here", GTK_ICON_SIZE_BUTTON);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), button);
	
	
	GtkBuilder * builder = gtk_builder_new_from_file("ui/bank_accounts.ui");
	assert(builder);

	GtkWidget * accounts_dlg = GTK_WIDGET(gtk_builder_get_object(builder, "accounts_dlg"));
	GtkWidget * add_accounts_dlg = GTK_WIDGET(gtk_builder_get_object(builder, "add_accounts_dlg"));
	GtkWidget * bank_account_add = GTK_WIDGET(gtk_builder_get_object(builder, "bank_account_add"));
	GtkWidget * bank_account_remove = GTK_WIDGET(gtk_builder_get_object(builder, "bank_account_remove"));
	
	GtkWidget * bank_codes = GTK_WIDGET(gtk_builder_get_object(builder, "bank_codes"));
	
	GtkCellRenderer * cr = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (bank_codes), cr, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (bank_codes), cr, "text", BANK_CODES_COLUMN_name, NULL);
	cr = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (bank_codes), cr, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (bank_codes), cr, "text", BANK_CODES_COLUMN_code, NULL);
	
	gtk_combo_box_set_model(GTK_COMBO_BOX(bank_codes), GTK_TREE_MODEL(shell->bank_codes_store));
	gtk_combo_box_set_id_column(GTK_COMBO_BOX(bank_codes), BANK_CODES_COLUMN_code);
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(bank_codes), BANK_CODES_COLUMN_name);
	
	shell->window = window;
	shell->accounts_dlg = accounts_dlg;
	shell->add_accounts_dlg = add_accounts_dlg;
	

	
	assert(bank_account_add);
	assert(bank_account_remove);
	
	g_signal_connect(button, "clicked", G_CALLBACK(on_show_accounts_dlg), shell);
	g_signal_connect(bank_account_add, "clicked", G_CALLBACK(on_bank_account_add), shell);
	g_signal_connect(bank_account_remove, "clicked", G_CALLBACK(on_bank_account_remove), shell);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), shell);
	
	return;
}

static int run(struct shell_context * shell)
{
	assert(shell && shell->window);
	
	gtk_widget_show_all(shell->window);
	on_show_accounts_dlg(NULL, shell);
	gtk_main();
	return 0;
}


static int load_bank_infos(struct shell_context * shell)
{
	static const char * get_banklist_url = "https://apis.bankcode-jp.com/v1/banks?limit=20";
	CURL * curl = curl_easy_init();
	assert(curl);
	
	struct http_json_context * http = shell->http;
	struct json_response_context * response = http->response;
	
	json_object * jresponse = http->get(http, get_banklist_url);
	assert(jresponse);
	json_object * jbanks_list = NULL;
	json_bool ok = json_object_object_get_ex(jresponse, "data", &jbanks_list);
	assert(ok && jbanks_list);
	
	GtkTreeIter iter;
	GtkListStore * store = gtk_list_store_new(BANK_CODES_COLUMNS_COUNT,
		G_TYPE_STRING, 
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING);
	assert(store);
	int num_banks = json_object_array_length(jbanks_list);
	assert(num_banks > 0);
	
	printf("num banks: %d\n", num_banks);
	for(int i = 0; i < num_banks; ++i) 
	{
		json_object * jbank = json_object_array_get_idx(jbanks_list, i);
		assert(jbank);
		const char * code = json_get_value(jbank, string, code);
		const char * name = json_get_value(jbank, string, name);
		const char * half_width_kana = json_get_value(jbank, string, half_width_kana);
		const char * full_width_kana = json_get_value(jbank, string, full_width_kana);
		const char * hiragana = json_get_value(jbank, string, hiragana);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 
			BANK_CODES_COLUMN_code, code,
			BANK_CODES_COLUMN_name, name, 
			BANK_CODES_COLUMN_half_width_kana, half_width_kana,
			BANK_CODES_COLUMN_full_width_kana, full_width_kana,
			BANK_CODES_COLUMN_hiragana, hiragana,
			-1);
	}
	
	shell->bank_codes_store = store;
	shell->jbanks_list = json_object_get(jbanks_list);
	json_object_put(jresponse);
	
	return response->err_code;
}

