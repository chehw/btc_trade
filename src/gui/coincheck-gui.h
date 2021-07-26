#ifndef BTC_TRADER_COINCHECK_GUI_H_
#define BTC_TRADER_COINCHECK_GUI_H_

#include <gtk/gtk.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "shell.h"
#include "trading_agency.h"

#include <pthread.h>
#include <time.h>


struct order_book_data
{
	const char * rate;
	const char * amount;
	double d_amount;
	double scales;	// range: [0, 100]
};

struct coincheck_ticker
{
	double last;
	double bid;
	double ask;
	double high;
	double low;
	double volume;
	int64_t timestamp;
};

struct panel_ticker_context
{
	struct {
		GtkWidget * last;
		GtkWidget * bid;
		GtkWidget * ask;
		GtkWidget * high;
		GtkWidget * low;
		GtkWidget * volume;
	} widget;
	
	struct coincheck_ticker current;
	
	size_t history_size;	// window size
	size_t length;
	size_t start_pos;
	struct coincheck_ticker * tickers_history; // Circular array
};
struct panel_ticker_context * panel_ticker_context_init(struct panel_ticker_context * ctx, size_t max_history_size);
void panel_ticker_context_cleanup(struct panel_ticker_context * ctx);
int panel_ticker_load_from_builder(struct panel_ticker_context * ctx, GtkBuilder * builder);
int panel_ticker_append(struct panel_ticker_context * ctx, json_object * jticker);
ssize_t panel_ticker_get_lastest_history(struct panel_ticker_context * ctx, size_t count, struct coincheck_ticker ** p_tickers);

typedef struct panel_view
{
	void * user_data;
	shell_context_t * shell;
	
	char title[100];
	
	GtkWidget * frame;
	GtkWidget * btc_balance;
	GtkWidget * btc_in_use;
	GtkWidget * jpy_balance;
	GtkWidget * jpy_in_use;
	
	struct {
		GtkWidget * da;
		cairo_surface_t * surface;
		int image_width;
		int image_height;
		int da_width;
		int da_height;
	}chart_ctx;
	
	GtkWidget * ask_orders;
	GtkWidget * bid_orders;
	
	GtkWidget * btc_buy;
	GtkWidget * btc_buy_rate;
	GtkWidget * btc_buy_amount;
	GtkWidget * btc_sell;
	GtkWidget * btc_sell_rate;
	GtkWidget * btc_sell_amount;
	
	struct timespec last_query_time;
	GtkWidget * trade_history;
	
	struct panel_ticker_context ticker_ctx[1];
	
	trading_agency_t * agent;
	pthread_mutex_t mutex;
	
	int num_asks;
	int num_bids;
	struct order_book_data * asks_list;
	struct order_book_data * bids_list;
	
}panel_view_t;

panel_view_t * panel_view_init(panel_view_t * panel, const char * title, shell_context_t * shell);
void panel_view_cleanup(panel_view_t * panel);
int panel_view_load_from_builder(panel_view_t * panel, GtkBuilder * builder);

extern panel_view_t * shell_get_main_panel(shell_context_t * shell, const char * agency_name);
int coincheck_panel_init(trading_agency_t * coincheck, shell_context_t * shell);
int coincheck_panel_update_order_book(panel_view_t * panel);

#ifdef __cplusplus
}
#endif
#endif
