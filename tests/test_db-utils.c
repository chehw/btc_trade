/*
 * test_db-utils.c
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

#include <db.h>
#include "utils.h"
#include <json-c/json.h>

#include "json-response.h"

struct bank_code_record_data
{
	char name[64];
	char half_width_kana[256];
	char full_width_kana[256];
	char hiragana[256];
};
struct bank_code_record
{
	char code[8];
	union {
		struct bank_code_record_data data[1];
		struct {
			char name[64];
			char half_width_kana[256];
			char full_width_kana[256];
			char hiragana[256];
		};
	};
};
void bank_code_record_dump(const struct bank_code_record * record)
{
	printf("code: %s\n", record->code);
	printf("name: %s\n", record->name);
	printf("half_width_kana: %s\n", record->half_width_kana);
	printf("full_width_kana: %s\n", record->full_width_kana);
	printf("hiragana: %s\n", record->hiragana);
}
int bank_code_record_set(struct bank_code_record * record, json_object * jrecord)
{
	const char * code = json_get_value(jrecord, string, code);
	const char * name = json_get_value(jrecord, string, name);
	const char * half_width_kana = json_get_value(jrecord, string, halfWidthKana);
	const char * full_width_kana = json_get_value(jrecord, string, fullWidthKana);
	const char * hiragana = json_get_value(jrecord, string, hiragana);
	assert(code && name && half_width_kana && full_width_kana && hiragana);
	
	strncpy(record->code, code, sizeof(record->code));
	strncpy(record->name, name, sizeof(record->name));
	strncpy(record->half_width_kana, half_width_kana, sizeof(record->half_width_kana));
	strncpy(record->full_width_kana, full_width_kana, sizeof(record->full_width_kana));
	strncpy(record->hiragana, hiragana, sizeof(record->hiragana));
	return 0;
}

#define BANK_CODES_SDBP_COUNT (4)
typedef struct db_context
{
	DB_ENV * env;
	DB * bank_codes;			// for write
	DB * bank_codes_readonly;	// for read
	union {
		struct {
			DB * bank_codes_by_name;
			DB * bank_codes_by_half_width_kana;
			DB * bank_codes_by_full_width_kana;
			DB * bank_codes_by_hiragana;
		};
		DB * sdbps[BANK_CODES_SDBP_COUNT];
	};
	
	struct {
		DB * tickers;
	} coincheck;
}db_context_t;
db_context_t * db_context_init(db_context_t * db, const char * db_home, void * user_data);
void db_context_cleanup(db_context_t * db);

#define exit_on_failure(rc) do { \
		if(0 == rc) break; \
		fprintf(stderr, "[ERROR]: %s\n", db_strerror(rc)); \
		exit(1); \
	}while(0)

static int load_bank_codes(db_context_t * db, struct http_json_context * http);
static void run_tests(db_context_t * db);
int main(int argc, char **argv)
{
	const char * db_home = "data";
	curl_global_init(CURL_GLOBAL_ALL);
	
	struct http_json_context http[1];
	memset(http, 0, sizeof(http));
	http_json_context_init(http, NULL);
	
	db_context_t * db = db_context_init(NULL, db_home, NULL);
	assert(db);
	
	load_bank_codes(db, http);
	
	run_tests(db);
	
	db_context_cleanup(db);
	free(db);
	
	http_json_context_cleanup(http);
	curl_global_cleanup();
	return 0;
}


/**
[
    {
      "code": "0000",
      "name": "日本銀行",
      "halfWidthKana": "ﾆﾂﾎﾟﾝｷﾞﾝｺｳ",
      "fullWidthKana": "ニツポンギンコウ",
      "hiragana": "につぽんぎんこう"
    },
    {
      "code": "0001",
      "name": "みずほ銀行",
      "halfWidthKana": "ﾐｽﾞﾎｷﾞﾝｺｳ",
      "fullWidthKana": "ミズホギンコウ",
      "hiragana": "みずほぎんこう"
    },
    {
      "code": "0005",
      "name": "三菱ＵＦＪ銀行",
      "halfWidthKana": "ﾐﾂﾋﾞｼﾕ-ｴﾌｼﾞｴｲｷﾞﾝｺｳ",
      "fullWidthKana": "ミツビシユ－エフジエイギンコウ",
      "hiragana": "みつびしゆ－えふじえいぎんこう"
    },
    {
      "code": "0009",
      "name": "三井住友銀行",
      "halfWidthKana": "ﾐﾂｲｽﾐﾄﾓｷﾞﾝｺｳ",
      "fullWidthKana": "ミツイスミトモギンコウ",
      "hiragana": "みついすみともぎんこう"
    },
    {
      "code": "0010",
      "name": "りそな銀行",
      "halfWidthKana": "ﾘｿﾅｷﾞﾝｺｳ",
      "fullWidthKana": "リソナギンコウ",
      "hiragana": "りそなぎんこう"
    },
    ...
**/
static void run_tests(db_context_t * db)
{
	DB * dbp = db->bank_codes;
	DB * sdbp_by_name = db->bank_codes_by_name;
	DB * sdbp_by_hiragana = db->bank_codes_by_hiragana;
	DB * sdbp_by_half_width_kana = db->bank_codes_by_half_width_kana;
	DB * sdbp_by_full_width_kana = db->bank_codes_by_full_width_kana;
	assert(dbp && sdbp_by_name);
	assert(sdbp_by_hiragana && sdbp_by_half_width_kana && sdbp_by_full_width_kana);
	
	
	DBC * cursorp = NULL;
	int rc = 0;
	
	rc = dbp->cursor(dbp, NULL, &cursorp, DB_READ_COMMITTED);
	exit_on_failure(rc);
	
	
	DBT key, value;
	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
	
	struct bank_code_record record[1];
	memset(record, 0, sizeof(record));
	
	
	
	static const char * prefix = "000";
	int cb_prefix = strlen(prefix);
	
	strncpy(record->code, prefix, sizeof(record->code));
	key.data = record->code;
	key.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
	key.dlen = cb_prefix;
	key.ulen = sizeof(record->code);
	
	value.data = record->data;
	value.flags = DB_DBT_USERMEM;
	value.ulen = sizeof(record->data);
	
	
	rc = cursorp->get(cursorp, &key, &value, DB_SET_RANGE); // move to the first record starting with the prefix
	exit_on_failure(rc);
	
	while(0 == rc){
		if(memcmp(record->code, prefix, cb_prefix) != 0) break;	
		bank_code_record_dump(record);
		rc = cursorp->get(cursorp, &key, &value, DB_NEXT);
	}
	if(!rc && rc != DB_NOTFOUND) {
		fprintf(stderr, "ret=%d, err_msg: %s\n", rc, db_strerror(rc));
	}
	cursorp->close(cursorp);
	return;
}

static int bank_codes_associate_by_name(DB * sdbp, const DBT *key, const DBT *value, DBT * skey)
{
	struct bank_code_record_data * record = value->data;
	skey->data = record->name;
	skey->size = strlen(record->name) + 1;
	return 0;
}
static int bank_codes_associate_by_half_width_kana(DB * sdbp, const DBT *key, const DBT *value, DBT * skey)
{
	struct bank_code_record_data * record = value->data;
	skey->data = record->half_width_kana;
	skey->size = strlen(record->half_width_kana) + 1;
	return 0;
}
static int bank_codes_associate_by_full_width_kana(DB * sdbp, const DBT *key, const DBT *value, DBT * skey)
{
	struct bank_code_record_data * record = value->data;
	skey->data = record->half_width_kana;
	skey->size = strlen(record->full_width_kana) + 1;
	return 0;
}
static int bank_codes_associate_by_hiragana(DB * sdbp, const DBT *key, const DBT *value, DBT * skey)
{
	struct bank_code_record_data * record = value->data;
	skey->data = record->half_width_kana;
	skey->size = strlen(record->hiragana) + 1;
	return 0;
}

db_context_t * db_context_init(db_context_t * db, const char * db_home, void * user_data)
{
	if(NULL == db) {
		db = calloc(1, sizeof(*db));
		assert(db);
	}
	
	DB_ENV * env = NULL;
	DB * dbp = NULL;
	DB * sdbp = NULL;
	int rc = 0;
	
	rc = db_env_create(&env, 0);
	exit_on_failure(rc);
	db->env = env;
	
	u_int32_t env_flags = DB_CREATE | DB_INIT_MPOOL
		| DB_INIT_LOCK
		| DB_INIT_TXN
		| DB_INIT_LOG
		| DB_INIT_REP
		| DB_RECOVER
		| DB_REGISTER
		| DB_THREAD
		| 0;
	u_int32_t db_flags = DB_CREATE 
		| DB_THREAD
		| DB_READ_UNCOMMITTED
		| DB_AUTO_COMMIT
		| 0;
	u_int32_t sdb_flags = DB_CREATE
		//~ | DB_IMMUTABLE_KEY
		| 0;
	rc = env->open(env, db_home, env_flags, 0);
	exit_on_failure(rc);
	
	// create db
	rc = db_create(&dbp, env, 0);
	exit_on_failure(rc);
	
	rc = dbp->open(dbp, NULL, "bank_codes.db", NULL, DB_BTREE, db_flags, 0); 
	exit_on_failure(rc);
	db->bank_codes = dbp;
	
	// create index db - sorted by names
	rc = db_create(&sdbp, env, 0);
	exit_on_failure(rc);
	sdbp->set_flags(sdbp, DB_DUPSORT);
	//~ sdbp->set_bt_compare(sdbp, partial_string_compare);
	rc = sdbp->open(sdbp, NULL, "bank_codes_by_name.db", NULL, DB_BTREE, db_flags, 0);
	exit_on_failure	(rc);
	rc = dbp->associate(dbp, NULL, sdbp, bank_codes_associate_by_name, sdb_flags);
	db->bank_codes_by_name = sdbp;
	sdbp = NULL;
	
	// create index db - sorted by half_width_kana 
	rc = db_create(&sdbp, env, 0);
	exit_on_failure(rc);
	sdbp->set_flags(sdbp, DB_DUPSORT);
	//~ sdbp->set_bt_compare(sdbp, partial_string_compare);
	rc = sdbp->open(sdbp, NULL, "bank_codes_by_half_width_kana.db", NULL, DB_BTREE, db_flags, 0);
	exit_on_failure	(rc);
	rc = dbp->associate(dbp, NULL, sdbp, bank_codes_associate_by_half_width_kana, sdb_flags);
	db->bank_codes_by_half_width_kana = sdbp;
	sdbp = NULL;
	
	// create index db - sorted by full_width_kana 
	rc = db_create(&sdbp, env, 0);
	exit_on_failure(rc);
	sdbp->set_flags(sdbp, DB_DUPSORT);
	//~ sdbp->set_bt_compare(sdbp, partial_string_compare);
	rc = sdbp->open(sdbp, NULL, "bank_codes_by_full_width_kana.db", NULL, DB_BTREE, db_flags, 0);
	exit_on_failure	(rc);
	rc = dbp->associate(dbp, NULL, sdbp, bank_codes_associate_by_full_width_kana, sdb_flags);
	db->bank_codes_by_full_width_kana = sdbp;
	sdbp = NULL;
	
	// create index db - sorted by hiragana
	rc = db_create(&sdbp, env, 0);
	exit_on_failure(rc);
	sdbp->set_flags(sdbp, DB_DUPSORT);
	//~ sdbp->set_bt_compare(sdbp, partial_string_compare);
	rc = sdbp->open(sdbp, NULL, "bank_codes_by_hiragana.db", NULL, DB_BTREE, db_flags, 0);
	exit_on_failure	(rc);
	rc = dbp->associate(dbp, NULL, sdbp, bank_codes_associate_by_hiragana, sdb_flags);
	db->bank_codes_by_hiragana = sdbp;
	sdbp = NULL;
	
	return db;
}
void db_context_cleanup(db_context_t * db)
{
	if(NULL == db) return;
	if(db->env) {
		for(int i = 0; i < BANK_CODES_SDBP_COUNT; ++i) {
			if(db->sdbps[i]) db->sdbps[i]->close(db->sdbps[i], 0);
			db->sdbps[i] = NULL;
		}
		if(db->bank_codes) db->bank_codes->close(db->bank_codes, 0);
		db->bank_codes = NULL;
		
		db->env->close(db->env, 0);
		db->env = NULL;
	}
	return;
}




static int load_bank_codes(db_context_t * db, struct http_json_context * http)
{
	static const char * get_banklist_url = "https://apis.bankcode-jp.com/v1/banks?limit=2000";
	
	json_object * jresponse = http->get(http, get_banklist_url);
	assert(jresponse);
	json_object * jbanks_list = NULL;
	json_bool ok = json_object_object_get_ex(jresponse, "data", &jbanks_list);
	assert(ok && jbanks_list);
	
	int num_banks = json_object_array_length(jbanks_list);
	assert(num_banks > 0);
	
	int rc = 0;
	static int overwritable = 1;
	
	DB * dbp = db->bank_codes;
	assert(dbp);
	
	printf("num banks: %d\n", num_banks);
	for(int i = 0; i < num_banks; ++i) 
	{
		json_object * jbank = json_object_array_get_idx(jbanks_list, i);
		assert(jbank);
		struct bank_code_record record[1];
		memset(record, 0, sizeof(record));
		
		rc = bank_code_record_set(record, jbank);
		
		DBT key, value;
		memset(&key, 0, sizeof(key));
		memset(&value, 0, sizeof(value));
		key.data = record->code;
		key.size = strlen(record->code) + 1;
		value.data = record->data;
		value.size = sizeof(record->data);
		
		rc = dbp->put(dbp, NULL, &key, &value, overwritable?0:DB_NOOVERWRITE);
		if(rc && rc != DB_KEYEXIST) {
			fprintf(stderr, "[ERROR]: %s\n", db_strerror(rc));
			break;
		}
	}
	json_object_put(jresponse);
	return rc;
}
