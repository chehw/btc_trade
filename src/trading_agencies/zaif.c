/*
 * zaif.c
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
#include <limits.h>
#include <stdarg.h>

#include "trading_agency.h"
#include "trading_agency_zaif.h"
#include "json-response.h"

#include "auto_buffer.h"
#include "crypto/hmac.h"
#include "utils.h"

static void auto_buffer_add_fmt(auto_buffer_t * buf, const char * fmt, ...)
{
	auto_buffer_resize(buf, buf->length + PATH_MAX);
	char * line = (char *)auto_buffer_get_data(buf) + buf->length;
	
	ssize_t cb_line = 0;
	va_list ap;
	va_start(ap, fmt);
	cb_line = vsnprintf(line, PATH_MAX, fmt, ap);
	va_end(ap);
	assert(cb_line > 0);
	
	buf->length += cb_line;
	return;
}

/**
 * zaif_auth_add_headers()
 */
struct curl_slist * zaif_auth_add_headers(
	struct curl_slist * headers, 
	const char * api_key, const char * api_secret, 
	const char * post_fields, ssize_t length)
{
	assert(api_key && api_secret);
	assert(post_fields);
	
	unsigned char hash[64] = { 0 };
	hmac_sha512_hash(api_secret, strlen(api_secret), post_fields, length, hash);
	
	char * signature = NULL;
	ssize_t cb_signature = bin2hex(hash, 64, &signature);
	assert(cb_signature == 128);
	
	char line[1024] = "";
	snprintf(line, sizeof(line), "key: %s", api_key);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	snprintf(line, sizeof(line), "sign: %s", signature);
	
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	free(signature);
	return headers;
}


/****************************************
 * zaif public APIs
****************************************/
int zaif_public_get_currency(trading_agency_t * agent, const char * currency, json_object ** p_jresponse)
{
	static const char * end_point = "currencies";

	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	if(NULL == currency) currency = "all";
	char url[PATH_MAX] = "";
	snprintf(url, sizeof(url), "%s/%s/%s", agent->base_url, end_point, currency);
	
	jresponse = http->get(http, url);
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}



/****************************************
 * zaif trade APIs
****************************************/
static inline double nonce_generate(const struct timespec * timestamp) 
{
	struct timespec ts[1];
	if(NULL == timestamp) {
		memset(ts, 0, sizeof(ts));
		clock_gettime(CLOCK_REALTIME, ts);
		timestamp = ts;
	}
	//~ int64_t nonce = timestamp->tv_sec;
	//~ return nonce;
	
	double nonce = (double)timestamp->tv_sec +  (double)timestamp->tv_nsec / 1000000000.0;
	return nonce;
}

static int init_post_fields(auto_buffer_t * post_fields, const char * method)
{
	double nonce = nonce_generate(NULL);
	auto_buffer_init(post_fields, 0);
	auto_buffer_add_fmt(post_fields, "nonce=%.3f&method=%s", (double)nonce, method);
	return 0;
}

static int send_request(trading_agency_t * agent, 
	enum trading_agency_credentials_type credentials_type, 
	auto_buffer_t * post_fields, 
	json_object ** p_jresponse)
{
	assert(agent && post_fields);
	if(-1 == credentials_type) credentials_type = trading_agency_credentials_type_query;
	
	int rc = 0;
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, credentials_type, &api_key, &api_secret);
	assert(0 == rc && api_key && api_secret);
	
	const char * url = agent->base_url;
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	http->headers = zaif_auth_add_headers(http->headers, api_key, api_secret, (char *)post_fields->data, post_fields->length);
	jresponse = http->post(http, url, (char *)post_fields->data, post_fields->length);
	auto_buffer_cleanup(post_fields);
	
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}

/**
 * ????????????API????????????????????????
 * ?????????????????????
 * ????????????????????????????????????????????????????????????API????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
 * 
 * ???????????????
 * ???????????????	??????	??????	???	???????????????
 * method	Yes	get_info	str	 
*/
int zaif_trade_get_info(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * api_method = "get_info";
	int rc = 0;
	auto_buffer_t post_fields[1];
	
	init_post_fields(post_fields, api_method);
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_query, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

/**
????????????(??????)?????????
get_info???????????????????????????????????????????????????????????????????????????
**/
int zaif_trade_get_info2(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * api_method = "get_info2";
	int rc = 0;
	auto_buffer_t post_fields[1];
	
	init_post_fields(post_fields, api_method);
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_query, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

/**
???????????????????????????
????????????????????????????????????????????????????????????????????????????????????
**/
int zaif_trade_get_personal_info(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * api_method = "get_personal_info";
	int rc = 0;
	auto_buffer_t post_fields[1];
	
	init_post_fields(post_fields, api_method);
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_query, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

/**
?????????????????????
????????????ID?????????????????????????????????????????????????????????????????????
**/
int zaif_trade_get_id_info(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * api_method = "get_id_info";
	int rc = 0;
	auto_buffer_t post_fields[1];
	
	init_post_fields(post_fields, api_method);
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_query, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

/**
??????????????????????????????????????????
??????????????????????????????????????????????????????
**/
int zaif_trade_get_trade_history(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * api_method = "trade_history";
	int rc = 0;
	auto_buffer_t post_fields[1];
	
	init_post_fields(post_fields, api_method);
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_query, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

/**
??????????????????????????????
???????????????????????????????????????????????????????????????????????????

??????
???????????????5?????????10???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
**/
int zaif_trade_active_orders(trading_agency_t * agent, const char * currency_pair, _Bool is_token, _Bool is_token_both, json_object ** p_jresponse)
{
	static const char * api_method = "active_orders";
	int rc = 0;
	auto_buffer_t post_fields[1];
	
	init_post_fields(post_fields, api_method);
	if(currency_pair) auto_buffer_add_fmt(post_fields, "&currency_pair=%s", currency_pair);
	if(is_token) auto_buffer_push(post_fields, "&is_token=true", -1);
	if(is_token_both) auto_buffer_push(post_fields,  "&is_token_both=true", -1);
	
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_query, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}


/**
??????
??????????????????????????????

??????
???????????????10?????????9???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????

??????????????? limit????????????
??????????????????????????????????????????????????????????????????????????????????????????????????? ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????

??????????????? comment????????????
???????????????255?????????????????????????????????????????????????????????????????? ???????????????????????????????????????????????????????????????????????? ????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????? ????????????????????????????????????????????????

??????????????????????????????????????????
??????????????????price?????????limit????????????????????????(amount)???????????????????????????????????????????????????invalid price parameter?????????invalid amount parameter??????????????????????????????????????? ???????????????????????????????????????API????????????????????????????????????????????? ??????????????????????????????????????????????????????????????????????????????????????????????????????????????????
**/
int zaif_trade_buy(trading_agency_t * agent, const char * currency_pair, 
	double price, double amount, 
	double limit, const char * comment,  
	json_object ** p_jresponse)
{
	static const char * api_method = "trade";
	int rc = 0;
	auto_buffer_t post_fields[1];
	assert(currency_pair && price > 0.00001 && amount >= 0.00001);
	
	init_post_fields(post_fields, api_method);
	auto_buffer_add_fmt(post_fields, "&currency_pair=%s", currency_pair);
	auto_buffer_push(post_fields, "&action=bid", -1);	// action: bid(??????) or ask(??????)
	auto_buffer_add_fmt(post_fields, "&price=%.3f&amount=%.3f", price, amount);
	if(limit >= 0.00001) {
		auto_buffer_add_fmt(post_fields, "&limit=%f", limit);
	}
	if(comment) {
		CURL * curl = agent->http->curl;
		char * encoded_str = curl_easy_escape(curl, comment, strlen(comment));
		if(encoded_str) {
			auto_buffer_add_fmt(post_fields, "&comment=%s", encoded_str);
			curl_free(encoded_str);
		}
	}
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_trade, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

int zaif_trade_sell(trading_agency_t * agent, const char * currency_pair, 
	double price, double amount, 
	double limit, const char * comment,  
	json_object ** p_jresponse)
{
	static const char * api_method = "trade";
	int rc = 0;
	auto_buffer_t post_fields[1];
	assert(currency_pair && price > 0.00001 && amount >= 0.00001);
	
	init_post_fields(post_fields, api_method);
	auto_buffer_add_fmt(post_fields, "&currency_pair=%s", currency_pair);
	auto_buffer_push(post_fields, "&action=ask", -1); // action: bid(??????) or ask(??????)
	auto_buffer_add_fmt(post_fields, "&price=%f&amount=%f", price, amount);
	if(limit >= 0.00001) {
		auto_buffer_add_fmt(post_fields, "&limit=%f", limit);
	}
	if(comment) {
		CURL * curl = agent->http->curl;
		char * encoded_str = curl_easy_escape(curl, comment, strlen(comment));
		if(encoded_str) {
			auto_buffer_add_fmt(post_fields, "&comment=%s", encoded_str);
			curl_free(encoded_str);
		}
	}
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_trade, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}

/**
??????????????????
????????????????????????????????????
**/
int zaif_trade_cancel_order(trading_agency_t * agent, const char * order_id, const char * currency_pair, _Bool is_token, json_object ** p_jresponse)
{
	static const char * api_method = "cancel_order";
	assert(agent && order_id);
	
	int rc = 0;
	auto_buffer_t post_fields[1];
	init_post_fields(post_fields, api_method);
	
	auto_buffer_add_fmt(post_fields, "&order_id=%s", order_id);
	if(currency_pair) auto_buffer_add_fmt(post_fields, "&currency_pair=%s", currency_pair);
	if(is_token) auto_buffer_push(post_fields, "&is_token=true", -1);
	
	debug_printf("post_fields: %s", (char *)post_fields->data);
	
	rc = send_request(agent, trading_agency_credentials_type_trade, post_fields, p_jresponse);
	auto_buffer_cleanup(post_fields);
	return rc;
}




