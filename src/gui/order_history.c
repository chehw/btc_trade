/*
 * order_history.c
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
#include <stdint.h>
#include <json-c/json.h>
#include <time.h>

#include "order_history.h"
#include "utils.h"
#include "trading_agency_coincheck.h"



/*********************************************************
 * get_order_history

coincheck-cli order_history
{ 
"success": true, 
"transactions": [ 
  { 
    "id": <int64>, 
    "order_id": <(int64)>, 
    "created_at": "2021-07-25T14:22:02.000Z", 
    "funds": { "btc": "0.01", "jpy": "-37500.0" }, 
    "pair": "btc_jpy", 
    "rate": "3750000.0", 
    "fee_currency": null, 
    "fee": "0.0", 
    "liquidity": "M", 
    "side": "buy" 
  }, 
  ...
]}
**********************************************************/
static json_object * get_order_history(trading_agency_t * agent)
{
	int rc = 0;
	json_bool ok = FALSE;
	json_object * jresult = NULL;
	json_object * jstatus = NULL;
	json_object * jorders = NULL;
	rc = coincheck_get_order_history(agent, NULL, &jresult);
	if(rc || NULL == jresult) {
		if(jresult) json_object_put(jresult);
		return NULL;
	}
	
	ok = json_object_object_get_ex(jresult, "success", &jstatus);
	assert(ok && jstatus);
	if(!ok || NULL == jstatus) return NULL;
	if(!json_object_get_boolean(jstatus)) return NULL;
	
	ok = json_object_object_get_ex(jresult, "transactions", &jorders);
	if(!ok || NULL == jorders) return NULL;
	
	return jorders;
}

static int order_details_compare(const void * pa, const void * pb)
{
	const struct order_details * a = *(const void **)pa;
	const struct order_details * b = *(const void **)pb;
	
	// order by order_id DESC
	if(a->order_id > b->order_id) return -1;
	if(a->order_id < b->order_id) return 1;
	
	// order by timestamp DESC
	if(a->created_at > b->created_at) return -1;
	if(a->created_at < b->created_at) return 1;
	
	return 0;
}

static int parse_json_orders(struct order_history * history, json_object * jorders)
{
	assert(history);
	if(NULL == jorders) return -1;
	json_bool ok = FALSE;
	history->jorders = json_object_get(jorders);
	
	struct order_details ** orders = NULL;
	int num_orders = json_object_array_length(jorders);
	if(num_orders <= 0) return 0;
	
	orders = calloc(num_orders, sizeof(*orders));
	assert(orders);
	
	for(int i = 0; i < num_orders; ++i) {
		json_object * jorder = json_object_array_get_idx(jorders, i);
		assert(jorder);
		if(NULL == jorder) continue;
		struct order_details * order = calloc(1, sizeof(*order));
		assert(order);
		order->id = json_get_value(jorder, int64, id);
		order->order_id = json_get_value(jorder, int64, order_id);
		
		json_object * jfunds = NULL;
		ok = json_object_object_get_ex(jorder, "funds", &jfunds);
		assert(ok && jfunds);
		order->btc = json_get_value(jfunds, string, btc);
		order->jpy = json_get_value(jfunds, string, jpy);
		
		order->rate = json_get_value(jorder, string, rate);
		order->liquidity = json_get_value(jorder, string, liquidity);
		order->side = json_get_value(jorder, string, side);
		
		struct tm t = { 0 };
		const char * created_at = json_get_value(jorder, string, created_at);
		assert(created_at);
		strptime(created_at, "%Y-%m-%dT%H:%M:%S.000Z", &t);
		order->sz_created_at = created_at;	
		order->created_at = mktime(&t) - timezone; // gmt to localtime
	
		orders[i] = order;
	}
	
	qsort(orders, num_orders, sizeof(*orders), order_details_compare);
	history->orders = orders;
	history->num_orders = num_orders;
	return 0;
}



/****************************************************
 * get_unsettled_orders:
$ coincheck-cli list_unsettled_orders
{ 
  "success": true, 
  "orders": [ 
  { 
    "id": <int64>, 
    "order_type": "buy", 
    "rate": "3720000.0", 
    "pair": "btc_jpy", 
    "pending_amount": "0.01",
    "pending_market_buy_amount": null, 
    "stop_loss_rate": null, 
    "created_at": "2021-07-25T16:45:03.000Z" } 
  ] 
}
*********************************************************/
static json_object * get_unsettled_orders(trading_agency_t * agent)
{
	int rc = 0;
	json_bool ok = FALSE;
	json_object * jresult = NULL;
	json_object * jstatus = NULL;
	json_object * jorders = NULL;
	rc = coincheck_get_unsettled_order_list(agent, &jresult);
	if(rc || NULL == jresult) {
		if(jresult) json_object_put(jresult);
		return NULL;
	}

	ok = json_object_object_get_ex(jresult, "success", &jstatus);
	assert(ok && jstatus);
	if(!ok || NULL == jstatus) return NULL;
	if(!json_object_get_boolean(jstatus)) return NULL;
	
	ok = json_object_object_get_ex(jresult, "orders", &jorders);
	if(!ok || NULL == jorders) return NULL;
	
	return jorders;
}

static int unsettled_order_details_compare(const void * pa, const void * pb)
{
	const struct unsettled_order_details * a = *(const void **)pa;
	const struct unsettled_order_details * b = *(const void **)pb;
	
	// order by order_id DESC
	if(a->order_id > b->order_id) return -1;
	if(a->order_id < b->order_id) return 1;
	
	// order by timestamp DESC
	if(a->created_at > b->created_at) return -1;
	if(a->created_at < b->created_at) return 1;
	
	return 0;
}

static int parse_json_unsettled_orders(struct order_history * history, json_object * junsettled_orders)
{
	assert(history);
	if(NULL == junsettled_orders) return -1;
	history->junsettled_orders = json_object_get(junsettled_orders);
	
	struct unsettled_order_details ** unsettled_orders = NULL;
	int num_orders = json_object_array_length(junsettled_orders);
	if(num_orders <= 0) return 0;
	
	unsettled_orders = calloc(num_orders, sizeof(*unsettled_orders));
	assert(unsettled_orders);
	
	fprintf(stderr, "unsettled_orders: %s\n",
		json_object_to_json_string_ext(junsettled_orders, JSON_C_TO_STRING_PRETTY));
	
	for(int i = 0; i < num_orders; ++i) {
		json_object * jorder = json_object_array_get_idx(junsettled_orders, i);
		assert(jorder);
		if(NULL == jorder) continue;
		struct unsettled_order_details * order = calloc(1, sizeof(*order));
		assert(order);
		order->order_id = json_get_value(jorder, int64, id);
		order->order_type = json_get_value(jorder, string, order_type);
		order->rate = json_get_value(jorder, string, rate);
		order->pending_amount = json_get_value(jorder, string, pending_amount);
	
		struct tm t = { 0 };
		const char * created_at = json_get_value(jorder, string, created_at);
		assert(created_at);
		strptime(created_at, "%Y-%m-%dT%H:%M:%S.000Z", &t);
		order->sz_created_at = created_at;
		order->created_at = mktime(&t) - timezone; // gmt to localtime
	
		unsettled_orders[i] = order;
	}
	
	qsort(unsettled_orders, num_orders, sizeof(*unsettled_orders), unsettled_order_details_compare);
	history->unsettled_orders = unsettled_orders;
	history->num_unsettled_orders = num_orders;
	return 0;
}

/*********************************************
 * struct order_history 
 * public interfaces
*********************************************/

static void clear_orders_list(struct order_history * history)
{
	if(NULL == history) return;
	if(history->orders) {
		for(int i = 0; i < history->num_orders; ++i) {
			free(history->orders[i]);
			history->orders[i] = NULL;
		}
		history->orders = NULL;
	}
	history->num_orders = 0;
	
	if(history->jorders) {
		json_object_put(history->jorders);
		history->jorders = NULL;
	}
	return;
}

static void clear_unsettled_order_list(struct order_history * history)
{
	if(NULL == history) return;
	if(history->unsettled_orders) {
		for(int i = 0; i < history->num_unsettled_orders; ++i) {
			free(history->unsettled_orders[i]);
			history->unsettled_orders[i] = NULL;
		}
		history->unsettled_orders = NULL;
	}
	history->num_unsettled_orders = 0;
	
	if(history->junsettled_orders) {
		json_object_put(history->junsettled_orders);
		history->junsettled_orders = NULL;
	}
	return;
}

static int order_history_get_orders(struct order_history * history, trading_agency_t * agent)
{
	int rc = 0;
	json_object * jorders = NULL;
	json_object * junsettled_orders = NULL;
	
	clear_orders_list(history);
	jorders = get_order_history(agent);
	parse_json_orders(history, jorders);
	

	clear_unsettled_order_list(history);
	junsettled_orders = get_unsettled_orders(agent);
	parse_json_unsettled_orders(history, junsettled_orders);
	
	return rc;
}

struct order_history * order_history_init(struct order_history * history)
{
	if(NULL == history) history = calloc(1, sizeof(*history));
	else memset(history, 0, sizeof(*history));
	
	assert(history);
	history->get_orders = order_history_get_orders;
	return history;
}
void order_history_cleanup(struct order_history * history)
{
	if(NULL == history) return;
	clear_orders_list(history);
	clear_unsettled_order_list(history);
	return;
}

/********************************
 * ui::module
********************************/
#include <gtk/gtk.h>

static const char * s_order_history_colnames[] = {
	[ORDER_HISTORY_COLUMN_order_id] = "order_id",
	[ORDER_HISTORY_COLUMN_funds_btc] = "btc",
	[ORDER_HISTORY_COLUMN_funds_jpy] = "jpy",
	[ORDER_HISTORY_COLUMN_rate] = "rate",
	[ORDER_HISTORY_COLUMN_side] = "side",
	[ORDER_HISTORY_COLUMN_liquidity] = "liquidity",
	[ORDER_HISTORY_COLUMN_created_at] = "timestamp",
	[ORDER_HISTORY_COLUMN_data_ptr] = "(data_ptr)",
};
const char * order_history_colname(enum ORDER_HISTORY_COLUMN col_id)
{
	assert(col_id >= 0 && col_id < ORDER_HISTORY_COLUMNS_COUNT);
	return s_order_history_colnames[col_id];
}

static void set_details_or_order_id(GtkTreeViewColumn *col, 
	GtkCellRenderer *cr,
	GtkTreeModel *model,
	GtkTreeIter *iter,
	gpointer user_data)
{
	void * data_ptr = NULL;
	char * side = NULL;
	
	gint col_id = GPOINTER_TO_INT(user_data);
	gtk_tree_model_get(model, iter, 
		ORDER_HISTORY_COLUMN_side, &side,
		ORDER_HISTORY_COLUMN_data_ptr, &data_ptr, -1);
	if(data_ptr) {
		if(col_id != ORDER_HISTORY_COLUMN_rate) return;
		
		char text[100] = "";
		snprintf(text, sizeof(text), "id: %lu", (unsigned long)((struct order_details *)data_ptr)->order_id);
		g_object_set(cr, 
			"text", text, 
			"foreground", "black",
			NULL);
	}else {
		char * text = NULL;
		assert(col_id != ORDER_HISTORY_COLUMN_order_id && col_id < ORDER_HISTORY_COLUMNS_COUNT);
		gtk_tree_model_get(model, iter, col_id, &text, -1);
		
		if(NULL == text) text = "";
		int is_buy_order = (strcasecmp(side, "buy") == 0);
		
		g_object_set(cr, 
			"text", text, 
			"foreground", (is_buy_order?"green":"red"),
			NULL);
	}
	return;
}

static inline GtkTreeStore * new_order_history_store(void)
{
	GtkTreeStore * store = gtk_tree_store_new(ORDER_HISTORY_COLUMNS_COUNT, 
		G_TYPE_INT64, 	// order_id
		G_TYPE_STRING, 	// btc
		G_TYPE_STRING, 	// jpy
		G_TYPE_STRING, 	// rate
		G_TYPE_STRING, 	// side
		G_TYPE_STRING,	// liquidity
		G_TYPE_STRING,	// created_at
		G_TYPE_POINTER 	// data_ptr
	);
	return store;
}

void init_order_history_tree(GtkTreeView * tree)
{
	GtkTreeViewColumn * col = NULL;
	GtkCellRenderer * cr = NULL;
	
	gtk_tree_view_set_grid_lines(tree, GTK_TREE_VIEW_GRID_LINES_BOTH);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("rate", cr, "text", ORDER_HISTORY_COLUMN_rate, NULL); 
	g_object_set(cr, "xalign", 0.95, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_column_set_cell_data_func(col, cr, set_details_or_order_id, GINT_TO_POINTER(ORDER_HISTORY_COLUMN_rate), NULL);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("side", cr, "text", ORDER_HISTORY_COLUMN_side, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_column_set_cell_data_func(col, cr, set_details_or_order_id, GINT_TO_POINTER(ORDER_HISTORY_COLUMN_side), NULL);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("btc", cr, "text", ORDER_HISTORY_COLUMN_funds_btc, NULL); 
	g_object_set(cr, "xalign", 0.95, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("jpy", cr, "text", ORDER_HISTORY_COLUMN_funds_jpy, NULL);
	g_object_set(cr, "xalign", 0.95, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("timestamp", cr, "text", ORDER_HISTORY_COLUMN_created_at, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	gtk_tree_view_column_set_expand(col, TRUE);
	
	GtkTreeStore * store = new_order_history_store();
	gtk_tree_view_set_model(tree, GTK_TREE_MODEL(store));
	g_object_unref(store);
	return;
}


//~ enum UNSETTLED_LIST_COLUMN
//~ {
	//~ UNSETTLED_LIST_COLUMN_order_id,
	//~ UNSETTLED_LIST_COLUMN_order_type,
	//~ UNSETTLED_LIST_COLUMN_rate,
	//~ UNSETTLED_LIST_COLUMN_pending_amount,
	//~ UNSETTLED_LIST_COLUMN_created_at,
	//~ UNSETTLED_LIST_COLUMN_data_ptr,
	//~ UNSETTLED_LIST_COLUMNS_COUNT
//~ };

static inline GtkListStore * new_unsettled_orders_store(void)
{
	GtkListStore * store = gtk_list_store_new(UNSETTLED_LIST_COLUMNS_COUNT, 
		G_TYPE_INT64, 	// order_id
		G_TYPE_STRING, 	// order_type
		G_TYPE_STRING, 	// rate
		G_TYPE_STRING, 	// pending_amount
		G_TYPE_STRING,	// created_at
		G_TYPE_POINTER, 	// data_ptr
		G_TYPE_STRING	// icon-name of the cancel_button
	);
	return store;
}

void init_unsettled_list_tree(GtkTreeView * tree)
{
	GtkTreeViewColumn * col = NULL;
	GtkCellRenderer * cr = NULL;
	
	gtk_tree_view_set_grid_lines(tree, GTK_TREE_VIEW_GRID_LINES_VERTICAL);
	
	cr = gtk_cell_renderer_pixbuf_new();
	g_object_set(cr, "icon-name", "window-close", NULL);
	col = gtk_tree_view_column_new_with_attributes("", cr, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("type", cr, "text", UNSETTLED_LIST_COLUMN_order_type, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	g_object_set(cr, "xalign", 0.95, NULL);
	col = gtk_tree_view_column_new_with_attributes("rate", cr, "text", UNSETTLED_LIST_COLUMN_rate, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	g_object_set(cr, "xalign", 0.95, NULL);
	col = gtk_tree_view_column_new_with_attributes("amount", cr, "text", UNSETTLED_LIST_COLUMN_pending_amount, NULL); 
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("order id", cr, "text", ORDER_HISTORY_COLUMN_order_id, NULL);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	
	cr = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("timestamp", cr, "text", UNSETTLED_LIST_COLUMN_created_at, NULL); 
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_append_column(tree, col);
	gtk_tree_view_column_set_expand(col, TRUE);
	
	
	GtkListStore * store = new_unsettled_orders_store();
	gtk_tree_view_set_model(tree, GTK_TREE_MODEL(store));
	g_object_unref(store);
	
	gtk_tree_view_set_activate_on_single_click(tree, TRUE);
	
	return;
}


void order_history_update(struct order_history * history, GtkTreeView * orders_tree, GtkTreeView * unsettled_tree)
{
	if(orders_tree) {
		GtkTreeStore * store = new_order_history_store();
		GtkTreeIter parent, iter;
		
		int64_t prev_order_id = -1;
		for(int i = 0; i < history->num_orders; ++i) {
			struct order_details * order = history->orders[i];
			assert(order);
			if(prev_order_id != order->order_id) {
				prev_order_id = order->order_id;
				
				gtk_tree_store_append(store, &parent, NULL);
				gtk_tree_store_set(store, &parent, 
					ORDER_HISTORY_COLUMN_data_ptr, order, 
					-1);

			}
			
			/* convert gmtime to localtime */
			char timestamp[100] = "";
			struct tm t[1];
			memset(t, 0, sizeof(t));
			localtime_r(&order->created_at, t);
			strftime(timestamp, sizeof(timestamp), "%Y/%m/%d %H:%M:%S %Z", t);
			
			gtk_tree_store_append(store, &iter, &parent);
			gtk_tree_store_set(store, &iter, 
				ORDER_HISTORY_COLUMN_order_id, order->order_id,
				ORDER_HISTORY_COLUMN_created_at, timestamp,
				ORDER_HISTORY_COLUMN_funds_btc, order->btc,
				ORDER_HISTORY_COLUMN_funds_jpy, order->jpy, 
				ORDER_HISTORY_COLUMN_side, order->side,
				ORDER_HISTORY_COLUMN_rate, order->rate,
				ORDER_HISTORY_COLUMN_liquidity, order->liquidity,
				-1);
		}
		gtk_tree_view_set_model(orders_tree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		
		gtk_tree_view_expand_all(orders_tree);
	}
	
	if(unsettled_tree) {
		GtkListStore * store = new_unsettled_orders_store();
		GtkTreeIter iter;
		
		for(int i = 0; i < history->num_unsettled_orders; ++i) {
			struct unsettled_order_details * unsettled = history->unsettled_orders[i];
			assert(unsettled);
			
			char timestamp[100] = "";
			struct tm t[1];
			memset(t, 0, sizeof(t));
			localtime_r(&unsettled->created_at, t);
			strftime(timestamp, sizeof(timestamp), "%Y/%m/%d %H:%M:%S %Z", t);
			
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
				UNSETTLED_LIST_COLUMN_order_type, unsettled->order_type,
				UNSETTLED_LIST_COLUMN_order_id, unsettled->order_id,
				UNSETTLED_LIST_COLUMN_rate, unsettled->rate,
				UNSETTLED_LIST_COLUMN_pending_amount, unsettled->pending_amount,
				UNSETTLED_LIST_COLUMN_created_at, (i==0)?timestamp:unsettled->sz_created_at,
				UNSETTLED_LIST_COLUMN_data_ptr, unsettled,
				-1);
		}
		
		gtk_tree_view_set_model(unsettled_tree, GTK_TREE_MODEL(store));
		g_object_unref(store);
	}
	return;
}






