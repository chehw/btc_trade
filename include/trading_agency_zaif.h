#ifndef TRADING_AGENCY_ZAIF_H_
#define TRADING_AGENCY_ZAIF_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include "trading_agency.h"
#include "auto_buffer.h"

/****************************************************
 * API Documentation: 
 *   https://zaif-api-document.readthedocs.io/ja/latest
****************************************************/
/* zaif Authentication */
struct curl_slist * zaif_auth_add_headers(
	struct curl_slist * headers, 
	const char * api_key, const char * api_secret, 
	const char * post_fields, ssize_t length);



/****************************************
 * zaif public APIs
****************************************/
int zaif_public_get_currency(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
int zaif_public_get_currency_pair(trading_agency_t * agent, const char * pair, json_object ** p_jresponse);
int zaif_public_get_last_price(trading_agency_t * agent, const char * pair, json_object ** p_jresponse);


/****************************************
 * zaif trade APIs
****************************************/
int zaif_trade_get_info(trading_agency_t * agent, json_object ** p_jresponse);

#ifdef __cplusplus
}
#endif
#endif
