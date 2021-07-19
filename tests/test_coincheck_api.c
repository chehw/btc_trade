/*
 * test_coincheck_api.c
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

#include <time.h>
#include <unistd.h>
#include <json-c/json.h>
#include <curl/curl.h>

#include "app.h"
#include "trading_agency.h"
#include "crypto/hmac.h"
#include "utils.h"
#include "auto_buffer.h"

#include "trading_agency_coincheck.h"

static void test_public_apis(trading_agency_t * agent, json_object * japi_doc);
static void test_private_apis(trading_agency_t * aggent, json_object * japi_doc);
void run_test(const char * conf_file, json_object * japi_doc);

int main(int argc, char **argv)
{
	curl_global_init(CURL_GLOBAL_SSL);
	
	const char * conf_file = "conf/config.json";
	const char * api_doc_file = "conf/coincheck-api.json";	// currently not used
	
	if(argc > 1) conf_file = argv[1];
	
	json_object * japi_doc = json_object_from_file(api_doc_file);
	assert(japi_doc);
	
	run_test(conf_file, japi_doc);
	
	json_object_put(japi_doc);
	curl_global_cleanup();
	return 0;
}

void run_test(const char * conf_file, json_object * japi_doc)
{
	static const char * agency_name = "coincheck";
	debug_printf("%s(): agency=%s, jdoc=%p", __FUNCTION__, agency_name, japi_doc);
	int rc = 0;
	trading_agency_t * coincheck = trading_agency_new(agency_name, NULL);
	
	json_object * jconfig = json_object_from_file(conf_file);
	assert(jconfig);
	
	/* load trading_agencies */
	json_object * jtrading_agencies = NULL;
	json_bool ok = json_object_object_get_ex(jconfig, "trading_agencies", &jtrading_agencies);
	assert(ok && jtrading_agencies);
	int num_agencies = json_object_array_length(jtrading_agencies);
	assert(num_agencies > 0);
	
	/* find jconfig of coincheck */
	json_object * jcoincheck_config = NULL;
	for(int i = 0; i < num_agencies; ++i) {
		json_object * jagency = json_object_array_get_idx(jtrading_agencies, i);
		assert(jagency);
		const char * exchange_name = json_get_value(jagency, string, exchange_name);
		assert(exchange_name);
		
		if(strcasecmp(exchange_name, agency_name) == 0) {
			jcoincheck_config = jagency;
			break;
		}
	}
	assert(jcoincheck_config);

	rc = coincheck->load_config(coincheck, jcoincheck_config);
	json_object_put(jconfig);
	
	assert(0 == rc);
	
	test_public_apis(coincheck, japi_doc);
	test_private_apis(coincheck, japi_doc);
	
	trading_agency_free(coincheck);
	return;
}

static void test_public_apis(trading_agency_t * agent, json_object * japi_doc)
{
	int rc = 0;
	json_object * jresponse = NULL;
	
	if(1) {
		rc = coincheck_public_get_ticker(agent, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "ticker: %s\n",
				json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED)
			);
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	if(1) {
		struct coincheck_pagination_params pagination[1] = {{
			.limit = 50,
			//~ .starting_after = "191661079",  // ???? 
		}};
		
		rc = coincheck_public_get_trades(agent, "btc_jpy", pagination, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "trades: %s\n",
				json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED)
			);
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	if(1) {
		rc = coincheck_public_get_order_book(agent, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "order_book: %s\n",
				json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED)
			);
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	if(1) {
		rc = coincheck_public_calc_rate(agent, "btc_jpy", "sell", 0, 0.1, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "order_book: %s\n",
				json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED)
			);
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	if(1) {
		rc = coincheck_public_get_buy_rate(agent, "btc_jpy", &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "get_buy_rate: %s\n",
				json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED)
			);
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	usleep(100 * 1000);
	exit(0);
	return;
}

static void test_private_apis(trading_agency_t * agent, json_object * japi_doc)
{
	assert(agent);
	
	int rc = 0;
	json_object * jresponse = NULL;
	
	const char * order_id = "3634782772";	// <== replaces with the real order_id 
	
	// 0. Check account balance.
	if(1) {
		//~ check_balance(curl, server, api_query_key, api_query_secret, NULL);
		rc = coincheck_account_get_balance(agent, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_account_get_balance: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
		
		rc = coincheck_account_get_info(agent, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_account_get_info: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	// 1. new order
	if(0) {
		rc = coincheck_new_order(agent, "btc_jpy", "buy", 3333000.0, 0.01, &jresponse);
		//~ rc = coincheck_new_order(agent, "btc_jpy", "buy", 3533000.0, 0.01, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_new_order: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	// 2. cancel order
	if(0) {
		//~ cancel_order(curl, server, api_trade_key, api_trade_secret, order_id, NULL);
		rc = coincheck_cancel_order(agent, order_id, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_cancel_order: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	usleep(100*1000);
	
	// 3. list_unsettled_orders
	if(0) {
		rc = coincheck_get_unsettled_order_list(agent, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_get_unsettled_order_list: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	// 4. check cancellation status
	if(0) {
		//~ check_cancellation_status(curl, server, api_trade_key, api_trade_secret, order_id, NULL);
		rc = coincheck_get_cancellation_status(agent, order_id, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_get_cancellation_status: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}
	
	// 5. get order history
	if(0) {
		//~ get_order_histroy(curl, server, api_trade_key, api_trade_secret, NULL);
		rc = coincheck_get_order_history(agent, NULL, &jresponse);
		assert(0 == rc);
		if(jresponse) {
			fprintf(stderr, "coincheck_get_cancellation_status: %s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
			json_object_put(jresponse);
			jresponse = NULL;
		}
	}

	return;
}
