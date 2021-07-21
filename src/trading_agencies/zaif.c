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

#include <glib.h>

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
//~ static int zaif_trade_init_request(auto_buffer_t * request, const char * api_method, const struct timespec * timestamp)

//~ static json_object * zaif_trade_init_request(json_object * jrequest, const char * api_method, const struct timespec * timestamp)
//~ {
	//~ if(NULL == jrequest) jrequest = json_object_new_object();
	//~ assert(jrequest && api_method);
	
	//~ struct timespec ts[1];
	//~ if(NULL == timestamp) {
		//~ memset(ts, 0, sizeof(ts));
		//~ clock_gettime(CLOCK_REALTIME, ts);
		//~ timestamp = ts;
	//~ }
	//~ assert(timestamp);
	
	//~ int64_t nonce_ms = (int64_t)timestamp->tv_sec * 1000 + (timestamp->tv_nsec / 1000000) % 1000;
	//~ json_object_object_add(jrequest, "nonce", json_object_new_int64(nonce_ms));
	//~ json_object_object_add(jrequest, "method", json_object_new_string(api_method));
	//~ return jrequest;
//~ }

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
	
	GHmac * hmac = g_hmac_new(G_CHECKSUM_SHA512, (unsigned char *)api_secret, strlen(api_secret));
	assert(hmac);
	g_hmac_update(hmac, (unsigned char *)post_fields, length);
	const char * signature = g_hmac_get_string(hmac);
	
	char line[1024] = "";
	snprintf(line, sizeof(line), "key: %s", api_key);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	snprintf(line, sizeof(line), "sign: %s", signature);
	headers = curl_slist_append(headers, line);
	debug_printf("line: %s", line);
	
	g_hmac_unref(hmac);
	
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

/**
 * 現物取引APIの個別情報です。
 * 残高情報の取得
 * 現在の残高（余力および残高・トークン）、APIキーの権限、過去のトレード数、アクティブな注文数、サーバーのタイムスタンプを取得します。
 * 
 * パラメータ
 * パラメータ	必須	詳細	型	デフォルト
 * method	Yes	get_info	str	 
*/
int zaif_trade_get_info(trading_agency_t * agent, json_object ** p_jresponse)
{
	static const char * api_method = "get_info";

	int rc = 0;
	const char * api_key = NULL;
	const char * api_secret = NULL;
	rc = agent->get_credentials(agent, trading_agency_credentials_type_query, &api_key, &api_secret);
	assert(0 == rc && api_key && api_secret);

	const char * url = agent->base_url;
	struct timespec ts[1];
	memset(ts, 0, sizeof(ts));
	clock_gettime(CLOCK_REALTIME, ts);
	int64_t nonce = (int64_t)ts->tv_sec;
	
	json_object * jresponse = NULL;
	struct http_json_context * http = agent->http;
	struct json_response_context * response = http->response;
	
	auto_buffer_t post_fields[1];
	auto_buffer_init(post_fields, 0);
	auto_buffer_add_fmt(post_fields, "nonce=%" PRIu64, nonce);
	auto_buffer_add_fmt(post_fields, "&method=%s", api_method);
	
	printf("post_fields: %s\n", (char *)post_fields->data);
	
	http->headers = zaif_auth_add_headers(http->headers, api_key, api_secret, (char *)post_fields->data, post_fields->length);
	jresponse = http->post(http, url, (char *)post_fields->data, post_fields->length);
	auto_buffer_cleanup(post_fields);
	
	if(NULL == jresponse) return response->err_code;
	
	if(p_jresponse) *p_jresponse = jresponse;
	return response->err_code;
}
