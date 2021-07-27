#ifndef BTC_TRADER_ORDER_HISTORY_H_
#define BTC_TRADER_ORDER_HISTORY_H_

#include <stdio.h>
#include <stdint.h>
#include <json-c/json.h>
#include <time.h>
#include "trading_agency.h"

#ifdef __cplusplus
extern "C" {
#endif

struct unsettled_order_details
{
	uint64_t order_id;
	const char * order_type;
	const char * rate;
	const char * pending_amount;
	const char * sz_created_at;
	time_t created_at;
};

struct order_details
{
	uint64_t id;
	uint64_t order_id;
	const char * btc; 	// funds.btc
	const char * jpy; 	// funds.jpy
	const char * rate;
	const char * side;	// order_type: "buy" or "sell"
	const char * liquidity; 	// "M" (maker) or "T" (taker)
	const char * sz_created_at;
	time_t created_at;
};

struct order_history
{
	json_object * jorders;
	json_object * junsettled_orders;
	
	int num_orders;
	struct order_details ** orders;
	
	int num_unsettled_orders;
	struct unsettled_order_details ** unsettled_orders;
	
	int (* get_orders)(struct order_history * history, trading_agency_t * agent);
	
};
struct order_history * order_history_init(struct order_history * history);
void order_history_cleanup(struct order_history * history);


/********************************
 * ui::module
********************************/
#include <gtk/gtk.h>
enum UNSETTLED_LIST_COLUMN
{
	UNSETTLED_LIST_COLUMN_order_id,
	UNSETTLED_LIST_COLUMN_order_type,
	UNSETTLED_LIST_COLUMN_rate,
	UNSETTLED_LIST_COLUMN_pending_amount,
	UNSETTLED_LIST_COLUMN_created_at,
	UNSETTLED_LIST_COLUMN_data_ptr,
	ORDER_HISTORY_COLUMN_cancel_button,
	UNSETTLED_LIST_COLUMNS_COUNT
};

enum ORDER_HISTORY_COLUMN
{
	ORDER_HISTORY_COLUMN_order_id,
	ORDER_HISTORY_COLUMN_funds_btc,
	ORDER_HISTORY_COLUMN_funds_jpy,
	ORDER_HISTORY_COLUMN_rate,
	ORDER_HISTORY_COLUMN_side,
	ORDER_HISTORY_COLUMN_liquidity,
	ORDER_HISTORY_COLUMN_created_at,
	ORDER_HISTORY_COLUMN_data_ptr,
	ORDER_HISTORY_COLUMNS_COUNT
};

void init_order_history_tree(GtkTreeView * tree);
void init_unsettled_list_tree(GtkTreeView * tree);
void order_history_update(struct order_history * history, GtkTreeView * orders_tree, GtkTreeView * unsettled_tree);

#ifdef __cplusplus
}
#endif
#endif
