/*
 * app.c
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

#include "app.h"
#include <getopt.h>
#include <json-c/json.h>
#include "shell.h"
#include "trading_agency.h"

#include "gui/coincheck-gui.h"
#include "utils.h"


int main(int argc, char **argv)
{
	app_context_t * app = app_context_init(NULL, argc, argv, NULL);
	assert(app);

	app->init(app);
	app->run(app);
	app->stop(app);
	
	app_context_cleanup(app);
	free(app);
	return 0;
}


/************************************************
 * app_context
************************************************/
typedef struct app_private
{
	app_context_t * app;
	const char * conf_file;
	json_object * jconfig;
	
	struct shell_context * shell;
	
	int num_agencies;
	trading_agency_t ** agencies;
}app_private_t;

static app_private_t * app_private_new(app_context_t * app) 
{
	assert(app);
	app_private_t * priv = calloc(1, sizeof(*priv));
	assert(priv);
	priv->app = app;
	app->priv = priv;
	return priv;
}

static void * app_private_free(app_private_t * priv)
{
	assert(priv && priv->app);
	priv->app->priv = NULL;
	
	shell_context_cleanup(priv->shell);
	priv->shell = NULL;
	
	int num_agencies = priv->num_agencies;
	trading_agency_t ** agencies = priv->agencies;
	priv->agencies = NULL;
	priv->num_agencies = 0;
	
	if(agencies) {
		for(int i = 0; i < num_agencies; ++i) {
			trading_agency_free(agencies[i]);
			agencies[i] = NULL;
		}
		free(agencies);
	}
	
	if(priv->jconfig) {
		json_object_put(priv->jconfig);
		priv->jconfig = NULL;
	}
	free(priv);
	return priv;
}

static int app_private_parse_args(app_private_t * priv, int argc, char ** argv)
{
	assert(priv);
	static struct option options[] = {
		{ "conf_file", required_argument, 0, 'c' },
		{NULL,}
	};
	const char * conf_file = NULL;
	while(1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "c:", options, &option_index);
		if(-1 == c) break;
		
		switch(c) {
		case 'c': conf_file = optarg; break;
		default:
			fprintf(stderr, "unknown args '%c(%.2x)'\n", c, (unsigned char)c);
			exit(1);
		}
	}
	
	if(conf_file) {
		priv->conf_file = conf_file;
	}

	return 0;
}


static int app_load_config(struct app_context * app, const char * conf_file)
{
	assert(app && app->priv);
	app_private_t * priv = app->priv;
	int rc = 0;
	json_object * jconfig = json_object_from_file(conf_file);
	assert(jconfig);
	
	priv->jconfig = jconfig;
	json_object * jtrading_agencies = NULL;
	json_bool ok = json_object_object_get_ex(jconfig, "trading_agencies", &jtrading_agencies);
	assert(ok && jtrading_agencies);
	
	int num_agencies = json_object_array_length(jtrading_agencies);
	assert(num_agencies > 0);
	
	trading_agency_t ** agencies = calloc(num_agencies, sizeof(*agencies));
	assert(agencies);
	
	for(int i = 0; i < num_agencies; ++i) {
		json_object * jagency_config = json_object_array_get_idx(jtrading_agencies, i);
		const char * exchange_name = json_get_value(jagency_config, string, exchange_name);
		assert(exchange_name);
		
		trading_agency_t * agent = trading_agency_new(exchange_name, app);
		assert(agent);
		rc = agent->load_config(agent, jagency_config);
		assert(0 == rc);
		agencies[i] = agent;
	}
	priv->num_agencies = num_agencies;
	priv->agencies = agencies;
	
	return rc;
}

static int app_init(struct app_context * app)
{
	assert(app && app->priv);
	app_private_t * priv = app->priv;
	int rc = 0;
	if(NULL == priv->conf_file) priv->conf_file = "conf/config.json";
	rc = app_load_config(app, priv->conf_file);
	
	assert(0 == rc);
	shell_context_t * shell = shell_context_init(NULL, app->argc, app->argv, app);
	assert(shell);
	priv->shell = shell;
	
	shell->init(shell);
	
	trading_agency_t * coincheck_agency = app_context_get_trading_agency(app, "coincheck");
	assert(coincheck_agency);
	
	coincheck_panel_init(coincheck_agency, shell);
	return 0;
	
}
static int app_run(struct app_context * app)
{
	assert(app && app->priv);
	app_private_t * priv = app->priv;
	shell_context_t * shell = priv->shell;
	assert(shell);
	
	if(app->is_running || app->quit) return -1;
	app->is_running = 1;
	
	//
	// ...
	
	
	// start shell 
	shell->run(shell);
	
	app->is_running = 0;
	app->quit = 1;
	return 0;
}
static int app_stop(struct app_context * app)
{
	assert(app && app->priv);
	app_private_t * priv = app->priv;
	shell_context_t * shell = priv->shell;
	assert(shell);
	
	if(shell->is_running)  shell->stop(shell);
	return 0;
}

static void app_cleanup(struct app_context * app)
{
	return;
}


app_context_t * app_context_init(app_context_t * app, int argc, char ** argv, void * user_data)
{
	if(NULL == app) {
		app = calloc(1, sizeof(*app));
		assert(app);
	}
	
	app->user_data = user_data;
	
	app->load_config = app_load_config;
	app->init = app_init;
	app->run = app_run;
	app->stop = app_stop;
	app->cleanup = app_cleanup;
	
	app_private_t * priv = app_private_new(app);
	app_private_parse_args(priv, argc, argv);
	
	app->argc = argc;
	app->argv = argv;
	return app;
}
void app_context_cleanup(app_context_t * app)
{
	if(NULL == app) return;
	if(app->cleanup) app->cleanup(app);
	
	if(app->priv) {
		app_private_free(app->priv);
		app->priv = NULL;
	}
	return;
}

shell_context_t * app_context_get_shell(struct app_context * app)
{
	assert(app && app->priv);
	if(NULL == app || NULL == app->priv) return NULL;

	app_private_t * priv = app->priv;
	return priv->shell;
}

trading_agency_t * app_context_get_trading_agency(struct app_context * app, const char * agency_name)
{
	assert(app && app->priv);
	if(NULL == agency_name) return NULL;
	
	app_private_t * priv = app->priv;
	
	for(int i = 0; i < priv->num_agencies; ++i) {
		trading_agency_t * agent = priv->agencies[i];
		assert(agent);
		if(strcasecmp(agency_name, agent->exchange_name) == 0) return agent;
	}
	
	return NULL;
}
