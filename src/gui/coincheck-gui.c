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


//~ static int s_first_run;
static gboolean hide_info(GtkWidget * info_bar)
{
	//~ s_first_run = !s_first_run;
	//~ if(s_first_run) return G_SOURCE_CONTINUE;
	
	if(info_bar) gtk_widget_hide(info_bar);
	return G_SOURCE_REMOVE;
}

static void show_info(shell_context_t * shell, const char * fmt, ...)
{
	GtkWidget * info_bar = shell->info_bar;
	GtkWidget * message_label = shell->message_label;
	gtk_info_bar_set_message_type(GTK_INFO_BAR(info_bar), GTK_MESSAGE_INFO);
	
	char msg[4096] = "";
	va_list ap;
	va_start(ap, fmt);
	int cb = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	printf("msg: %s\n", msg);
	if(cb > 0) {
		gtk_label_set_text(GTK_LABEL(message_label), msg);
		gtk_widget_show(message_label);
		gtk_widget_show(info_bar);
		
		const guint inteval = 5000; // show infobar 3 seconds
		g_timeout_add(inteval, (GSourceFunc)hide_info, info_bar);
	}
	return;
}


static void coincheck_panel_btc_buy(GtkWidget * button, panel_view_t * panel)
{
	GtkWidget * entry = panel->btc_buy_rate;
	GtkWidget * spin = panel->btc_buy_amount;
	GtkWidget * btc_buy = panel->btc_buy;
	
	double rate = 0.0;
	double amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	const char * sz_rate = gtk_entry_get_text(GTK_ENTRY(entry));
	if(sz_rate) rate = atof(sz_rate);
	
	if(rate < 1.0 || amount < 0.001) {
		// todo: popup error message
		gtk_widget_set_sensitive(btc_buy, FALSE);
		return;
	}
	
	///< @todo : update order history and unsettled order list
	trading_agency_t * agent = panel->agent;
	assert(agent);
	
	json_object * jresult = NULL;
	int rc = coincheck_new_order(agent, 
		"btc_jpy", "buy", 
		rate, amount, &jresult);
	if(rc) {
		show_info(panel->shell, 
			"%s(rate=%s, amount=%.3f): ERROR, rc = %d", __FUNCTION__, sz_rate, amount, rc);
	}
	if(jresult) {
		show_info(panel->shell, 
			"%s(rate=%s, amount=%.3f): %s", __FUNCTION__, sz_rate, amount,
			json_object_to_json_string_ext(jresult, JSON_C_TO_STRING_SPACED)
		);
		json_object_put(jresult);
	}
	
}
static void coincheck_panel_btc_sell(GtkWidget * button, panel_view_t * panel)
{
	GtkWidget * entry = panel->btc_sell_rate;
	GtkWidget * spin = panel->btc_sell_amount;
	GtkWidget * btc_sell = panel->btc_sell;
	
	double rate = 0.0;
	double amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	const char * sz_rate = gtk_entry_get_text(GTK_ENTRY(entry));
	if(sz_rate) rate = atof(sz_rate);

	if(rate < 1.0 || amount < 0.001) {
		// todo: popup error message
		gtk_widget_set_sensitive(btc_sell, FALSE);
		return;
	}
	
	///< @todo : update order history and unsettled order list
	trading_agency_t * agent = panel->agent;
	json_object * jresult = NULL;
	int rc = coincheck_new_order(agent, 
		"btc_jpy", "sell", 
		rate, amount, &jresult);
	if(rc) {
		show_info(panel->shell, 
			"%s(rate=%s, amount=%.3f): ERROR, rc = %d", __FUNCTION__, sz_rate, amount, rc);
	}
	if(jresult) {
		show_info(panel->shell, 
			"%s(rate=%s, amount=%.3f): %s", rc, __FUNCTION__, sz_rate, amount,
			json_object_to_json_string_ext(jresult, JSON_C_TO_STRING_SPACED)
		);
		json_object_put(jresult);
	}
}
static void coincheck_panel_buy_rate_changed(GtkEntry * entry, panel_view_t * panel)
{
	GtkWidget * btc_buy = panel->btc_buy;
	GtkWidget * spin = panel->btc_buy_amount;
	assert(spin);
	
	int length = gtk_entry_get_text_length(entry);
	const char * text = gtk_entry_get_text(entry);
	if(length <= 0) return;
	
	double rate = atof(text);
	if(rate < 1.0) {
		// todo: popup error message
		gtk_widget_set_sensitive(btc_buy, FALSE);
		return;
	}
	char verified_text[100] = "";
	snprintf(verified_text, sizeof(verified_text), "%.2f", rate);
	gtk_entry_set_text(entry, verified_text);
	
	double amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	if(amount < 0.001) {
		gtk_widget_grab_focus(spin);
	}else {
		gtk_widget_grab_focus(btc_buy);
	}
	gtk_widget_set_sensitive(btc_buy, ((rate >= 1.0) && (amount >= 0.001)));
	return;
}
static void coincheck_panel_buy_amount_changed(GtkSpinButton  * spin, panel_view_t * panel)
{
	GtkWidget * btc_buy = panel->btc_buy;
	GtkWidget * entry = panel->btc_buy_rate;
	assert(entry);
	
	int length = gtk_entry_get_text_length(GTK_ENTRY(entry));
	if(length <= 0) return;
	const char * text = gtk_entry_get_text(GTK_ENTRY(entry));
	double amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	if(amount < 0.001) return;
	
	double rate = atof(text);
	if(rate < 1.0) {
		// todo: popup error message
		gtk_widget_grab_focus(entry);
		return;
	}else {
		gtk_widget_grab_focus(btc_buy);
	}
	
	gtk_widget_set_sensitive(btc_buy, ((rate >= 1.0) && (amount >= 0.001)));
	return;
}
static void coincheck_panel_sell_rate_changed(GtkEntry * entry, panel_view_t * panel)
{
	GtkWidget * btc_sell = panel->btc_sell;
	GtkWidget * spin = panel->btc_sell_amount;
	assert(spin);
	
	int length = gtk_entry_get_text_length(entry);
	const char * text = gtk_entry_get_text(entry);
	if(length <= 0) return;
	
	double rate = atof(text);
	if(rate < 1.0) {
		// todo: popup error message
		gtk_widget_set_sensitive(btc_sell, FALSE);
		return;
	}
	char verified_text[100] = "";
	snprintf(verified_text, sizeof(verified_text), "%.2f", rate);
	gtk_entry_set_text(entry, verified_text);
	
	double amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	if(amount < 0.001) {
		gtk_widget_grab_focus(spin);
	}else {
		gtk_widget_grab_focus(btc_sell);
	}
	gtk_widget_set_sensitive(btc_sell, ((rate >= 1.0) && (amount >= 0.001)));
	return;
}
static void coincheck_panel_sell_amount_changed(GtkSpinButton  * spin, panel_view_t * panel)
{
	GtkWidget * btc_sell = panel->btc_sell;
	GtkWidget * entry = panel->btc_sell_rate;
	assert(entry);
	
	int length = gtk_entry_get_text_length(GTK_ENTRY(entry));
	if(length <= 0) return;
	const char * text = gtk_entry_get_text(GTK_ENTRY(entry));
	double amount = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
	if(amount < 0.001) return;
	
	double rate = atof(text);
	if(rate < 1.0) {
		// todo: popup error message
		gtk_widget_grab_focus(entry);
		return;
	}else {
		gtk_widget_grab_focus(btc_sell);
	}
	
	gtk_widget_set_sensitive(btc_sell, ((rate >= 1.0) && (amount >= 0.001)));
	return;
}

static void on_ask_orders_selection_changed(GtkTreeSelection *selection, panel_view_t * panel)
{
	GtkTreeModel * model = NULL;
	GtkTreeIter iter;
	
	gboolean ok = gtk_tree_selection_get_selected(selection, &model, &iter);
	if(!ok) return;
	
	gchar * rate = NULL;
	gchar * amount = NULL;
	
	gtk_tree_model_get(model, &iter, ORDER_BOOK_COLUMN_rate, &rate, ORDER_BOOK_COLUMN_amount, &amount, -1);
	
	// update btc_sell ( to sell to an existing ask order ) 
	if(rate) gtk_entry_set_text(GTK_ENTRY(panel->btc_sell_rate), rate);
	if(amount) gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel->btc_sell_amount), (double)atof(amount));
	coincheck_panel_buy_rate_changed(GTK_ENTRY(panel->btc_sell_rate), panel);
}


static void on_bid_orders_selection_changed(GtkTreeSelection *selection, panel_view_t * panel)
{
	GtkTreeModel * model = NULL;
	GtkTreeIter iter;
	
	gboolean ok = gtk_tree_selection_get_selected(selection, &model, &iter);
	if(!ok) return;
	
	gchar * rate = NULL;
	gchar * amount = NULL;
	
	gtk_tree_model_get(model, &iter, ORDER_BOOK_COLUMN_rate, &rate, ORDER_BOOK_COLUMN_amount, &amount, -1);
	
	// update btc_buy ( to buy from an existing bid order ) 
	if(rate) gtk_entry_set_text(GTK_ENTRY(panel->btc_buy_rate), rate);
	if(amount) gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel->btc_buy_amount), (double)atof(amount));
	coincheck_panel_buy_rate_changed(GTK_ENTRY(panel->btc_buy_rate), panel);
}


int panel_view_load_from_builder(panel_view_t * panel, GtkBuilder * builder)
{
	assert(panel && builder);
	panel->frame       = GTK_WIDGET(gtk_builder_get_object(builder, "main_panel"));
	panel->btc_balance = GTK_WIDGET(gtk_builder_get_object(builder, "btc_balance"));
	panel->btc_in_use  = GTK_WIDGET(gtk_builder_get_object(builder, "btc_in_use"));
	panel->jpy_balance = GTK_WIDGET(gtk_builder_get_object(builder, "jpy_balance"));
	panel->jpy_in_use  = GTK_WIDGET(gtk_builder_get_object(builder, "jpy_in_use"));
	panel->da_chart    = GTK_WIDGET(gtk_builder_get_object(builder, "da_chart"));
	panel->ask_orders  = GTK_WIDGET(gtk_builder_get_object(builder, "ask_orders"));
	panel->bid_orders  = GTK_WIDGET(gtk_builder_get_object(builder, "bid_orders"));
	
	GtkTreeSelection * ask_orders_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(panel->ask_orders));
	GtkTreeSelection * bid_orders_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(panel->bid_orders));
	assert(ask_orders_selection && bid_orders_selection);
	g_signal_connect(ask_orders_selection, "changed", G_CALLBACK(on_ask_orders_selection_changed), panel);
	g_signal_connect(bid_orders_selection, "changed", G_CALLBACK(on_bid_orders_selection_changed), panel);
	
	
	panel->btc_buy         = GTK_WIDGET(gtk_builder_get_object(builder, "btc_buy"));
	panel->btc_buy_rate    = GTK_WIDGET(gtk_builder_get_object(builder, "btc_buy_rate"));
	panel->btc_buy_amount  = GTK_WIDGET(gtk_builder_get_object(builder, "btc_buy_amount"));
	panel->btc_sell        = GTK_WIDGET(gtk_builder_get_object(builder, "btc_sell"));
	panel->btc_sell_rate   = GTK_WIDGET(gtk_builder_get_object(builder, "btc_sell_rate"));
	panel->btc_sell_amount = GTK_WIDGET(gtk_builder_get_object(builder, "btc_sell_amount"));
	assert(panel->btc_buy && panel->btc_buy_rate && panel->btc_buy_amount);
	assert(panel->btc_sell && panel->btc_sell_rate && panel->btc_sell_amount);
	
	gtk_widget_set_sensitive(panel->btc_buy, FALSE);
	gtk_widget_set_sensitive(panel->btc_sell, FALSE);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(panel->btc_buy_amount), 0.001, 100.0);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(panel->btc_sell_amount), 0.001, 100.0);
	
	g_signal_connect(panel->btc_buy, "clicked", G_CALLBACK(coincheck_panel_btc_buy), panel);
	g_signal_connect(panel->btc_sell, "clicked", G_CALLBACK(coincheck_panel_btc_sell), panel);
	g_signal_connect(panel->btc_buy_rate, "activate", G_CALLBACK(coincheck_panel_buy_rate_changed), panel);
	g_signal_connect(panel->btc_buy_amount, "value-changed", G_CALLBACK(coincheck_panel_buy_amount_changed), panel);
	g_signal_connect(panel->btc_sell_rate, "activate", G_CALLBACK(coincheck_panel_sell_rate_changed), panel);
	g_signal_connect(panel->btc_sell_amount, "value-changed", G_CALLBACK(coincheck_panel_sell_amount_changed), panel);
	
	
	
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

static void update_balance(panel_view_t * panel)
{
	assert(panel && panel->agent);
	json_object * jbalance = NULL;
	int rc = 0;
	rc = coincheck_account_get_balance(panel->agent, &jbalance);
	if(NULL == jbalance) return;
	assert(0 == rc);
	
	GtkEntry * btc_balance = GTK_ENTRY(panel->btc_balance);
	GtkEntry * btc_in_use = GTK_ENTRY(panel->btc_in_use);
	GtkEntry * jpy_balance = GTK_ENTRY(panel->jpy_balance);
	GtkEntry * jpy_in_use = GTK_ENTRY(panel->jpy_in_use);
	
	json_object * jstatus = NULL;
	json_bool ok = json_object_object_get_ex(jbalance, "success", &jstatus);
	if(!ok || !jstatus || ! json_object_get_boolean(jstatus)) {
		json_object_put(jbalance);
		return;
	}
	
	const char * btc = json_get_value_default(jbalance, string, btc, "");
	const char * jpy = json_get_value_default(jbalance, string, jpy, "");
	const char * btc_reserved = json_get_value_default(jbalance, string, btc_reserved, "");
	const char * jpy_reserved = json_get_value_default(jbalance, string, jpy_reserved, "");
	
	gtk_entry_set_text(btc_balance, btc);
	gtk_entry_set_text(btc_in_use, btc_reserved);
	gtk_entry_set_text(jpy_balance, jpy);
	gtk_entry_set_text(jpy_in_use, jpy_reserved);
	
	json_object_put(jbalance);
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
	
	update_balance(panel);
	
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

#define MAX_ORDERS_DEPTH	(30) // only check the first 30 orders 
	double max_amount = 0.0;
	if(num_asks > 0) {
		if(num_asks > MAX_ORDERS_DEPTH) num_asks = MAX_ORDERS_DEPTH;
		
		asks_list = calloc(num_asks, sizeof(*asks_list));
		assert(asks_list);
		
		for(int i = 0; i < num_asks; ++i) {
			json_object * jask = json_object_array_get_idx(jasks, i);
			assert(jask);
			asks_list[i].rate = json_object_get_string(json_object_array_get_idx(jask, 0));
			asks_list[i].amount = json_object_get_string(json_object_array_get_idx(jask, 1));
			asks_list[i].d_amount = atof(asks_list[i].amount);
			if(asks_list[i].d_amount > max_amount) max_amount = asks_list[i].d_amount;
		}
	}
	
	
	if(num_bids > 0) {
		if(num_bids > MAX_ORDERS_DEPTH) num_bids = MAX_ORDERS_DEPTH;
		bids_list = calloc(num_bids, sizeof(*bids_list));
		assert(bids_list);
		
		for(int i = 0; i < num_bids; ++i) {
			json_object * jbid = json_object_array_get_idx(jbids, i);
			assert(jbid);
			bids_list[i].rate = json_object_get_string(json_object_array_get_idx(jbid, 0));
			bids_list[i].amount = json_object_get_string(json_object_array_get_idx(jbid, 1));
			bids_list[i].d_amount = atof(bids_list[i].amount);
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

static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
#define AUTO_UNLOCK_MUTEX_PTR __attribute__((cleanup(auto_unlock_mutex_ptr)))
static void auto_unlock_mutex_ptr(void * ptr)
{
	pthread_mutex_t ** p_mutex = ptr;
	if(p_mutex && *p_mutex) pthread_mutex_unlock(*p_mutex);
	return;
}

#define auto_lock() AUTO_UNLOCK_MUTEX_PTR pthread_mutex_t *_m_ = &s_mutex; \
		pthread_mutex_lock(_m_)

extern gboolean coincheck_check_banlance(shell_context_t * shell)
{
	if(!shell->is_running || shell->quit) return G_SOURCE_REMOVE;
	if(!shell->action_state) return G_SOURCE_CONTINUE;
	
	panel_view_t * coincheck_panel = shell_get_main_panel(shell, "coincheck");
	
	auto_lock();
	update_balance(coincheck_panel);
	
	return G_SOURCE_CONTINUE;
}
extern gboolean coincheck_update_order_book(shell_context_t * shell)
{
	if(!shell->is_running || shell->quit) return G_SOURCE_REMOVE;
	if(!shell->action_state) return G_SOURCE_CONTINUE;
	
	panel_view_t * coincheck_panel = shell_get_main_panel(shell, "coincheck");
	
	auto_lock();
	coincheck_panel_update_order_book(coincheck_panel);
	
	
	return G_SOURCE_CONTINUE;
}
