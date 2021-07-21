/*
 * coincheck.c
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

#include <limits.h>

#include <json-c/json.h>
#include <curl/curl.h>
#include "auto_buffer.h"

#include "crypto/hmac.h"
#include "utils.h"

#include "trading_agency.h"
#include "trading_agency_coincheck.h"

#include "json-response.h"

static const char * s_sz_pagination_order[coincheck_pagination_order_size] = {
	[coincheck_pagination_order_DESC] = "desc",
	[coincheck_pagination_order_ASC] = "asc",
};

/**
 * coincheck_auth_add_headers()
 */
struct curl_slist * coincheck_auth_add_headers(
	struct curl_slist * headers, 
	const char * api_key, const char * api_secret, 
	const char * uri, const char * body, ssize_t cb_body,
	const struct timespec * timestamp)
{
	assert(api_key && api_secret);
	struct timespec ts[1];
	if(NULL == timestamp) {
		memset(ts, 0, sizeof(ts));
		clock_gettime(CLOCK_REALTIME, ts);
		timestamp = ts;
	}
	assert(timestamp);
	
	hmac_sha256_t hmac[1];
	unsigned char hash[32] = { 0 };
	
	auto_buffer_t message[1];
	memset(message, 0, sizeof(message));
	auto_buffer_init(message, 0);
	
	char * signature = NULL;
	int64_t nonce_ms = (int64_t)timestamp->tv_sec * 1000 + (timestamp->tv_nsec / 1000000) % 1000;
	char sz_nonce[100] = "";
	int cb_nonce = snprintf(sz_nonce, sizeof(sz_nonce), "%lu", (unsigned long)nonce_ms);
	assert(cb_nonce > 0);
	
	auto_buffer_push(message, sz_nonce, cb_nonce);
	auto_buffer_push(message, uri, strlen(uri));
	if(body) {
		if(cb_body == -1 || cb_body == 0) cb_body = strlen(body);
		if(cb_body > 0) auto_buffer_push(message, body, cb_body);
	}
	
	hmac_sha256_init(hmac, (unsigned char *)api_secret, strlen(api_secret));
	hmac_sha256_update(hmac, (unsigned char *)message->data, message->length);
	hmac_sha256_final(hmac, hash);
	bin2hex(hash, sizeof(hash), &signature);
	
	char line[1024] = "";
	snprintf(line, sizeof(line), "ACCESS-KEY: %s", api_key);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	snprintf(line, sizeof(line), "ACCESS-NONCE: %lu", (unsigned long)nonce_ms);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	snprintf(line, sizeof(line), "ACCESS-SIGNATURE: %s", signature);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	//~ debug_printf("uri: %s\n"
		//~ "api-key: %s\n"
		//~ "secret: %s\n"
		//~ "nounce: %lu\n" 
		//~ "signature: %s\n",
		//~ uri, api_key, "********" /*api_secret */, 
		//~ (unsigned long)nounce_ms,
		//~ signature);
	
	free(signature);
	auto_buffer_cleanup(message);
	return headers;
}

/****************************************
 * coincheck public APIs
****************************************/
int coincheck_public_get_ticker(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * end_point = "api/ticker";
	assert(agent);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;

	char url[PATH_MAX] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point); 
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Public trades
 * You can get current order transactions.
 * HTTP REQUEST
 * GET /api/trades
 * PARAMETERS
 *  pair Specify a currency pair to trade. "btc_jpy", "etc_jpy", "fct_jpy" and "mona_jpy" are now available.
 */
int coincheck_public_get_trades(trading_agency_t * agent, const char * pair, const struct coincheck_pagination_params * pagination, json_object ** p_jresponse)
{
	static const char * end_point = "api/trades";
	assert(agent && pair);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;

	char url[PATH_MAX] = "";
	char *p = url;
	char *p_end = p + sizeof(url);
	int cb = 0;
	cb = snprintf(p, p_end - p, "%s/%s?pair=%s", agent->base_url, end_point, pair); 
	assert(cb > 0);
	p += cb;
	if(pagination) {
		cb = snprintf(p, p_end - p, "&limit=%ld&order=%s", 
			(long)pagination->limit,
			s_sz_pagination_order[pagination->order]);
		assert(cb > 0);
		p += cb;
		
		if(pagination->starting_after) {
			cb = snprintf(p, p_end - p, "&starting_after=%s", pagination->starting_after);
			assert(cb > 0);
			p += cb;
		}
		
		if(pagination->ending_before) {
			cb = snprintf(p, p_end - p, "&ending_before=%s", pagination->ending_before);
			assert(cb > 0);
			p += cb;
		}
	}
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}
/**
 * Order Book
 * Fetch order book information.
 * HTTP REQUEST
 * GET /api/order_books
**/
int coincheck_public_get_order_book(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * end_point = "api/order_books";
	assert(agent);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;

	char url[PATH_MAX] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point); 
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Calc Rate
 * To calculate the rate from the order of the exchange.
 * HTTP REQUEST
 * GET /api/exchange/orders/rate
 * PARAMETERS
 * *order_type Order type（"sell" or "buy"）
 * *pair Specify a currency pair to trade. "btc_jpy", "etc_jpy", "fct_jpy" and "mona_jpy" are now available.
 * amount Order amount (ex) 0.1
 * price Order price (ex) 28000
 * ※ Either price or amount must be specified as a parameter.
*/
int coincheck_public_calc_rate(trading_agency_t * agent, const char * pair, const char * order_type, double price, double amount, json_object ** p_jresponse)
{
	static const char * end_point = "api/exchange/orders/rate";
	assert(agent);
	assert(pair && order_type);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;

	char url[PATH_MAX] = "";
	char * p = url;
	char * p_end = p + sizeof(url);
	int cb = 0;
	cb = snprintf(url, sizeof(url), "%s/%s?pair=%s&order_type=%s", agent->base_url, end_point, pair, order_type); 
	assert(cb > 0);
	p += cb;
	
	if(price >= 0.000001) {
		cb = snprintf(p, p_end - p, "&price=%f", price);
		assert(cb > 0);
		p += cb;
	}
	if(amount >= 0.000001) {
		cb = snprintf(p, p_end - p, "&amount=%f", amount);
		assert(cb > 0);
		p += cb;
	}
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Buy Rate
 * Rate of Buy Coin.
 * HTTP REQUEST
 * GET /api/rate/[pair]
 * PARAMETERS
 * *pair pair (e.g. "btc_jpy" )
**/
int coincheck_public_get_buy_rate(trading_agency_t * agent, const char * pair, json_object ** p_jresponse)
{
	static const char * end_point = "api/rate";
	assert(agent && pair);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;

	char url[PATH_MAX] = "";
	int cb = 0;
	cb = snprintf(url, sizeof(url), "%s/%s/%s", agent->base_url, end_point, pair); 
	assert(cb > 0);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}


/****************************************
 * coincheck private APIs
 * coincheck::Order
****************************************/
//~ #define COINCHECK_ORDER_BTC_AMOUNT_MIN (0.005)
int coincheck_new_order(trading_agency_t * agent, const char * pair, const char * order_type, const double rate, const double amount, json_object ** p_jresponse)
{
	static const char * end_point = "api/exchange/orders";
	int rc = 0;
	
	assert(agent && agent->priv && pair && order_type);
	assert(amount >= COINCHECK_ORDER_BTC_AMOUNT_MIN);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_trade, &api_key, &api_secret);
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);

	char request_body[4096] = "";
	char * p = request_body;
	char * p_end = p + sizeof(request_body);
	int cb_body = 0;
	
	if(strcasecmp(order_type, "buy") == 0 || strcasecmp(order_type, "sell") == 0) {
		cb_body = snprintf(p, p_end - p, 
			"pair=%s&order_type=%s&rate=%f&amount=%f",
			pair, order_type, 
			rate, amount
		);
	}else if(strcasecmp(order_type, "market_buy") == 0) {
		cb_body = snprintf(p, p_end - p, 
			"pair=%s&order_type=%s&market_buy_amount=%f",
			pair, order_type, 
			rate
		);
	}else if(strcasecmp(order_type, "market_sell") == 0) {
		cb_body = snprintf(p, p_end - p, 
			"pair=%s&order_type=%s&amount=%f",
			pair, order_type, 
			amount
		);
	}
	assert(cb_body > 0);
	debug_printf("post_fields: %s", request_body);
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, 
		request_body, cb_body, 
		NULL);
	
	jresponse = http->post(http, url, request_body, cb_body);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Unsettled order list
 * You can get a unsettled order list.
 * HTTP REQUEST
 * GET /api/exchange/orders/opens
*/
int coincheck_get_unsettled_order_list(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * end_point = "api/exchange/orders/opens";
	int rc = 0;
	assert(agent);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_query, &api_key, &api_secret); // use query_key (the principle of least privilege )
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, 
		NULL, 0, 
		NULL);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Cancel Order
 * New OrderOr, you can cancel by specifyingunsettle order list's ID.
 * HTTP REQUEST
 * DELETE /api/exchange/orders/[id]
 * @param
 * 	id New order or Unsettle order list's ID
*/
int coincheck_cancel_order(trading_agency_t * agent, const char * order_id, json_object ** p_jresponse)
{
	static const char * end_point = "api/exchange/orders";
	int rc = 0;
	assert(agent && order_id);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_trade, &api_key, &api_secret);
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s/%s", agent->base_url, end_point, order_id);
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, NULL, 0, 
		NULL);
	
	jresponse = http->delete(http, url, NULL, 0);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Order cancellation status
 * You can refer to the cancellation processing status of the order.
 * HTTP REQUEST
 * GET /api/exchange/orders/cancel_status?id=[id]
 * @param
 * 	order_id : (id) New Order Or, you can cancel by specifyingunsettle order list's ID.
**/
int coincheck_get_cancellation_status(trading_agency_t * agent, const char * order_id, json_object ** p_jresponse)
{
	static const char * end_point = "api/exchange/orders/cancel_status";
	int rc = 0;
	assert(agent && order_id);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_query, &api_key, &api_secret); // use query_key (the principle of least privilege )
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s?id=%s", agent->base_url, end_point, order_id);
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, NULL, 0, 
		NULL);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * Transaction history
 * Display your transaction history
 * HTTP REQUEST
 * GET /api/exchange/orders/transactions
**/
int coincheck_get_order_history(trading_agency_t * agent, struct coincheck_pagination_params * pagination, json_object ** p_jresponse)
{
	static const char * end_point = "api/exchange/orders/transactions";
	int rc = 0;
	assert(agent);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_query, &api_key, &api_secret);	// use query_key (the principle of least privilege )
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	char * p = url;
	char * p_end = p + sizeof(url);
	int cb = 0;
	cb = snprintf(p, p_end - p, "%s/%s", agent->base_url, end_point); 
	assert(cb > 0);
	p += cb;
	
	if(pagination) { /* todo: add tests */
		cb = snprintf(p, p_end - p, "?limit=%ld&order=%s", 
			(long)pagination->limit, 
			s_sz_pagination_order[pagination->order]);
		assert(cb > 0);
		p += cb;
		if(pagination->starting_after) {
			cb = snprintf(p, p_end - p, "&starting_after=%s", pagination->starting_after);
			assert(cb > 0);
			p += cb;
		}
		if(pagination->ending_before) {
			cb = snprintf(p, p_end - p, "&ending_before=%s", pagination->ending_before);
			assert(cb > 0);
			p += cb;
		}
	}
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, NULL, 0, 
		NULL);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}



/****************************************
 * coincheck private APIs
 * coincheck::Account
****************************************/

/**
 * Balance
 * Check your account balance.
 * It doesn't include jpy_reserved use unsettled orders in jpy_btc.
 * HTTP REQUEST
 * GET /api/accounts/balance
**/
int coincheck_account_get_balance(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * end_point = "api/accounts/balance";
	int rc = 0;
	assert(agent);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_query, &api_key, &api_secret); // use query_key (the principle of least privilege )
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, NULL, 0, 
		NULL);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}
//~ int coincheck_account_send_btc(trading_agency_t * agent, const char * address, const char * amount, json_object ** p_jresponse);
//~ int coincheck_account_get_sending_history(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
//~ int coincheck_account_get_deposit_history(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);

/**
 * Account information
 * HTTP REQUEST
 * GET /api/accounts
**/
int coincheck_account_get_info(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * end_point = "api/accounts";
	int rc = 0;
	assert(agent);
	
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_query, &api_key, &api_secret); // use query_key (the principle of least privilege )
	assert(0 == rc && api_key && api_secret);
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);
	
	http->headers = coincheck_auth_add_headers(http->headers, 
		api_key, api_secret, 
		url, NULL, 0, 
		NULL);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}
