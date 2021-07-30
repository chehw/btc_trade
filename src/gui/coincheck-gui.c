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

#include "order_history.h"

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


static void update_balance(panel_view_t * panel);
static gboolean update_orders_history(panel_view_t * panel);

enum ORDER_BOOK_COLUMN
{
	ORDER_BOOK_COLUMN_rate,
	ORDER_BOOK_COLUMN_amount,
	ORDER_BOOK_COLUMN_scales,
	ORDER_BOOK_COLUMNS_COUNT
};

static gboolean hide_info(GtkWidget * info_bar)
{
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
		
		const guint inteval = 5000; // show infobar 5 seconds
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
		
		update_balance(panel);
		update_orders_history(panel);
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
		update_balance(panel);
		update_orders_history(panel);
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

/**************************************
 * orders book
***************************************/
static void on_ask_orders_selection_changed(GtkTreeSelection *selection, panel_view_t * panel)
{
	GtkTreeModel * model = NULL;
	GtkTreeIter iter;
	
	gboolean ok = gtk_tree_selection_get_selected(selection, &model, &iter);
	if(!ok) return;
	
	gchar * rate = NULL;
	gchar * amount = NULL;
	
	gtk_tree_model_get(model, &iter, ORDER_BOOK_COLUMN_rate, &rate, ORDER_BOOK_COLUMN_amount, &amount, -1);
	
	// update btc_buy ( to buy from an existing ask order ) 
	if(rate) gtk_entry_set_text(GTK_ENTRY(panel->btc_buy_rate), rate);
	if(amount) gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel->btc_buy_amount), (double)atof(amount));
	coincheck_panel_buy_rate_changed(GTK_ENTRY(panel->btc_buy_rate), panel);
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
	
	// update btc_sell ( to sell to an existing bid order ) 
	if(rate) gtk_entry_set_text(GTK_ENTRY(panel->btc_sell_rate), rate);
	if(amount) gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel->btc_sell_amount), (double)atof(amount));
	coincheck_panel_buy_rate_changed(GTK_ENTRY(panel->btc_sell_rate), panel);
}

/**************************************
 * tickers
***************************************/
void draw_tickers(panel_view_t * panel);
static gboolean update_tickers(panel_view_t * panel)
{
	assert(panel && panel->shell);
	shell_context_t * shell = panel->shell;
	if(shell->quit) return G_SOURCE_REMOVE;
	if(!shell->action_state || !shell->is_running) return G_SOURCE_CONTINUE;
	
	struct panel_ticker_context * ctx = panel->ticker_ctx;
	int rc = 0;
	trading_agency_t * agent = panel->agent;
	json_object * jtickers = NULL;
	rc = coincheck_public_get_ticker(agent, &jtickers);
	if(0 == rc && jtickers) {
		panel_ticker_append(ctx, jtickers);
	}
	if(jtickers) json_object_put(jtickers);
	
	draw_tickers(panel);
	return G_SOURCE_CONTINUE;
}

/**************************************
 * order history
***************************************/
static gboolean update_orders_history(panel_view_t * panel)
{
	assert(panel && panel->shell);
	shell_context_t * shell = panel->shell;
	if(shell->quit) return G_SOURCE_REMOVE;
	
	struct order_history * history = panel->orders;
	int rc = history->get_orders(history, panel->agent);
	if(rc) return G_SOURCE_CONTINUE;
	
	order_history_update(history, GTK_TREE_VIEW(panel->orders_tree), GTK_TREE_VIEW(panel->unsettled_tree));
	return G_SOURCE_CONTINUE;
}

static gboolean on_panel_view_da_draw(GtkWidget * da, cairo_t * cr, panel_view_t * panel)
{
	static int margin = 10;
	int width = gtk_widget_get_allocated_width(da);
	int height = gtk_widget_get_allocated_height(da);
	
	if(width <= 1 || height <= 1) return FALSE;
	
	cairo_surface_t * surface = panel->chart_ctx.surface;
	int image_width = panel->chart_ctx.image_width;
	int image_height = panel->chart_ctx.image_height;
	
	width -= margin * 2;
	height -= margin * 2;
	
	if(NULL == surface || image_width <= 1 || image_height <= 1 ) 
	{
		cairo_set_source_rgba(cr, 0, 0, 0, 1);
		cairo_paint(cr);
		return FALSE;
	}
	
	double scale_x = (double)width / (double)image_width;
	double scale_y = (double)height / (double)image_height;
	
	cairo_scale(cr, scale_x, scale_y);
	cairo_set_source_surface(cr, surface, margin, margin);
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


/****************************************************
 * order book
***************************************************/
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
int update_order_book(panel_view_t * panel)
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

/*****************************************************
 * background tasks 
 * ( on_timeout callbacks <== g_timeout_add)
***************************************************/
gboolean coincheck_check_banlance(shell_context_t * shell)
{
	if(shell->quit) return G_SOURCE_REMOVE;
	if(!shell->action_state || !shell->is_running) return G_SOURCE_CONTINUE;
	
	panel_view_t * panel = shell_get_main_panel(shell, "coincheck");
	if(!panel->agent) return G_SOURCE_CONTINUE;
	
	auto_lock();
	update_balance(panel);
	
	return G_SOURCE_CONTINUE;
}

gboolean coincheck_update_order_book(shell_context_t * shell)
{
	if(!shell->is_running || shell->quit) return G_SOURCE_REMOVE;
	if(!shell->action_state) return G_SOURCE_CONTINUE;
	
	panel_view_t * coincheck_panel = shell_get_main_panel(shell, "coincheck");
	
	auto_lock();
	update_order_book(coincheck_panel);
	return G_SOURCE_CONTINUE;
}

/*********************************
 * panel::tickers
*********************************/
#define PANEL_TICKER_MAX_HISTORY_SIZE (1000000)
struct panel_ticker_context * panel_ticker_context_init(struct panel_ticker_context * ctx, size_t max_history_size)
{
	if(NULL == ctx) ctx = calloc(1, sizeof(*ctx));
	if(max_history_size <= 0) max_history_size = PANEL_TICKER_MAX_HISTORY_SIZE;
	
	struct coincheck_ticker * history = calloc(max_history_size, sizeof(*history));
	assert(history);
	
	ctx->tickers_history = history;
	ctx->history_size = max_history_size;
	return ctx;
}
void panel_ticker_context_cleanup(struct panel_ticker_context * ctx)
{
	if(NULL == ctx) return;
	if(ctx->tickers_history) {
		free(ctx->tickers_history);
		ctx->tickers_history = NULL;
	}
	ctx->history_size = 0;
	ctx->start_pos = 0;
	ctx->length = 0;
	return;
}

ssize_t panel_ticker_get_lastest_history(struct panel_ticker_context * ctx, size_t count, struct coincheck_ticker ** p_tickers)
{
	if(NULL == ctx->tickers_history || ctx->history_size == 0) return -1;
	if(count == 0 || count > ctx->length) count = ctx->length;
	if(count == 0) return 0;
	if(NULL == p_tickers) return (ssize_t)count;
	
	size_t start_pos = ctx->start_pos + ctx->length - count;
	size_t end_pos = start_pos + count;
	if(end_pos > ctx->history_size) end_pos -= ctx->history_size;
	
	struct coincheck_ticker * tickers = *p_tickers;
	if(NULL == tickers) {
		tickers = malloc(count * sizeof(*tickers));
		assert(tickers);
		*p_tickers = tickers;
	}
	
	if(end_pos > start_pos) {
		memcpy(tickers, ctx->tickers_history + start_pos, sizeof(*tickers) * count);
	}else {
		size_t partial_length = ctx->history_size - start_pos;
		assert(partial_length > 0 && end_pos > 0);
		
		memcpy(tickers, ctx->tickers_history + start_pos, sizeof(*tickers) * partial_length);
		memcpy(tickers + partial_length, ctx->tickers_history,  sizeof(*tickers) * end_pos);
	}
	return (ssize_t)count;
}

int panel_ticker_load_from_builder(struct panel_ticker_context * ctx, GtkBuilder * builder)
{
	GtkWidget *last = NULL, *ask = NULL, *bid = NULL, *high = NULL, *low = NULL, *volume = NULL;
	ctx->widget.last = last = GTK_WIDGET(gtk_builder_get_object(builder, "ticker_last"));
	ctx->widget.ask = ask = GTK_WIDGET(gtk_builder_get_object(builder, "ticker_ask"));
	ctx->widget.bid = bid = GTK_WIDGET(gtk_builder_get_object(builder, "ticker_bid"));
	ctx->widget.high = high = GTK_WIDGET(gtk_builder_get_object(builder, "ticker_high"));
	ctx->widget.low = low = GTK_WIDGET(gtk_builder_get_object(builder, "ticker_low"));
	ctx->widget.volume = volume = GTK_WIDGET(gtk_builder_get_object(builder, "ticker_volume"));
	
	assert(last && ask && bid && high && low && volume);
	return 0;
}

static int ticker_history_append(struct panel_ticker_context * ctx, const struct coincheck_ticker * ticker)
{
	assert(ctx->tickers_history && ctx->history_size > 0);
	size_t cur_pos = ctx->start_pos + ctx->length;
	cur_pos %= ctx->history_size;
	
	ctx->tickers_history[cur_pos] = *ticker;
	
	if(ctx->length < ctx->history_size) {
		++ctx->length;
	}else {
		++ctx->start_pos;
		if(ctx->start_pos == ctx->history_size) ctx->start_pos = 0;
	}
	return 0;
}

int panel_ticker_append(struct panel_ticker_context * ctx, json_object * jticker)
{
	assert(ctx && jticker);
	struct coincheck_ticker ticker = { 0 };
	
	char sz_val[100] = "";
#define set_entry(key) do { \
		ticker.key = json_get_value(jticker, double, key); \
		snprintf(sz_val, sizeof(sz_val), "%s: %.2f", #key, ticker.key); \
		gtk_entry_set_text(GTK_ENTRY(ctx->widget.key), sz_val); \
	} while(0)
	
	set_entry(last);
	set_entry(ask);
	set_entry(bid);
	set_entry(high);
	set_entry(low);
	set_entry(volume);
#undef set_entry

	ctx->current = ticker;
	ticker_history_append(ctx, &ticker);
	return 0;
}




/*****************************************************
 * ui::init
*****************************************************/
static void init_bid_tree(GtkTreeView * tree)
{
	GtkListStore * store = NULL;
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
	g_object_unref(store);
	return;
}

static void init_ask_tree(GtkTreeView * tree)
{
	GtkListStore * store = NULL;
	GtkTreeViewColumn * col = NULL;
	GtkCellRenderer * cr = NULL;
	
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
	return;
}

static void on_row_activated_unsettled_tree(GtkTreeView * tree, GtkTreePath * path, GtkTreeViewColumn * col, panel_view_t * panel)
{
	if(NULL == path || NULL == col) return;
	GtkTreeModel * model = gtk_tree_view_get_model(tree);
	GtkTreeIter iter;
	if(NULL == model) return;
	
	gboolean ok = gtk_tree_model_get_iter(model, &iter, path);
	if(!ok) return;
	
	int64_t order_id = 0;
	gtk_tree_model_get(model, &iter, UNSETTLED_LIST_COLUMN_order_id, &order_id, -1);
	
	
	const char * colname = gtk_tree_view_column_get_title(col);
	if(NULL == colname || !colname[0]) {
		char sz_order_id[100] = "";
		snprintf(sz_order_id, sizeof(sz_order_id), "%" PRIi64, order_id);
		
		json_object * jresult = NULL;
		int rc = coincheck_cancel_order(panel->agent, sz_order_id, &jresult);
		if(0 == rc) {
			fprintf(stderr, "canceled order: %s\n", json_object_to_json_string_ext(jresult, JSON_C_TO_STRING_PRETTY));
			show_info(panel->shell, "cancel order: %ld", (long)json_object_to_json_string_ext(jresult, JSON_C_TO_STRING_SPACED));
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
			
			update_balance(panel);
		}
		
	}
}
static void on_refresh(GtkButton * button, panel_view_t * panel)
{
	update_orders_history(panel);
	update_balance(panel);
	return;
}

#define load_widget(field, widget_name) do { \
		panel->field =  GTK_WIDGET(gtk_builder_get_object(builder, #widget_name)); \
		assert(panel->field); \
	} while(0)
int panel_view_load_from_builder(panel_view_t * panel, GtkBuilder * builder)
{
	assert(panel && builder);
	load_widget(frame        , main_panel);
	load_widget(btc_balance  , btc_balance);
	load_widget(btc_in_use   , btc_in_use);
	load_widget(jpy_balance  , jpy_balance);
	load_widget(jpy_in_use   , jpy_in_use);
	load_widget(chart_ctx.da , da_chart);
	load_widget(ask_orders   , ask_orders);
	load_widget(bid_orders   , bid_orders);
	GtkTreeSelection * ask_orders_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(panel->ask_orders));
	GtkTreeSelection * bid_orders_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(panel->bid_orders));
	assert(ask_orders_selection && bid_orders_selection);
	g_signal_connect(ask_orders_selection, "changed", G_CALLBACK(on_ask_orders_selection_changed), panel);
	g_signal_connect(bid_orders_selection, "changed", G_CALLBACK(on_bid_orders_selection_changed), panel);
	
	
	load_widget(btc_buy         , btc_buy);
	load_widget(btc_buy_rate    , btc_buy_rate);
	load_widget(btc_buy_amount  , btc_buy_amount);
	load_widget(btc_sell        , btc_sell);
	load_widget(btc_sell_rate   , btc_sell_rate);
	load_widget(btc_sell_amount , btc_sell_amount);
	gtk_widget_set_sensitive(panel->btc_buy, FALSE);
	gtk_widget_set_sensitive(panel->btc_sell, FALSE);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(panel->btc_buy_amount), 0.0, 100.0);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(panel->btc_sell_amount), 0.0, 100.0);
	g_signal_connect(panel->btc_buy, "clicked", G_CALLBACK(coincheck_panel_btc_buy), panel);
	g_signal_connect(panel->btc_sell, "clicked", G_CALLBACK(coincheck_panel_btc_sell), panel);
	g_signal_connect(panel->btc_buy_rate, "activate", G_CALLBACK(coincheck_panel_buy_rate_changed), panel);
	g_signal_connect(panel->btc_buy_amount, "value-changed", G_CALLBACK(coincheck_panel_buy_amount_changed), panel);
	g_signal_connect(panel->btc_sell_rate, "activate", G_CALLBACK(coincheck_panel_sell_rate_changed), panel);
	g_signal_connect(panel->btc_sell_amount, "value-changed", G_CALLBACK(coincheck_panel_sell_amount_changed), panel);
	
	init_bid_tree(GTK_TREE_VIEW(panel->bid_orders));
	init_ask_tree(GTK_TREE_VIEW(panel->ask_orders));
	
	panel_ticker_load_from_builder(panel->ticker_ctx, builder);
	
	load_widget(orders_tree, orders_history);
	load_widget(unsettled_tree, unsettled_list);
	
	init_order_history_tree(GTK_TREE_VIEW(panel->orders_tree));
	init_unsettled_list_tree(GTK_TREE_VIEW(panel->unsettled_tree));
	
	GtkWidget * refresh = GTK_WIDGET(gtk_builder_get_object(builder, "refresh_orders_history"));
	assert(refresh);
	g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh), panel);
	
	//~ g_signal_connect(panel->unsettled_tree, "button-press-event", (GCallback)on_clicked_unsettled_tree, panel);
	g_signal_connect(panel->unsettled_tree, "row-activated", G_CALLBACK(on_row_activated_unsettled_tree), panel);
	

	return 0;
}

/*********************************************
 * public interfaces:
 * panel_view 
 * 
*********************************************/
panel_view_t * panel_view_init(panel_view_t * panel, const char * title, shell_context_t * shell)
{
	if(NULL == panel) panel = calloc(1, sizeof(*panel));
	assert(panel);
	
	pthread_mutex_init(&panel->mutex, NULL);
	
	panel->shell = shell;
	if(title) strncpy(panel->title, title, sizeof(panel->title));
	
	struct panel_ticker_context * ctx = panel_ticker_context_init(panel->ticker_ctx, 0);
	assert(ctx);
	
	order_history_init(panel->orders);
	return panel;
}

void panel_view_cleanup(panel_view_t * panel)
{
	if(NULL == panel) return;
	panel_ticker_context_cleanup(panel->ticker_ctx);
	return;
}

int coincheck_panel_init(trading_agency_t * agent, shell_context_t * shell)
{
	panel_view_t * panel = shell_get_main_panel(shell, "coincheck");
	assert(panel);
	
	assert(agent);
	panel->agent = agent;
	GtkWidget * da = panel->chart_ctx.da;
	assert(da);
	
	if(da) {
		g_signal_connect(da, "draw", G_CALLBACK(on_panel_view_da_draw), panel);
	}
	
	update_balance(panel);
	update_tickers(panel);
	update_orders_history(panel);
	
	// run backgound tasks
	g_timeout_add(1000, (GSourceFunc)update_tickers, panel);
	//~ g_timeout_add(3000, (GSourceFunc)update_orders_history, panel);
	
	return 0;
}
