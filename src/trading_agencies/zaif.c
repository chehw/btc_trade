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

#include "trading_agency.h"
#include "trading_agency_zaif.h"
#include "json-response.h"

/****************************************
 * coincheck public APIs
****************************************/
int zaif_public_get_currency(trading_agency_t * agent, const char * currency, json_object ** p_jresponse)
{
	static const char * end_point = "currencies";
	struct http_json_context * http = agent->http;
	assert(http);
	if(NULL == currency) currency = "all";
	
	char url[PATH_MAX] = "";
	snprintf(url, sizeof(url), "%s/%s/%s", agent->base_url, end_point, currency);
	
	json_object * jresponse = http->get(http, url);
	if(NULL == jresponse) return -1;
	
	if(p_jresponse) *p_jresponse = jresponse;
	
	return 0;
}
