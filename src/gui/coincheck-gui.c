/*
 * coincheck-gui.c
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
#include <pthread.h>

#include "coincheck-gui.h"
#include "trading_agency.h"
#include "trading_agency_coincheck.h"
#include "shell.h"
#include "utils.h"

panel_view_t * panel_view_init(panel_view_t * panel, const char * title, shell_context_t * shell)
{
	if(NULL == panel) panel = calloc(1, sizeof(*panel));
	assert(panel);
	
	pthread_mutex_init(&panel->mutex, NULL);
	
	panel->shell = shell;
	if(title) strncpy(panel->title, title, sizeof(panel->title));
	return panel;
}
void panel_view_cleanup(panel_view_t * panel)
{
	return;
}

enum ORDER_BOOK_COLUMN
{
	ORDER_BOOK_COLUMN_rate,
	ORDER_BOOK_COLUMN_amount,
	ORDER_BOOK_COLUMN_scales,
	ORDER_BOOK_COLUMNS_COUNT
};
int panel_view_load_from_builder(panel_view_t * panel, GtkBuilder * builder)
{
	assert(panel && builder);
	panel->frame = GTK_WIDGET(gtk_builder_get_object(builder, "main_panel"));
	panel->btc_balance = GTK_WIDGET(gtk_builder_get_object(builder, "btc_balance"));
	panel->btc_in_use = GTK_WIDGET(gtk_builder_get_object(builder, "btc_in_use"));
	panel->jpy_balance = GTK_WIDGET(gtk_builder_get_object(builder, "jpy_balance"));
	panel->jpy_in_use = GTK_WIDGET(gtk_builder_get_object(builder, "jpy_in_use"));
	panel->da_chart = GTK_WIDGET(gtk_builder_get_object(builder, "da_chart"));
	panel->ask_orders = GTK_WIDGET(gtk_builder_get_object(builder, "ask_orders"));
	panel->bid_orders = GTK_WIDGET(gtk_builder_get_object(builder, "bid_orders"));
	
	// init ask tree
	GtkListStore * store = NULL;
	GtkTreeView * tree = GTK_TREE_VIEW(panel->ask_orders);
	assert(tree);
	GtkTreeViewColumn * col = NULL;
	GtkCellRenderer * cr = NULL;
	
	cr = gtk_cell_renderer_progress_new();
	g_object_set(cr, 
		"text", "",
		"inverted", TRUE, 
		NULL);
	col = gtk_tree_view_column_new_with_attributes("volumes", cr, 
		"value", ORDER_BOOK_COLUMN_scales,
		NULL);
	gtk_tree_view_column_set_spacing(col, 5);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("amount", cr, "text", ORDER_BOOK_COLUMN_amount, NULL);
	gtk_tree_view_column_set_min_width(col, 80);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	g_object_set(cr, "foreground", "green", NULL);
	col = gtk_tree_view_column_new_with_attributes("rate", cr, 
		"text", ORDER_BOOK_COLUMN_rate, 
		NULL);
	gtk_tree_view_column_set_min_width(col, 80);
	gtk_tree_view_append_column(tree, col);
	
	store = gtk_list_store_new(ORDER_BOOK_COLUMNS_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(tree, GTK_TREE_MODEL(store));
	
	// init bid tree
	tree = GTK_TREE_VIEW(panel->bid_orders);
	assert(tree);
	cr = gtk_cell_renderer_text_new();
	g_object_set(cr, "foreground", "red", NULL);
	col = gtk_tree_view_column_new_with_attributes("rate", cr, 
		"text", ORDER_BOOK_COLUMN_rate, 
		NULL);
	gtk_tree_view_column_set_min_width(col, 80);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("amount", cr, "text", ORDER_BOOK_COLUMN_amount, NULL);
	gtk_tree_view_column_set_min_width(col, 80);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_progress_new();
	g_object_set(cr, 
		"text", "",
		"inverted", FALSE, 
		NULL);
	col = gtk_tree_view_column_new_with_attributes("volumes", cr, 
		"value", ORDER_BOOK_COLUMN_scales, 
		NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	store = gtk_list_store_new(ORDER_BOOK_COLUMNS_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(tree, GTK_TREE_MODEL(store));
	
	
	g_object_unref(store);
	return 0;
}


static gboolean on_panel_view_da_draw(GtkWidget * da, cairo_t * cr, panel_view_t * panel)
{
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_paint(cr);
	return FALSE;
}

static void update_balance(panel_view_t * panel, json_object * jbalance)
{
	if(NULL == jbalance) return;
	
	GtkEntry * btc_balance = GTK_ENTRY(panel->btc_balance);
	GtkEntry * btc_in_use = GTK_ENTRY(panel->btc_in_use);
	GtkEntry * jpy_balance = GTK_ENTRY(panel->jpy_balance);
	GtkEntry * jpy_in_use = GTK_ENTRY(panel->jpy_in_use);
	
	// clear data
	gtk_entry_set_text(btc_balance, "");
	gtk_entry_set_text(btc_in_use,  "");
	gtk_entry_set_text(jpy_balance, "");
	gtk_entry_set_text(jpy_in_use,  "");
	
	json_object * jstatus = NULL;
	json_bool ok = json_object_object_get_ex(jbalance, "success", &jstatus);
	if(!ok || !jstatus || ! json_object_get_boolean(jstatus)) return;
	
	const char * btc = json_get_value(jbalance, string, btc);
	const char * jpy = json_get_value(jbalance, string, jpy);
	gtk_entry_set_text(btc_balance, btc?btc:"");
	//~ gtk_entry_set_text(btc_in_use, "");
	gtk_entry_set_text(jpy_balance, jpy?jpy:"");
	//~ gtk_entry_set_text(jpy_in_use, "");
	return;
}

int coincheck_panel_init(trading_agency_t * agent, shell_context_t * shell)
{
	panel_view_t * panel = shell_get_main_panel(shell, "coincheck");
	assert(panel);
	
	assert(agent);
	panel->agent = agent;
	
	GtkWidget * da_chart = panel->da_chart;
	assert(da_chart);
	
	if(da_chart) {
		g_signal_connect(da_chart, "draw", G_CALLBACK(on_panel_view_da_draw), panel);
	}
	
	
	json_object * jbalance = NULL;
	int rc = 0;
	rc = coincheck_account_get_balance(agent, &jbalance);
	if(0 == rc && jbalance) {
		update_balance(panel, jbalance);
	}
	if(jbalance) json_object_put(jbalance);
	
	return 0;
}

static void update_orders(panel_view_t * panel, json_object * jorders)
{
	if(NULL == jorders) return;
	
	//~ fprintf(stderr, "order_book: %s\n", json_object_to_json_string_ext(jorders, JSON_C_TO_STRING_SPACED));
	json_object * jasks = NULL;
	json_object * jbids = NULL;
	json_bool ok = 0;
	ok = json_object_object_get_ex(jorders, "asks", &jasks);
	assert(ok && jasks);
	
	ok = json_object_object_get_ex(jorders, "bids", &jbids);
	assert(ok && jbids);
	
	int num_asks = json_object_array_length(jasks);
	int num_bids = json_object_array_length(jbids);
	
	struct order_book_data *asks_list = NULL, *bids_list = NULL;
	
	double max_amount = 0.0;
	if(num_asks > 0) {
		asks_list = calloc(num_asks, sizeof(*asks_list));
		assert(asks_list);
		
		for(int i = 0; i < num_asks; ++i) {
			json_object * jask = json_object_array_get_idx(jasks, i);
			assert(jask);
			asks_list[i].rate = json_object_get_string(json_object_array_get_idx(jask, 0));
			asks_list[i].amount = json_object_get_string(json_object_array_get_idx(jask, 1));
			asks_list[i].d_amount = atof(asks_list[i].amount);
			
			if(i >= 30) continue;	// only check the first 30 orders 
			if(asks_list[i].d_amount > max_amount) max_amount = asks_list[i].d_amount;
		}
	}
	
	if(num_bids > 0) {
		bids_list = calloc(num_bids, sizeof(*bids_list));
		assert(bids_list);
		
		for(int i = 0; i < num_bids; ++i) {
			json_object * jbid = json_object_array_get_idx(jbids, i);
			assert(jbid);
			bids_list[i].rate = json_object_get_string(json_object_array_get_idx(jbid, 0));
			bids_list[i].amount = json_object_get_string(json_object_array_get_idx(jbid, 1));
			bids_list[i].d_amount = atof(bids_list[i].amount);
			
			if(i >= 30) continue;	// only check the first 30 orders 	
			if(bids_list[i].d_amount > max_amount) max_amount = bids_list[i].d_amount;
		}
	}
	
	if(max_amount > 0) {
		for(int i = 0; i < num_asks; ++i) {
			asks_list[i].scales = asks_list[i].d_amount / max_amount * 100;
			if(asks_list[i].scales > 100.0) asks_list[i].scales = 100;
		}
		for(int i = 0; i < num_bids; ++i) {
			bids_list[i].scales = bids_list[i].d_amount / max_amount * 100;
			if(bids_list[i].scales > 100.0) bids_list[i].scales = 100;
		}
	}
	
	GtkListStore * asks_store = gtk_list_store_new(ORDER_BOOK_COLUMNS_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE);
	GtkTreeIter iter_asks;
	for(int i = 0; i < num_asks; ++i) {
		gtk_list_store_append(asks_store, &iter_asks);
		gtk_list_store_set(asks_store, &iter_asks,
			ORDER_BOOK_COLUMN_rate,   asks_list[i].rate,
			ORDER_BOOK_COLUMN_amount, asks_list[i].amount,
			ORDER_BOOK_COLUMN_scales, asks_list[i].scales,
			-1);
	}
	
	GtkListStore * bids_store = gtk_list_store_new(ORDER_BOOK_COLUMNS_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE);
	GtkTreeIter iter_bids;
	for(int i = 0; i < num_bids; ++i) {
		gtk_list_store_append(bids_store, &iter_bids);
		gtk_list_store_set(bids_store, &iter_bids,
			ORDER_BOOK_COLUMN_rate,   bids_list[i].rate,
			ORDER_BOOK_COLUMN_amount, bids_list[i].amount,
			ORDER_BOOK_COLUMN_scales, bids_list[i].scales,
			-1);
	}
	
	pthread_mutex_lock(&panel->mutex);
	if(panel->asks_list) free(panel->asks_list);
	if(panel->bids_list) free(panel->bids_list);
	panel->num_asks = num_asks;
	panel->asks_list = asks_list;
	panel->num_bids = num_bids;
	panel->bids_list = bids_list;
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(panel->ask_orders), GTK_TREE_MODEL(asks_store));
	gtk_tree_view_set_model(GTK_TREE_VIEW(panel->bid_orders), GTK_TREE_MODEL(bids_store));
	pthread_mutex_unlock(&panel->mutex);
	
	g_object_unref(asks_store);
	g_object_unref(bids_store);
	
}
int coincheck_panel_update_order_book(panel_view_t * panel)
{
	trading_agency_t * agent = panel->agent;
	assert(agent);
	
	json_object * jorders = NULL;
	int rc = 0;
	rc = coincheck_public_get_order_book(agent, &jorders);
	if(rc) return -1;
	
	update_orders(panel, jorders);

	json_object_put(jorders);
	
	return 0;
}
