#ifndef TRADING_AGENCY_COINCHECK_H_
#define TRADING_AGENCY_COINCHECK_H_

#include <stdio.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "trading_agency.h"

/****************************************************
 * API Documentation: 
 *   https://coincheck.com/documents/exchange/api
****************************************************/

/* Coincheck Authentication */
struct curl_slist * coincheck_auth_add_headers(struct curl_slist * headers, 
	const char * api_key, const char * api_secret, 
	const char * uri, 
	const char * body, ssize_t cb_body, /* post_data */
	const struct timespec * timestamp 	/* nonce */
);

enum coincheck_pagination_order
{
	coincheck_pagination_order_DESC,
	coincheck_pagination_order_ASC,
	coincheck_pagination_order_size
};
struct coincheck_pagination_params
{
	int64_t limit;
	enum coincheck_pagination_order order;
	const char * starting_after;
	const char * ending_before;
};

/****************************************
 * coincheck public APIs
****************************************/
int coincheck_public_get_ticker(trading_agency_t * agent, json_object ** p_jresponse);
int coincheck_public_get_trades(trading_agency_t * agent, const char * pair, const struct coincheck_pagination_params * pagination, json_object ** p_jresponse);
int coincheck_public_get_order_book(trading_agency_t * agent, json_object ** p_jresponse);
int coincheck_public_calc_rate(trading_agency_t * agent, const char * pair, const char * order_type, double price, double amount, json_object ** p_jresponse);
int coincheck_public_get_buy_rate(trading_agency_t * agent, const char * pair, json_object ** p_jresponse);

/****************************************
 * coincheck private APIs
 * coincheck::Order
****************************************/
#define COINCHECK_ORDER_BTC_AMOUNT_MIN (0.005)
int coincheck_new_order(trading_agency_t * agent, const char * pair, const char * order_type, const double rate, const double amount, json_object ** p_jresponse);
int coincheck_get_unsettled_order_list(trading_agency_t * agent, json_object ** p_jresponse);
int coincheck_cancel_order(trading_agency_t * agent, const char * order_id, json_object ** p_jresponse);
int coincheck_get_cancellation_status(trading_agency_t * agent, const char * order_id, json_object ** p_jresponse);
int coincheck_get_order_history(trading_agency_t * agent, struct coincheck_pagination_params * pagination, json_object ** p_jresponse);

/****************************************
 * coincheck private APIs
 * coincheck::Account
****************************************/
int coincheck_account_get_balance(trading_agency_t * agent, json_object ** p_jresponse);
//~ int coincheck_account_send_btc(trading_agency_t * agent, const char * address, const char * amount, json_object ** p_jresponse);
//~ int coincheck_account_get_sending_history(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
//~ int coincheck_account_get_deposit_history(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
int coincheck_account_get_info(trading_agency_t * agent, json_object ** p_jresponse);

#ifdef __cplusplus
}
#endif
#endif
