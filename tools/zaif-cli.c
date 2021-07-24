/*
 * zaif-cli.c
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
#include <getopt.h>
#include <json-c/json.h>
#include <search.h>
#include <errno.h>

#include "trading_agency_zaif.h"

/****************************************
 * zaif trade APIs
****************************************/
//~ int zaif_trade_get_info(trading_agency_t * agent, json_object ** p_jresponse);
//~ int zaif_trade_get_info2(trading_agency_t * agent, json_object ** p_jresponse);
//~ int zaif_trade_get_personal_info(trading_agency_t * agent, json_object ** p_jresponse);
//~ int zaif_trade_get_id_info(trading_agency_t * agent, json_object ** p_jresponse);
//~ int zaif_trade_get_trade_history(trading_agency_t * agent, json_object ** p_jresponse);
//~ int zaif_trade_active_orders(trading_agency_t * agent, const char * currency_pair, _Bool is_token, _Bool is_token_both, json_object ** p_jresponse);
//~ int zaif_trade_buy(trading_agency_t * agent, const char * currency_pair, 
	//~ double price, double amount, 
	//~ double limit, const char * comment,  
	//~ json_object ** p_jresponse);
//~ int zaif_trade_sell(trading_agency_t * agent, const char * currency_pair, 
	//~ double price, double amount, 
	//~ double limit, const char * comment,  
	//~ json_object ** p_jresponse);

//~ int zaif_trade_cancel_order(trading_agency_t * agent, const char * order_id, 
	//~ const char * currency_pair, // nullable
	//~ _Bool is_token, // nullable
	//~ json_object ** p_jresponse);
	

struct cli_context;
int cli_trade_get_info(struct cli_context * ctx);
int cli_trade_get_info2(struct cli_context * ctx);
int cli_trade_get_personal_info(struct cli_context * ctx);
int cli_trade_get_id_info(struct cli_context * ctx);
int cli_trade_get_trade_history(struct cli_context * ctx);
int cli_trade_active_orders(struct cli_context * ctx);
int cli_trade_buy(struct cli_context * ctx);
int cli_trade_sell(struct cli_context * ctx);
int cli_trade_cancel_order(struct cli_context * ctx);


typedef int (* cli_function_ptr)(struct cli_context * cli);
typedef struct cli_mapping_record
{
	char command[100];
	const char * func_name;
	cli_function_ptr func;
}cli_mapping_record_t;

#define FUNCTION_DEF(_cmd, _func) { .command = #_cmd, .func_name = #_func, .func = _func }
static cli_mapping_record_t s_cli_mappings[] = {
	FUNCTION_DEF(get_info, cli_trade_get_info),
	FUNCTION_DEF(get_info2, cli_trade_get_info2),
	FUNCTION_DEF(get_personal_info, cli_trade_get_personal_info),
	FUNCTION_DEF(get_id_info, cli_trade_get_id_info),
	
	FUNCTION_DEF(get_trade_history, cli_trade_get_trade_history),
	FUNCTION_DEF(active_orders, cli_trade_active_orders),
	FUNCTION_DEF(btc_buy, cli_trade_buy),
	FUNCTION_DEF(btc_sell, cli_trade_sell),
	
	FUNCTION_DEF(cancel_order, cli_trade_cancel_order),
	{ "", }
};

static void * s_cli_mappings_root;
cli_function_ptr cli_mappings_find(const char * command)
{
	void * p_node = tfind(command, &s_cli_mappings_root, (__compar_fn_t)strcasecmp);
	if(NULL == p_node) return NULL;
	
	cli_mapping_record_t * record = *(void **)p_node;
	
	return record->func;
}

__attribute__((constructor))
static void cli_mappings_init(void)
{
	size_t num_mappings = sizeof(s_cli_mappings) / sizeof(s_cli_mappings[0]);
	for(size_t i = 0 ;i < num_mappings; ++i) {
		cli_mapping_record_t * record = &s_cli_mappings[i];
		if(!record->command[0]) break;
		
		tsearch(record, &s_cli_mappings_root, (__compar_fn_t)strcasecmp);
	}
	return;
}

static void no_free(void * ptr) { }

__attribute__((destructor))
static void cli_mapping_cleanup(void)
{
	tdestroy(s_cli_mappings_root, no_free);
	s_cli_mappings_root	= NULL; 
	return;
}

/**
 * todo: load function mappings from config file
 * 
 * // pyseudo code
 * load_config(function_mappings);
 * void *handle = dlopen(NULL, RTLD_LAZY);
 * void *func_ptr = dlsym(handle, config->functions[i].func_name");
**/

typedef struct cli_context
{
	trading_agency_t * agent;
	const char * command;
	int num_params;
	char ** params_list;
	char * exe_name;
}cli_context_t;
cli_context_t * cli_context_new(int argc, char ** argv);
void cli_context_free(cli_context_t * ctx);



static void print_usuage(const char * exe_name)
{
	fprintf(stderr, "%s command [params_list]\n", exe_name);
	
	fprintf(stderr, "Command List: \n");
	fprintf(stderr, 
		"  - get_info: (no params)\n"
		"    - description: \n"
		"    - examples: \n"
		"        %s get_info\n"
		"\n", 
		exe_name);
		
	fprintf(stderr, 
		"  - get_info2: (no params) \n"
		"      description: \n"
		"      examples: \n"
		"        %s get_info2\n"
		"\n", 
		exe_name);

	fprintf(stderr, 
		"  - get_personal_info: (no params) \n"
		"      description: \n"
		"      examples: \n"
		"        %s get_personal_info\n"
		"\n", 
		exe_name);
		
	fprintf(stderr, 
		"  - get_id_info: (no params) \n"
		"      description: \n"
		"      examples: \n"
		"        %s get_id_info\n"
		"\n", 
		exe_name);

	fprintf(stderr, 
		"  - get_trade_history: (no params) \n"
		"    - description: \n"
		"    - examples: \n"
		"        %s get_trade_history\n"
		"\n", 
		exe_name);
	
	fprintf(stderr, 
		"  - active_orders: \n"
		"    - params_list: [ pair=<btc_jpy> ]\n"
		"    - description: list_unsettled_orders \n"
		"    - examples: \n"
		"        %s active_orders n"
		"        %s active_orders btc_jpy\n"
		"\n", 
		exe_name, exe_name);
		
	fprintf(stderr, 
		"  - btc_buy: \n"
		"    - params_list: price=<price> amount=<amount> \n"
		"    - description: \n"
		"    - examples: \n"
		"        %s btc_buy price=3250000 amount=0.005\n"
		"        %s btc_buy 3250000 0.005\n"
		"\n", 
		exe_name, exe_name);
	
	fprintf(stderr, 
		"  - btc_sell: \n"
		"    - params_list: price=<price> amount=<amount> \n"
		"    - description: \n"
		"    - examples: \n"
		"        %s btc_sell price=3550000 amount=0.005\n"
		"        %s btc_sell 3550000 0.005\n"
		"\n", 
		exe_name, exe_name);
	
		
	fprintf(stderr, 
		"  - cancel_order: \n"
		"    - params_list: order_id=<order_id> ]\n"
		"    - description: \n"
		"    - examples: \n"
		"        %s cancel_order order_id='3637471298'\n"
		"        %s cancel_order '3637471298'\n"
		"\n", 
		exe_name, exe_name);
	return;
}

int main(int argc, char **argv)
{
	int rc = -1;
	cli_context_t * ctx = cli_context_new(argc, argv);
	
	cli_function_ptr cli_func = cli_mappings_find(ctx->command);
	if(cli_func) rc = cli_func(ctx);
	
	cli_context_free(ctx);
	return rc;
}

static inline int output_json_response(int rc, json_object * jresponse) 
{
	if(rc) {
		fprintf(stderr, "%s() failed.\n", __FUNCTION__);
	}
	if(jresponse) {
		printf("%s\n", json_object_to_json_string_ext(jresponse, JSON_C_TO_STRING_SPACED));
		json_object_put(jresponse);
	}
	return rc;
}


/*****************************************
 * cli functions
 ****************************************/
 
int cli_trade_get_info(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	json_object * jresponse = NULL;
	rc = zaif_trade_get_info(ctx->agent, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_trade_get_info2(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	json_object * jresponse = NULL;
	rc = zaif_trade_get_info2(ctx->agent, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_trade_get_personal_info(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	json_object * jresponse = NULL;
	rc = zaif_trade_get_personal_info(ctx->agent, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_trade_get_id_info(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	json_object * jresponse = NULL;
	rc = zaif_trade_get_id_info(ctx->agent, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_trade_get_trade_history(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	json_object * jresponse = NULL;
	rc = zaif_trade_get_trade_history(ctx->agent, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_trade_active_orders(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	json_object * jresponse = NULL;
	
	const char * pair = NULL;
	int is_token = 0;
	int is_token_both = 0;
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "pair=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			pair = p_find + strlen(pattern);
			continue;
		}
		
		pattern = "is_token=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			is_token = atoi(p_find + strlen(pattern));
			continue;
		}
		
		pattern = "is_token_both=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			is_token_both = atoi(p_find + strlen(pattern));
			continue;
		}
	}
	if(NULL == pair) pair = "btc_jpy";
	rc = zaif_trade_active_orders(ctx->agent, pair, is_token, is_token_both, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_trade_buy(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	
	const char * price = NULL;
	const char * amount = NULL;
	if(ctx->num_params != 2) {
		print_usuage(ctx->exe_name);
		return -1;
	}
	assert(ctx->num_params == 2);
	
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "price=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			price = p_find + strlen(pattern);
			continue;
		}
		
		pattern = "amount=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			amount =  p_find + strlen(pattern);
			continue;
		}
	}
	
	if(NULL == price && NULL == amount) {
		price = ctx->params_list[0];
		amount = ctx->params_list[1];
	}
	
	fprintf(stderr, "%s(price=%s, amount=%s)\n", __FUNCTION__, price, amount);

	rc = zaif_trade_buy(ctx->agent, "btc_jpy",
		atof(price), atof(amount), 
		0, NULL, 
		&jresponse);
	return output_json_response(rc, jresponse);
}
int cli_trade_sell(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	
	const char * price = NULL;
	const char * amount = NULL;
	if(ctx->num_params != 2) {
		print_usuage(ctx->exe_name);
		return -1;
	}
	assert(ctx->num_params == 2);
	
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "price=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			price = p_find + strlen(pattern);
			continue;
		}
		
		pattern = "amount=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			amount =  p_find + strlen(pattern);
			continue;
		}
	}
	
	if(NULL == price && NULL == amount) {
		price = ctx->params_list[0];
		amount = ctx->params_list[1];
	}
	
	fprintf(stderr, "%s(price=%s, amount=%s)\n", __FUNCTION__, price, amount);

	rc = zaif_trade_sell(ctx->agent, "btc_jpy",
		atof(price), atof(amount), 
		0, NULL, 
		&jresponse);
	return output_json_response(rc, jresponse);
}
int cli_trade_cancel_order(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	assert(ctx->num_params >= 1);
	if(ctx->num_params < 1) return EINVAL;
	
	static const char pattern[] = "order_id=";
	static int cb_pattern = sizeof(pattern) - 1;
	for(int i = 0; i < ctx->num_params; ++i)
	{
		json_object * jresponse = NULL;
		const char * order_id = ctx->params_list[i];
		const char * p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) order_id = p_find + cb_pattern;
		
		fprintf(stderr, "%s(order_id=%s)\n", __FUNCTION__, order_id);
		rc = zaif_trade_cancel_order(ctx->agent, order_id, NULL, FALSE, &jresponse);
		output_json_response(rc, jresponse);
		if(rc) break;
	}
	
	return rc;
} 

/*****************************************
 * cli context
 ****************************************/
cli_context_t * cli_context_new(int argc, char ** argv)
{
	if(argc <= 1) {
		print_usuage(argv[0]);
		exit(1);
	}
	
	const char * conf_file = "zaif-config.json";
	json_object * jconfig = json_object_from_file(conf_file);
	
	int rc = 0;
	cli_context_t * cli = calloc(1, sizeof(*cli));
	assert(cli);
	trading_agency_t * agent = trading_agency_new("zaif::trade", cli);
	assert(agent);
	rc = agent->load_config(agent, jconfig);
	assert(0 == rc);
	
	cli->exe_name = argv[0];
	cli->agent = agent;
	
	cli->command = argv[1];
	if(argc > 2) {
		cli->num_params = argc - 2;
		cli->params_list = &argv[2];
	}
	
	json_object_put(jconfig);

	return cli;
}
void cli_context_free(cli_context_t * ctx)
{
	if(NULL == ctx) return;
	if(ctx->agent) {
		trading_agency_free(ctx->agent);
		ctx->agent = NULL;
	}
	
	free(ctx);
}
