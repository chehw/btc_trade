/*
 * test_zaif_api.c
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

#include <json-c/json.h>
#include "trading_agency_zaif.h"
#include "utils.h"

static void test_public_apis(trading_agency_t * agent);
static void test_private_apis(trading_agency_t * aggent);
void run_test(const char * conf_file);

int main(int argc, char **argv)
{
	const char * conf_file = "conf/config.json";
	
	if(argc > 1) conf_file = argv[1];
	run_test(conf_file);
	
	return 0;
}


void run_test(const char * conf_file)
{
	int rc = 0;
	trading_agency_t * zaif_public = trading_agency_new("zaif::public", NULL);
	trading_agency_t * zaif_trade = trading_agency_new("zaif::trade", NULL);
	
	json_object * jconfig = json_object_from_file(conf_file);
	assert(jconfig);
	
	/* load trading_agencies */
	json_object * jtrading_agencies = NULL;
	json_object * jzaif_public= NULL;
	json_object * jzaif_trade = NULL;
	
	json_bool ok = json_object_object_get_ex(jconfig, "trading_agencies", &jtrading_agencies);
	assert(ok && jtrading_agencies);
	int num_agencies = json_object_array_length(jtrading_agencies);
	assert(num_agencies > 0);
	
	/* find jconfig of coincheck */
	for(int i = 0; i < num_agencies; ++i) {
		json_object * jagency = json_object_array_get_idx(jtrading_agencies, i);
		assert(jagency);
		const char * exchange_name = json_get_value(jagency, string, exchange_name);
		assert(exchange_name);
		
		if(strcasecmp(exchange_name, "zaif::public") == 0) {
			jzaif_public = jagency;
			continue;
		}
		if(strcasecmp(exchange_name, "zaif::trade") == 0) {
			jzaif_trade = jagency;
			continue;
		}
	}
	assert(jzaif_public && jzaif_trade);

	if(jzaif_public) {
		rc = zaif_public->load_config(zaif_public, jzaif_public);
		assert(0 == rc);
	}
	
	if(jzaif_trade) {
		rc = zaif_trade->load_config(zaif_trade, jzaif_trade);
		assert(0 == rc);
	}
	
	test_public_apis(zaif_public);
	test_private_apis(zaif_trade);
	
	trading_agency_free(zaif_public);
	trading_agency_free(zaif_trade);
	json_object_put(jconfig);
	return;
}


#define dump_json_response(title, jresponse) do { \
		if(jresponse) { \
			fprintf(stderr, "%s: %s\n", title, json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_PLAIN)); \
			json_object_put(jresponse); jresponse = NULL; \
		} \
	} while(0)

static void test_public_apis(trading_agency_t * agent)
{
	assert(agent);
	int rc = 0;
	json_object * jresponse = NULL;
	
	// 0. zaif_public_get_currency
	if(1) { 
		rc = zaif_public_get_currency(agent, "btc", &jresponse);
		assert(0 == rc);
		dump_json_response("currency", jresponse);
	}
	
	if(1) { 
		rc = zaif_public_get_currency(agent, "all", &jresponse);
		assert(0 == rc);
		dump_json_response("currency", jresponse);
	}
	
	return;
}
static void test_private_apis(trading_agency_t * aggent)
{
	return;
}
