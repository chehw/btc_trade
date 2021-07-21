/*
 * trading_agency.c
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

#include <pthread.h>

#include "trading_agency.h"
#include <json-c/json.h>
#include <curl/curl.h>

#include "json-response.h"

#define AUTO_UNLOCK_MUTEX_PTR __attribute__((cleanup(auto_unlock_mutex_ptr)))
void auto_unlock_mutex_ptr(void * ptr)
{
	pthread_mutex_t ** p_mutex = ptr;
	if(p_mutex && *p_mutex) pthread_mutex_unlock(*p_mutex);
	return;
}

typedef const char * string;
#define json_get_value(jobj, type, key) ({ \
		type value = (type)0;	\
		json_object * jvalue = NULL;	\
		json_bool ok = json_object_object_get_ex(jobj, #key, &jvalue);	\
		if(ok && jvalue) value = (type)json_object_get_##type(jvalue);	\
		value;	\
	})

static int trading_agency_load_config(struct trading_agency * agent, json_object * jconfig);
static int trading_agency_init(struct trading_agency * agent);
static int trading_agency_run(struct trading_agency * agent);
static int trading_agency_stop(struct trading_agency * agent);
static void trading_agency_cleanup(struct trading_agency * agent);
static int trading_agency_get_credentials(struct trading_agency * agent, 
		enum trading_agency_credentials_type type, 
		const char ** p_api_key, const char ** p_api_secret);
	
/************************************************
 * plugins manager:
 * @todo use dlopen() /d dlsym() ...
 */
#include "trading_agency_zaif.h"
typedef trading_agency_t * (*trading_agency_create_fn)(void * user_data);
trading_agency_create_fn trading_agency_query_create_function(const char * agency_name)
{
	///< @todo plugins_mgr->get_factory(agency_name)
	//~ if(strcasecmp(agency_name, "zaif") == 0) {
		//~ return trading_agency_zaif_new;
	//~ }
	return NULL;
}
int trading_agency_class_init(trading_agency_t * agent)
{
	agent->load_config = trading_agency_load_config;
	agent->init = trading_agency_init;
	agent->run = trading_agency_run;
	agent->stop = trading_agency_stop;
	agent->cleanup = trading_agency_cleanup;
	
	agent->get_credentials = trading_agency_get_credentials;
	
	return 0;
}
/**
 * end of plugins manager
************************************************/




/************************************************
 * trading_agency_private
************************************************/
#define TRADING_AGENCY_MAX_KEY_SIZE (100)
typedef struct trading_agency_private
{
	trading_agency_t * agent;
	json_object * jconfig;
	const char * base_url;
	
	char api_query_key[TRADING_AGENCY_MAX_KEY_SIZE];
	char api_query_secret[TRADING_AGENCY_MAX_KEY_SIZE];
	
	char api_trade_key[TRADING_AGENCY_MAX_KEY_SIZE];
	char api_trade_secret[TRADING_AGENCY_MAX_KEY_SIZE];
	
	char api_withdraw_key[TRADING_AGENCY_MAX_KEY_SIZE];
	char api_withdraw_secret[TRADING_AGENCY_MAX_KEY_SIZE];
	
	pthread_t th;
	pthread_mutex_t mutex;
	
	struct {
		pthread_cond_t cond;
		pthread_mutex_t mutex;
	}worker_cond_mutex;
	
	int64_t query_inteval_ms; 
	
	int is_running;
	int quit;
}trading_agency_private_t;
static trading_agency_private_t * trading_agency_private_new(trading_agency_t * agent)
{
	assert(agent);
	trading_agency_private_t * priv = calloc(1, sizeof(*priv));
	assert(priv);
	
	priv->agent = agent;
	agent->priv = priv;
	
	int rc = 0;
	rc = pthread_mutex_init(&priv->mutex, NULL);
	rc = pthread_mutex_init(&priv->worker_cond_mutex.mutex, NULL);
	rc = pthread_cond_init(&priv->worker_cond_mutex.cond, NULL);
	assert(0 == rc);
	
	priv->query_inteval_ms = 200;	// 200 ms (0.2s)
	return priv;
}
static void trading_agency_private_free(trading_agency_private_t * priv)
{
	if(NULL == priv) return;
	// clear secrets
	memset(priv->api_query_key, 0, sizeof(priv->api_query_key));
	memset(priv->api_query_secret, 0, sizeof(priv->api_query_secret));
	
	memset(priv->api_trade_key, 0, sizeof(priv->api_trade_key));
	memset(priv->api_trade_secret, 0, sizeof(priv->api_trade_secret));
	
	memset(priv->api_withdraw_key, 0, sizeof(priv->api_withdraw_key));
	memset(priv->api_withdraw_secret, 0, sizeof(priv->api_withdraw_secret));
	
	if(priv->jconfig) {
		json_object_put(priv->jconfig);
		priv->jconfig = NULL;
	}
	free(priv);
	return;
}

/************************************************
 * trading_agency
************************************************/
trading_agency_t * trading_agency_new(const char * exchange_name, void * user_data)
{
	trading_agency_t * agent = NULL;
	
	if(exchange_name) {
		trading_agency_create_fn agent_new = trading_agency_query_create_function(exchange_name);
		if(agent_new) agent = agent_new(user_data);
	}
	
	if(NULL == agent) {
		agent = calloc(1, sizeof(*agent));
		assert(agent);
		if(exchange_name) strncpy(agent->exchange_name, exchange_name, sizeof(agent->exchange_name));
		trading_agency_class_init(agent);
		agent->user_data = user_data;
	}
	
	http_json_context_init(agent->http, agent);

	trading_agency_private_t * priv = trading_agency_private_new(agent);
	assert(priv && priv == agent->priv);
	
	return agent;
}

void trading_agency_free(trading_agency_t * agent)
{
	if(NULL == agent) return;
	
	trading_agency_private_t * priv = agent->priv;
	
	if(priv->is_running) {
		agent->stop(agent);
	}
	
	if(agent->cleanup) agent->cleanup(agent);
	
	http_json_context_cleanup(agent->http);
	
	trading_agency_private_free(agent->priv);
	free(agent);
	return;
}

/************************************************
 * trading_agency::member functions
************************************************/
static int load_credentials_file(trading_agency_private_t * priv, const char * credentials_file)
{
	assert(priv && credentials_file);
	if(NULL == credentials_file) return -1;
	
	json_object * jcredentials = json_object_from_file(credentials_file);
	json_object * jquery = NULL;
	json_object * jtrade = NULL;
	json_object * jwithdraw = NULL;
	json_bool ok = FALSE;
	
	ok = json_object_object_get_ex(jcredentials, "query", &jquery);
	if(ok && jquery) {
		const char * api_key = json_get_value(jquery, string, api_key);
		const char * api_secret = json_get_value(jquery, string, api_secret);
		
		if(api_key) strncpy(priv->api_query_key, api_key, sizeof(priv->api_query_key));
		if(api_secret) strncpy(priv->api_query_secret, api_secret, sizeof(priv->api_query_secret)); 
	}
	
	ok = json_object_object_get_ex(jcredentials, "trade", &jtrade);
	if(ok && jtrade) {
		const char * api_key = json_get_value(jtrade, string, api_key);
		const char * api_secret = json_get_value(jtrade, string, api_secret);
		
		if(api_key) strncpy(priv->api_trade_key, api_key, sizeof(priv->api_trade_key));
		if(api_secret) strncpy(priv->api_trade_secret, api_secret, sizeof(priv->api_trade_secret)); 
	}
	
	ok = json_object_object_get_ex(jcredentials, "withdraw", &jwithdraw);
	if(ok && jwithdraw) {
		const char * api_key = json_get_value(jwithdraw, string, api_key);
		const char * api_secret = json_get_value(jwithdraw, string, api_secret);
		
		if(api_key) strncpy(priv->api_withdraw_key, api_key, sizeof(priv->api_withdraw_key));
		if(api_secret) strncpy(priv->api_withdraw_secret, api_secret, sizeof(priv->api_withdraw_secret)); 
	}
	
	json_object_put(jcredentials);
	return 0;
}

static int trading_agency_load_config(struct trading_agency * agent, json_object * jconfig)
{
	assert(agent && agent->priv);
	assert(jconfig);
	if(NULL == jconfig) return -1;

	trading_agency_private_t * priv = agent->priv;
	priv->jconfig = json_object_get(jconfig); // add_ref
	
	const char * exchange_name = json_get_value(jconfig, string, exchange_name);
	if(exchange_name) {
		strncpy(agent->exchange_name, exchange_name, sizeof(agent->exchange_name));
	}
	
	if(agent->base_url[0] == '\0') {
		const char * base_url = json_get_value(jconfig, string, base_url);
		assert(base_url);
		if(base_url) {
			strncpy(agent->base_url, base_url, sizeof(agent->base_url));
		}
	}
	
	const char * credentials_file = json_get_value(jconfig, string, credentials_file);
	//~ assert(credentials_file);
	if(credentials_file) {
		load_credentials_file(agent->priv, credentials_file);
	}
	return 0;
}

static int trading_agency_init(struct trading_agency * agent)
{
	return 0;
}

static void * worker_thread(void * user_data)
{
	struct trading_agency * agent = user_data;
	assert(agent && agent->priv);
	trading_agency_private_t * priv = agent->priv;
	int rc = 0;

	struct timespec timer = {
		.tv_sec = priv->query_inteval_ms / 1000,
		.tv_nsec = (priv->query_inteval_ms % 1000) * 1000000,
	};
	
	while(!priv->quit) 
	{
		struct timespec remaining = { 0 };
		nanosleep(&timer, &remaining);
	}
	
	priv->is_running = 0;
	priv->quit = 1;
	pthread_exit((void *)(intptr_t)rc);
}

static int trading_agency_run(struct trading_agency * agent)
{
	assert(agent && agent->priv);
	trading_agency_private_t * priv = agent->priv;
	int rc = 0;
	
	
	AUTO_UNLOCK_MUTEX_PTR pthread_mutex_t * mutex = &priv->mutex;
	pthread_mutex_lock(mutex);
	if(priv->is_running) return 0;
	
	priv->is_running = 1;
	rc = pthread_create(&priv->th, NULL, worker_thread, agent);
	assert(0 == rc);
	
	
	return rc;
}
static int trading_agency_stop(struct trading_agency * agent)
{
	assert(agent && agent->priv);
	trading_agency_private_t * priv = agent->priv;
	
	if(!priv->is_running) return -1;
	
	void * exit_code = NULL;
	if(priv->th) {
		int rc = pthread_join(priv->th, &exit_code);
		if(rc) {
			
		}
	}
	priv->is_running = 0;
	priv->quit = 1;
	return (int)(intptr_t)exit_code;
}

static void trading_agency_cleanup(struct trading_agency * agent)
{
	if(NULL == agent) return;
	//~ if(agent->priv) {
		//~ trading_agency_private_free(agent->priv);
		//~ agent->priv = NULL;
	//~ }
	return;
}

static int trading_agency_get_credentials(struct trading_agency * agent, 
		enum trading_agency_credentials_type type, 
		const char ** p_api_key, const char ** p_api_secret)
{
	assert(agent && agent->priv);
	trading_agency_private_t * priv = agent->priv;
	switch(type)
	{
	case trading_agency_credentials_type_query:
		if(p_api_key) *p_api_key = priv->api_query_key;
		if(p_api_secret) *p_api_secret = priv->api_query_secret;
		break;
	case trading_agency_credentials_type_trade:
		if(p_api_key) *p_api_key = priv->api_trade_key;
		if(p_api_secret) *p_api_secret = priv->api_trade_secret;
		break;
	case trading_agency_credentials_type_withdraw:
		if(p_api_key) *p_api_key = priv->api_withdraw_key;
		if(p_api_secret) *p_api_secret = priv->api_withdraw_secret;
		break;
	default:
		return -1;
	}
	return 0;
}
