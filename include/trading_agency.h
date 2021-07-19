#ifndef _TRADING_AGENCY_H_
#define _TRADING_AGENCY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include <limits.h>

#define TRADING_AGENCY_MAX_NAME_LEN (100)
enum trading_agency_credentials_type
{
	trading_agency_credentials_type_query,
	trading_agency_credentials_type_trade,
	trading_agency_credentials_type_withdraw,
};

typedef struct trading_agency
{
	void * priv;
	void * user_data;
	char exchange_name[TRADING_AGENCY_MAX_NAME_LEN];
	char base_url[1024];
	
	int (* load_config)(struct trading_agency * agent, json_object * jconfig);
	int (* init)(struct trading_agency * agent);
	int (* run)(struct trading_agency * agent);
	int (* stop)(struct trading_agency * agent);
	void (* cleanup)(struct trading_agency * agent);
	
	int (* get_credentials)(struct trading_agency * agent, 
		enum trading_agency_credentials_type type, 
		const char ** p_api_key, const char ** p_api_secret);
	
	/*****************************
	 * virtual functions
	*****************************/
	// public api
	int (* query)(struct trading_agency * agent, 
		const char * method,  // [ "GET", "POST", ... ]
		const char * command, json_object * jparams, 
		json_object ** p_jresult);
	
	// private api
	int (* execute)(struct trading_agency * agent, 
		const char * method, // [ "GET", "POST", ... ]
		const char * command, json_object * jparams, 
		json_object ** p_jresult); 
}trading_agency_t;
trading_agency_t * trading_agency_new(const char * agency_name, void * user_data);
int trading_agency_class_init(trading_agency_t * agent);
void trading_agency_free(trading_agency_t * agent);

struct app_context;
extern trading_agency_t * app_context_get_trading_agency(struct app_context * app, const char * agency_name);

#ifdef __cplusplus
}
#endif
#endif
