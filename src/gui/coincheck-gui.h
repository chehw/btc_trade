#ifndef BTC_TRADER_COINCHECK_GUI_H_
#define BTC_TRADER_COINCHECK_GUI_H_

#include <gtk/gtk.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "shell.h"
#include "trading_agency.h"

#include <pthread.h>


struct order_book_data
{
	const char * rate;
	const char * amount;
	double d_amount;
	double scales;	// range: [0, 100]
};
	
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
	GtkWidget * da_chart;
	
	GtkWidget * ask_orders;
	GtkWidget * bid_orders;
	
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
