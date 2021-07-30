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
int coincheck_get_order_history(trading_agency_t * agent, const struct coincheck_pagination_params * pagination, json_object ** p_jresponse);

/****************************************
 * coincheck private APIs
 * coincheck::Account
****************************************/
int coincheck_account_get_balance(trading_agency_t * agent, json_object ** p_jresponse);
//~ int coincheck_account_send_btc(trading_agency_t * agent, const char * address, const char * amount, json_object ** p_jresponse);
//~ int coincheck_account_get_sending_history(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
//~ int coincheck_account_get_deposit_history(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
int coincheck_account_get_info(trading_agency_t * agent, json_object ** p_jresponse);

/****************************************
 * coincheck private APIs
 * coincheck::Withdraw
****************************************/
int coincheck_get_bank_accounts(trading_agency_t * agent, json_object ** p_jresponse);
int coincheck_bank_account_add_json(trading_agency_t * agent, json_object * jbank_info, json_object ** p_jresponse);
int coincheck_bank_account_add(trading_agency_t * agent, 
	const char * bank_name, // 銀行名
	const char * branch_name, // 支店名
	const char * bank_account_type, // 銀行口座の種類（futsu : 普通口座, toza : 当座預金口座）
	const char * number, // 口座番号（例）"0123456"
	const char * name, // 口座名義
	json_object ** p_jresponse);
int coincheck_bank_account_remove(trading_agency_t * agent, 
	long bank_account_id, // 銀行口座のID
	json_object ** p_jresponse);
int coincheck_get_withdraws_history(trading_agency_t * agent, 
	const struct coincheck_pagination_params * pagination, 
	json_object ** p_jresponse);
int coincheck_withdraw_request(trading_agency_t * agent, 
	long bank_account_id, // 銀行口座のID
	const char * amount, //  金額
	const char * currency, // 通貨 ( 現在は "JPY" のみ対応)
	json_object ** p_jresponse);
int coincheck_withdraw_cancel(trading_agency_t * agent, 
	long widthdraw_id, // 出金申請のID
	json_object ** p_jresponse);

#ifdef __cplusplus
}
#endif
#endif
