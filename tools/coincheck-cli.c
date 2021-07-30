/*
 * coincheck-cli.c
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

#include "trading_agency_coincheck.h"

struct cli_context;
int cli_get_ticker(struct cli_context * ctx);
int cli_get_trades(struct cli_context * ctx);
int cli_btc_buy(struct cli_context * ctx);
int cli_btc_sell(struct cli_context * ctx);
int cli_cancel_order(struct cli_context * ctx);
int cli_list_unsettled_orders(struct cli_context * ctx);
int cli_get_order_history(struct cli_context * ctx);
int cli_get_balance(struct cli_context * ctx);
int cli_get_account(struct cli_context * ctx);

int cli_get_bank_accounts(struct cli_context * ctx);
int cli_bank_account_add(struct cli_context * ctx);
int cli_bank_account_remove(struct cli_context * ctx);
int cli_get_withdraws_history(struct cli_context * ctx);
int cli_withdraw_request(struct cli_context * ctx);
int cli_withdraw_cancel(struct cli_context * ctx);

typedef int (* cli_function_ptr)(struct cli_context * cli);
typedef struct cli_mapping_record
{
	char command[100];
	const char * func_name;
	cli_function_ptr func;
}cli_mapping_record_t;

#define FUNCTION_DEF(_cmd, _func) { .command = #_cmd, .func_name = #_func, .func = _func }
static cli_mapping_record_t s_cli_mappings[] = {
	FUNCTION_DEF(ticker, cli_get_ticker),
	FUNCTION_DEF(trades, cli_get_trades),
	FUNCTION_DEF(btc_buy, cli_btc_buy),
	FUNCTION_DEF(btc_sell, cli_btc_sell),
	FUNCTION_DEF(cancel_order, cli_cancel_order),
	FUNCTION_DEF(list_unsettled_orders, cli_list_unsettled_orders),
	FUNCTION_DEF(order_history, cli_get_order_history),
	FUNCTION_DEF(balance, cli_get_balance),
	FUNCTION_DEF(account, cli_get_account),
	// withdraws api
	FUNCTION_DEF(bank_accounts, cli_get_bank_accounts),
	FUNCTION_DEF(bank_account_add, cli_bank_account_add),
	FUNCTION_DEF(bank_account_remove, cli_bank_account_remove),
	FUNCTION_DEF(withdraws, cli_get_withdraws_history),
	FUNCTION_DEF(withdraw_request, cli_withdraw_request),
	FUNCTION_DEF(withdraw_cancel, cli_withdraw_cancel),
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
}cli_context_t;
cli_context_t * cli_context_new(int argc, char ** argv);
void cli_context_free(cli_context_t * ctx);


static void print_usuage(const char * exe_name)
{
	fprintf(stderr, "%s command [params_list]\n", exe_name);
	
	fprintf(stderr, "Command List: \n");
	for(int i = 0; ; ++i)
	{
		if(!s_cli_mappings[i].command || !s_cli_mappings[i].command[0]) break;
		fprintf(stderr, "\t%s", s_cli_mappings[i].command);
		if((i % 4) == 3) printf("\n"); 
	}
	printf("\nDescriptions: \n"); 
	
	fprintf(stderr, 
		"  - ticker: (no params)\n"
		"    - description: \n"
		"    - examples: \n"
		"        %s ticker\n"
		"\n", 
		exe_name);
		
	fprintf(stderr, 
		"  - trades: \n"
		"      params_list: pair=<pair> [ limit=<limit> ]\n"
		"      description: \n"
		"      examples: \n"
		"        %s trades pair=btc_jpy\n"
		"        %s trades pair=btc_jpy limit=30\n"
		"\n", 
		exe_name, exe_name);
	
	fprintf(stderr, 
		"  - btc_buy: \n"
		"    - params_list: rate=<rate> amount=<amount> ]\n"
		"    - description: \n"
		"    - examples: \n"
		"        %s btc_buy rate=3250000 amount=0.005\n"
		"        %s btc_buy 3250000 0.005\n"
		"\n", 
		exe_name, exe_name);
	
	fprintf(stderr, 
		"  - btc_sell: \n"
		"    - params_list: rate=<rate> amount=<amount> ]\n"
		"    - description: \n"
		"    - examples: \n"
		"        %s btc_sell rate=3550000 amount=0.005\n"
		"        %s btc_sell 3550000 0.005\n"
		"\n", 
		exe_name, exe_name);
		
	fprintf(stderr, 
		"  - cancel_order: \n"
		"    - params_list: <order_id> ]\n"
		"    - description: \n"
		"    - examples: \n"
		"        %s cancel_order order_id='3637471298'\n"
		"        %s cancel_order '3637471298'\n"
		"\n", 
		exe_name, exe_name);
		
	fprintf(stderr, 
		"  - list_unsettled_orders: (no params)\n"
		"    - description: \n"
		"      examples: \n"
		"        %s list_unsettled_orders\n"
		"\n",
		exe_name);
	
	fprintf(stderr, 
		"  - order_history: (no params)\n"
		"    - description: \n"
		"      examples: \n"
		"        %s order_history\n"
		"\n",
		exe_name);
		
	fprintf(stderr, 
		"  - balance: (no params)\n"
		"    - description: \n"
		"      examples: \n"
		"        %s balance\n"
		"\n",
		exe_name);
	
	fprintf(stderr, 
		"  - account: (no params)\n"
		"    - description: Get account info.\n"
		"      examples: \n"
		"        %s account\n"
		"\n",
		exe_name);
		
	// withdraws
	fprintf(stderr, 
		"  - bank_accounts: (no params)\n"
		"    - description: Get bank account info.\n"
		"      examples: \n"
		"        %s bank_accounts\n"
		"\n",
		exe_name);
		
	fprintf(stderr, 
		"  - bank_account_add: \n"
		"    - params_list: [ bank_name, branch_name, bank_account_type, number, name ]\n" 
		"    - description: add bank account.\n"
		"      examples: \n"
		"        %s bank_account_add\n"
		"        %s bank_account_add bank_name=\"<bank>\"\n"
		"\n",
		exe_name, exe_name);
		
	fprintf(stderr, 
		"  - bank_account_remove: bank_account_id\n"
		"    - params_list: [ bank_account_id ]\n" 
		"    - description: remove bank account.\n"
		"      examples: \n"
		"        %s bank_account_remove <bank_account_id> \n"
		"\n",
		exe_name);
	
	fprintf(stderr, 
		"  - withdraws: (no params)\n"
		"    - description: Get withdraws history.\n"
		"      examples: \n"
		"        %s withdraws\n"
		"\n",
		exe_name);
	
	fprintf(stderr, 
		"  - withdraw_request: \n"
		"    - description: Get withdraws history.\n"
		"    - params_list: [ bank_account_id, amount ]\n" 
		"      examples: \n"
		"        %s withdraw_request <bank_account_id> \"10000.0\" \n"
		"\n",
		exe_name);
		
	fprintf(stderr, 
		"  - withdraw_cancel: \n"
		"    - params_list: [ withdraw_id ]\n" 
		"    - description: cancel withdraws.\n"
		"      examples: \n"
		"        %s withdraw_cancel <withdraw_id>\n"
		"\n",
		exe_name);
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
int cli_get_ticker(cli_context_t * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	rc = coincheck_public_get_ticker(ctx->agent, &jresponse);
	
	return output_json_response(rc, jresponse);
}

int cli_get_trades(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	const char * pair = NULL;
	struct coincheck_pagination_params pagination = { 
		.limit = 50,
	};
	
	
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "pair=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			pair = p_find + strlen(pattern);
			continue;
		}
		
		pattern = "limit=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			pagination.limit = atoi(p_find + strlen(pattern));
			continue;
		}
	}
	if(NULL == pair) pair = "btc_jpy";
	
	rc = coincheck_public_get_trades(ctx->agent, pair, &pagination, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_btc_buy(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	
	const char * rate = NULL;
	const char * amount = NULL;
	assert(ctx->num_params == 2);
	
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "rate=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			rate = p_find + strlen(pattern);
			continue;
		}
		
		pattern = "amount=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			amount =  p_find + strlen(pattern);
			continue;
		}
	}
	
	if(NULL == rate && NULL == amount) {
		rate = ctx->params_list[0];
		amount = ctx->params_list[1];
	}
	
	fprintf(stderr, "%s(rate=%s, amount=%s)\n", __FUNCTION__, rate, amount);

	rc = coincheck_new_order(ctx->agent, "btc_jpy", "buy", 
		atof(rate), atof(amount), 
		&jresponse);
	return output_json_response(rc, jresponse);
}

int cli_btc_sell(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	
	const char * rate = NULL;
	const char * amount = NULL;
	assert(ctx->num_params == 2);
	
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "rate=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			rate = p_find + strlen(pattern);
			continue;
		}
		
		pattern = "amount=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			amount =  p_find + strlen(pattern);
			continue;
		}
	}
	
	if(NULL == rate && NULL == amount) {
		rate = ctx->params_list[0];
		amount = ctx->params_list[1];
	}
	
	fprintf(stderr, "%s(rate=%s, amount=%s)\n", __FUNCTION__, rate, amount);

	rc = coincheck_new_order(ctx->agent, "btc_jpy", "sell", 
		atof(rate), atof(amount), 
		&jresponse);
	return output_json_response(rc, jresponse);
}

int cli_cancel_order(cli_context_t * ctx)
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
		rc = coincheck_cancel_order(ctx->agent, order_id, &jresponse);
		output_json_response(rc, jresponse);
		if(rc) break;
	}
	
	return rc;
}

int cli_list_unsettled_orders(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	rc = coincheck_get_unsettled_order_list(ctx->agent, &jresponse);
	
	return output_json_response(rc, jresponse);
}
int cli_get_order_history(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	rc = coincheck_get_order_history(ctx->agent, NULL, &jresponse);
	
	return output_json_response(rc, jresponse);
}

int cli_get_balance(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	rc = coincheck_account_get_balance(ctx->agent, &jresponse);
	
	return output_json_response(rc, jresponse);
}


int cli_get_account(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	rc = coincheck_account_get_info(ctx->agent, &jresponse);
	
	return output_json_response(rc, jresponse);
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
	
	const char * conf_file = "coincheck-config.json";
	json_object * jconfig = json_object_from_file(conf_file);
	
	int rc = 0;
	cli_context_t * cli = calloc(1, sizeof(*cli));
	assert(cli);
	trading_agency_t * agent = trading_agency_new("coincheck", cli);
	assert(agent);
	rc = agent->load_config(agent, jconfig);
	assert(0 == rc);
	
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




/****************************************
 * coincheck private APIs
 * coincheck::Withdraw
****************************************/
int cli_get_bank_accounts(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	rc = coincheck_get_bank_accounts(ctx->agent, &jresponse);
	return output_json_response(rc, jresponse);
}


#define BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH (100)
struct bank_account_info
{
	char bank_name[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH];
	char branch_name[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH];
	char bank_account_type[32];
	char number[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH];
	char name[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH];
};
int bank_account_info_parse_args(struct bank_account_info * info, int num_args, char ** args)
{
#define check_key_and_set(keyname) do { \
		if(strcasecmp(key, #keyname) == 0) strncpy(info->keyname, value, sizeof(info->keyname)); \
	} while(0)
	

	for(int i = 0; i < num_args; ++i) {
		char line[1024] = "";
		int cb = snprintf(line, sizeof(line), "%s", args[i]);
		assert(cb > 0);
		
		char * token = NULL;
		char * key = strtok_r(line, "=", &token);
		char * value = strtok_r(NULL, "\n", &token);
		if(NULL == key || NULL == value) return -1;
	
		check_key_and_set(bank_name);
		check_key_and_set(branch_name);
		check_key_and_set(bank_account_type);
		check_key_and_set(number);
		check_key_and_set(name);
	}
#undef check_key_and_set

	return 0;
}

static int ask_value(const char * keyname, const char * hints, char * value, size_t size)
{
	assert(keyname && value && size > 0);
	if(hints) {
		printf("%s: (%s): ", keyname, hints);
	}else {
		printf("%s: ", keyname);
	}
	fflush(stdout);
	
	char * line = fgets(value, size, stdin);
	int cb = strlen(line);
	if(cb <= 0) return -1;
	
	while(cb > 0 && (line[cb - 1] == '\n' || line[cb - 1] == '\r')) line[--cb] = '\0';
	if(cb <= 0) {
		fprintf(stderr, "\e[31m" "[%s] should not be empty." "\e[39m" "\n", keyname);
		exit(1);
		return -1;
	}
	
	return 0;
}

static void bank_account_info_dump(const struct bank_account_info * info) 
{
	printf("===== bank info =====\n");
	printf("bank_name: %s\n", info->bank_name);
	printf("branch_name: %s\n", info->branch_name);
	printf("bank_account_type: %s\n", info->bank_account_type);
	printf("number: %s\n", info->number);
	printf("name: %s\n", info->name);
}

int cli_bank_account_add(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	struct bank_account_info info[1];
	memset(info, 0, sizeof(info));
	
	json_object * jresponse = NULL;
	rc = bank_account_info_parse_args(info, ctx->num_params, ctx->params_list);
	assert(0 == rc);
	
	if(!info->bank_name[0]) rc = ask_value("bank_name", NULL, info->bank_name, sizeof(info->bank_name));
	assert(0 == rc);
	if(!info->branch_name[0]) rc = ask_value("branch_name", NULL, info->branch_name, sizeof(info->branch_name));
	assert(0 == rc);
	if(!info->bank_account_type[0]) rc = ask_value("bank_account_type", "futsu | toza", info->bank_account_type, sizeof(info->bank_account_type));
	assert(0 == rc);
	if(!info->number[0]) rc = ask_value("number", NULL, info->number, sizeof(info->number));
	assert(0 == rc);
	if(!info->name[0]) rc = ask_value("name", "フリガナ", info->name, sizeof(info->name));
	assert(0 == rc);
	
	if(1) bank_account_info_dump(info);
	
	return 0;
	
	rc = coincheck_bank_account_add(ctx->agent, 
		info->bank_name,
		info->branch_name, 
		info->bank_account_type,
		info->number,
		info->name,
		&jresponse);
	return output_json_response(rc, jresponse);
}
int cli_bank_account_remove(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	char bank_account_id[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH] = "";
	long id = 0;
	
	if(ctx->num_params < 1) {
		rc = ask_value("bank_account_id", NULL, bank_account_id, sizeof(bank_account_id));
		if(rc) {
			fprintf(stderr, "\e[31m" "bank_account_id should not be empty." "\e[39m" "\n");
		}
		else id = atol(bank_account_id);
	} else id = atol(ctx->params_list[0]);
	
	if(id <= 0) {
		fprintf(stderr, "\e[31m" "invalid bank_account_id (%ld)." "\e[39m" "\n", id);
	}
	
	json_object * jresponse = NULL;
	rc = coincheck_bank_account_remove(ctx->agent, id, &jresponse);
	return output_json_response(rc, jresponse);
}
int cli_get_withdraws_history(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	
	json_object * jresponse = NULL;
	struct coincheck_pagination_params pagination = { 
		.limit = 50,
	};
	
	for(int i =0; i < ctx->num_params; ++i) {
		const char * pattern = NULL;
		const char * p_find = NULL;
		pattern = "limit=";
		p_find = strstr(ctx->params_list[i], pattern);
		if(p_find) {
			pagination.limit = atoi(p_find + strlen(pattern));
			continue;
		}
	}
	
	rc = coincheck_get_withdraws_history(ctx->agent, &pagination, &jresponse);
	return output_json_response(rc, jresponse);
}

int cli_withdraw_request(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	char bank_account_id[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH] = "";
	char amount[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH] = "";
	long id = 0;
	
	if(ctx->num_params >= 2) {
		strncpy(bank_account_id, ctx->params_list[0], sizeof(bank_account_id));
		strncpy(amount, ctx->params_list[1], sizeof(amount));
	}
	if(!bank_account_id[0]) ask_value("bank_account_id", NULL, bank_account_id, sizeof(bank_account_id));
	if(!amount[0]) ask_value("amount", NULL,  amount, sizeof(amount));
	
	id = atol(bank_account_id);
	printf("======\n");
	printf("bank_account_id: %ld, amount=%s\n", id, amount);
	
	if(id <= 0) {
		fprintf(stderr, "\e[31m" "invalid bank_account_id (%ld)." "\e[39m" "\n", id);
	}
	
	json_object * jresponse = NULL;
	rc = coincheck_withdraw_request(ctx->agent, id, amount, "JPY", &jresponse);
	return output_json_response(rc, jresponse);
}
int cli_withdraw_cancel(struct cli_context * ctx)
{
	assert(ctx && ctx->agent);
	int rc = 0;
	char withdraw_id[BANK_ACCOUNT_INFO_MAX_FIELD_LENGTH] = "";
	long id = 0;
	
	if(ctx->num_params >= 1) {
		strncpy(withdraw_id, ctx->params_list[0], sizeof(withdraw_id));
	}
	if(!withdraw_id[0]) ask_value("withdraw_id", NULL, withdraw_id, sizeof(withdraw_id));
	if(id <= 0) {
		fprintf(stderr, "\e[31m" "invalid bank_account_id (%ld)." "\e[39m" "\n", id);
	}
	id = atol(withdraw_id);
	printf("======\n");
	printf("cancel withdraw: withdraw_id=%ld\n", id);
	
	json_object * jresponse = NULL;
	rc = coincheck_withdraw_cancel(ctx->agent, id, &jresponse);
	return output_json_response(rc, jresponse);
}
