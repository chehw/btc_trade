#ifndef TRADING_AGENCY_ZAIF_H_
#define TRADING_AGENCY_ZAIF_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include "trading_agency.h"

/****************************************************
 * API Documentation: 
 *   https://zaif-api-document.readthedocs.io/ja/latest
****************************************************/
/* zaif Authentication */
struct curl_slist * zaif_auth_add_headers(struct curl_slist * headers, 
	const char * api_key, const char * api_secret, 
	const char * uri, 
	const char * form_data, ssize_t cb_form_data, /* post_data */
	const struct timespec * timestamp 	/* nonce */
);



/****************************************
 * coincheck public APIs
****************************************/
int zaif_public_get_currency(trading_agency_t * agent, const char * currency, json_object ** p_jresponse);
int zaif_public_get_currency_pair(trading_agency_t * agent, const char * pair, json_object ** p_jresponse);
int zaif_public_get_last_price(trading_agency_t * agent, const char * pair, json_object ** p_jresponse);

#ifdef __cplusplus
}
#endif
#endif
