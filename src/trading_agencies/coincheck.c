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
	int64_t nounce_ms = (int64_t)timestamp->tv_sec * 1000 + (timestamp->tv_nsec / 1000000) % 1000;
	char sz_nounce[100] = "";
	int cb_nounce = snprintf(sz_nounce, sizeof(sz_nounce), "%lu", (unsigned long)nounce_ms);
	assert(cb_nounce > 0);
	
	auto_buffer_push(message, sz_nounce, cb_nounce);
	auto_buffer_push(message, uri, strlen(uri));
	if(body) {
		if(cb_body == -1 || cb_body == 0) cb_body = strlen(body);
		if(cb_body > 0) auto_buffer_push(message, body, cb_body);
	}
	
	hmac_sha256_init(hmac, (unsigned char *)api_secret, strlen(api_secret));
	hmac_sha256_update(hmac, (unsigned char *)message->data, message->length);
	hmac_sha256_final(hmac, hash);
	bin2hex(hash, 32, &signature);
	
	char line[1024] = "";
	snprintf(line, sizeof(line), "ACCESS-KEY: %s", api_key);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	snprintf(line, sizeof(line), "ACCESS-NONCE: %lu", (unsigned long)nounce_ms);
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
 * json_response_context
****************************************/
struct json_response_context
{
	auto_buffer_t buf[1];
	json_object * jresponse;
	
	int auto_parse;
	int err_code;
	long response_code;
	
	json_tokener * jtok;
	enum json_tokener_error jerr;
};

static struct json_response_context * json_response_context_init(struct json_response_context * ctx, int auto_parse)
{
	if(NULL == ctx) ctx = malloc(sizeof(*ctx));
	assert(ctx);
	memset(ctx, 0, sizeof(*ctx));
	
	auto_buffer_init(ctx->buf, 0);
	if((ctx->auto_parse = auto_parse)) {
		ctx->jtok = json_tokener_new();
		assert(ctx->jtok);
	}
	return ctx;
}

static void json_response_context_clear(struct json_response_context * ctx)
{
	if(NULL == ctx) return;
	if(ctx->jresponse) {
		json_object_put(ctx->jresponse);
		ctx->jresponse = NULL;
	}
	json_tokener_reset(ctx->jtok);
	ctx->jerr = 0;
}

static void json_response_context_cleanup(struct json_response_context * ctx)
{
	if(NULL == ctx) return;
	auto_buffer_cleanup(ctx->buf);
	json_response_context_clear(ctx);
	
	if(ctx->jtok) {
		json_tokener_free(ctx->jtok);
		ctx->jtok = NULL;
	}
	return;
}

/****************************************
 * curl callback function
****************************************/
static size_t on_coincheck_json_response(char * ptr, size_t size, size_t n, void * user_data)
{
	struct json_response_context * ctx = user_data;
	assert(ctx);

	size_t cb = size * n;
	if(cb == 0) return 0;
	
	auto_buffer_push(ctx->buf, ptr, cb);
	if(!ctx->auto_parse) return cb;
	
	json_tokener * jtok = ctx->jtok;
	if(NULL == jtok) return 0;
	
	json_object * jobject = json_tokener_parse_ex(jtok, ptr, (int)cb);
	ctx->jerr = json_tokener_get_error(jtok);
	if(ctx->jerr == json_tokener_continue) return cb;
	if(ctx->jerr != json_tokener_success) {
		fprintf(stderr, "%s(%d)::json_token_parse failed: %s\n",
			__FILE__, __LINE__, json_tokener_error_desc(ctx->jerr));
		return 0;
	}
	ctx->jresponse = jobject;
	return cb;
}

/****************************************
 * helpler functions
****************************************/
static int http_get(CURL * _curl, const char * url, struct json_response_context * ctx)
{
	assert(url && ctx);
	CURL * curl = _curl;
	if(NULL == curl) {
		curl = curl_easy_init();
		assert(curl);
	}
	
	ctx->err_code = 0;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_coincheck_json_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
	
	CURLcode ret = curl_easy_perform(curl);
	ctx->err_code = ret;
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_perform() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return -1;
	}
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ctx->response_code);
	ctx->err_code = ret;
	
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_getinfo() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return -1;
	}
	
	curl_easy_reset(curl);
	if(curl != _curl) curl_easy_cleanup(curl);
	return ctx->err_code;
}

static int http_post(CURL * _curl, const char * url, const char * request_body, long cb_body, struct json_response_context * ctx)
{
	assert(url && ctx);
	CURL * curl = _curl;
	if(NULL == curl) {
		curl = curl_easy_init();
		assert(curl);
	}
	
	ctx->err_code = 0;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_coincheck_json_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
	
	CURLcode ret = curl_easy_perform(curl);
	ctx->err_code = ret;
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_perform() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return -1;
	}
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ctx->response_code);
	ctx->err_code = ret;
	
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_getinfo() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return -1;
	}
	
	curl_easy_reset(curl);
	if(curl != _curl) curl_easy_cleanup(curl);
	return ctx->err_code;
}

static int http_delete(CURL * _curl, const char * url, struct json_response_context * ctx)
{
	assert(url && ctx);
	CURL * curl = _curl;
	if(NULL == curl) {
		curl = curl_easy_init();
		assert(curl);
	}
	
	ctx->err_code = 0;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_coincheck_json_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	
	CURLcode ret = curl_easy_perform(curl);
	ctx->err_code = ret;
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_perform() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return -1;
	}
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ctx->response_code);
	ctx->err_code = ret;
	
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_getinfo() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return -1;
	}
	
	curl_easy_reset(curl);
	if(curl != _curl) curl_easy_cleanup(curl);
	return ctx->err_code;
}

/****************************************
 * coincheck public APIs
****************************************/
int coincheck_public_get_ticker(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * end_point = "api/ticker";
	assert(agent);
	
	int rc = 0;
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);

	char url[PATH_MAX] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point); 
	
	rc = http_get(NULL, url, ctx);
	if(0 == rc && ctx->response_code >= 200 && ctx->response_code < 300) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	json_response_context_cleanup(ctx);
	return rc;
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
	
	int rc = 0;
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);

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
	debug_printf("%s(): url=%s", __FUNCTION__, url);
	
	rc = http_get(NULL, url, ctx);
	if(0 == rc && ctx->response_code >= 200 && ctx->response_code < 300) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	json_response_context_cleanup(ctx);
	
	return rc;
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
	
	int rc = 0;
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);

	char url[PATH_MAX] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point); 
	
	rc = http_get(NULL, url, ctx);
	if(0 == rc && ctx->response_code >= 200 && ctx->response_code < 300) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
		rc = 0;
	}
	json_response_context_cleanup(ctx);
	return rc;
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
	
	int rc = 0;
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);

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
	debug_printf("%s(): url=%s", __FUNCTION__, url);
	
	rc = http_get(NULL, url, ctx);
	if(0 == rc && ctx->response_code >= 200 && ctx->response_code < 300) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	json_response_context_cleanup(ctx);
	return rc;
	return 0;
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
	
	int rc = 0;
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);

	char url[PATH_MAX] = "";
	int cb = 0;
	cb = snprintf(url, sizeof(url), "%s/%s/%s", agent->base_url, end_point, pair); 
	assert(cb > 0);
	
	rc = http_get(NULL, url, ctx);
	if(0 == rc && ctx->response_code >= 200 && ctx->response_code < 300) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	json_response_context_cleanup(ctx);
	
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);

	char request_body[4096] = "";
	char * p = request_body;
	char * p_end = p + sizeof(request_body);
	int cb_body = 0;
	cb_body = snprintf(p, p_end - p, "pair=%s&order_type=%s&rate=%f&amount=%f",
		pair, order_type, rate, amount);
	assert(cb_body > 0);
	debug_printf("post_fields: %s", request_body);
	
	CURL * curl = curl_easy_init();
	
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, request_body, cb_body, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_post(curl, url, request_body, -1, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);
	
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, NULL, 0, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_get(curl, url, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s/%s", agent->base_url, end_point, order_id);
	
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, NULL, 0, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_delete(curl, url, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s?id=%s", agent->base_url, end_point, order_id);
	
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, NULL, 0, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_get(curl, url, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
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
	debug_printf("%s(): url=%s", __FUNCTION__, url);
	
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, NULL, 0, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_get(curl, url, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}else {
		fprintf(stderr, "err_code=%d, response_code=%ld, buf=%.*s\n", 
			ctx->err_code, ctx->response_code, 
			(int)ctx->buf->length, 
			(char *)ctx->buf->data);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);
	
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, NULL, 0, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_get(curl, url, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
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
	
	struct json_response_context ctx[1];
	json_response_context_init(ctx, 1);
	
	char url[4096] = "";
	snprintf(url, sizeof(url), "%s/%s", agent->base_url, end_point);
	
	CURL * curl = curl_easy_init();
	struct curl_slist * headers = NULL;
	headers = coincheck_auth_add_headers(headers, api_key, api_secret, url, NULL, 0, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	
	rc = http_get(curl, url, ctx);
	curl_slist_free_all(headers);
	
	if(0 == rc && ctx->jresponse) {
		if(p_jresponse) *p_jresponse = json_object_get(ctx->jresponse);
	}
	
	json_response_context_cleanup(ctx);
	curl_easy_cleanup(curl);
	return rc;
}
