/*
 * sha_vs_glib-sha.c
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

#include <glib.h>
#include "utils.h"

#include <sys/time.h>
#include <sys/resource.h>

#include <stdint.h>
#include <inttypes.h>

#include <unistd.h>

#include "crypto/sha.h"

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

/**
 * build:
 * 
gcc -std=gnu99 -Wall -O3 -o sha_glib_vs_gnutls sha_glib_vs_gnutls.c \
     -I../include -I../utils \
     ../utils/utils.c ../utils/crypto/sha256.c \
     -lm -lpthread -ljson-c -lcurl \
      $(pkg-config --cflags --libs glib-2.0 gnutls)
 *
*/

struct resource_timer
{
	app_timer_t timer[1];
	
	int who; /* RUSAGE_SELF(0), RUSAGE_CHILDREN(-1), RUSAGE_THREAD(1) */
	struct rusage begin;
	struct rusage current;
	
	// micro-seconds
	struct{
	int64_t usertime_begin;
	int64_t systime_begin;
	};

	struct {
	int64_t systime_current;
	int64_t usertime_current;
	};
	
	int flags;
};
void resource_timer_begin(struct resource_timer * rt, int who)
{
	memset(rt, 0, sizeof(*rt));

	rt->who = who;
	app_timer_start(rt->timer);
	
	struct rusage * usuage = &rt->begin;
	getrusage(rt->who, usuage);
	rt->usertime_begin = (int64_t)usuage->ru_utime.tv_sec * 1000000 + (int64_t)usuage->ru_utime.tv_usec;
	rt->systime_begin = (int64_t)usuage->ru_stime.tv_sec * 1000000 + (int64_t)usuage->ru_stime.tv_usec;
	
	rt->flags = 1;
	return;
}
void resource_timer_get_status(struct resource_timer * rt)
{
	struct rusage * usuage = &rt->current;
	getrusage(rt->who, usuage);
	rt->usertime_current = (int64_t)usuage->ru_utime.tv_sec * 1000000 + (int64_t)usuage->ru_utime.tv_usec;
	rt->systime_current = (int64_t)usuage->ru_stime.tv_sec * 1000000 + (int64_t)usuage->ru_stime.tv_usec;
	app_timer_get_elapsed(rt->timer);
}

void dump_usuage(struct resource_timer * rt, const char * title, int show_details)
{
	if(!rt->flags) resource_timer_begin(rt, rt->who);
	resource_timer_get_status(rt);
	
	app_timer_t * timer = rt->timer;
	double time_elapsed = timer->end - timer->begin;
	
	struct rusage * current = &rt->current;
	fprintf(stderr, "========== usuages: %s ===========\n", title);
	fprintf(stderr, "time elapsed: %f ms\n", time_elapsed * 1000.0);
	
	fprintf(stderr, "user time: %" PRIi64 " ms" "\n", rt->usertime_current - rt->usertime_begin);
	fprintf(stderr, "sys  time: %" PRIi64 " ms" "\n", rt->systime_current - rt->systime_begin);
	
	if(show_details) 
	{ 
		fprintf(stderr, "ru_maxrss: %ld\n", current->ru_ixrss); /* RAM 上に存在する仮想ページのサイズ (resident set size) の最大値 */
		fprintf(stderr, "ru_ixrss : %ld\n", current->ru_ixrss); /* 共有メモリーの合計サイズ */
		fprintf(stderr, "ru_idrss : %ld\n", current->ru_ixrss);  /* 非共有データの合計サイズ */
		
		fprintf(stderr, "ru_isrss : %ld\n", current->ru_isrss); /* 非共有スタックの合計サイズ */
		fprintf(stderr, "ru_minflt: %ld\n", current->ru_minflt); /* ページの再利用 (ソフトページフォルト) */
		fprintf(stderr, "ru_majflt: %ld\n", current->ru_majflt); /* ページフォールト (ハードページフォルト) */
		fprintf(stderr, "ru_nswap : %ld\n", current->ru_nswap); /* スワップ */
		fprintf(stderr, "ru_inblock: %ld\n", current->ru_inblock); /* ブロック入力操作 */
		fprintf(stderr, "ru_oublock: %ld\n", current->ru_oublock); /* ブロック出力操作 */
		fprintf(stderr, "ru_msgsnd: %ld\n", current->ru_msgsnd); /* 送信された IPC メッセージ */
		fprintf(stderr, "ru_msgrcv: %ld\n", current->ru_msgrcv); /* 受信された IPC メッセージ */
		fprintf(stderr, "ru_nsignals: %ld\n", current->ru_nsignals); /* 受信されたシグナル */
		fprintf(stderr, "ru_nvcsw : %ld\n", current->ru_nvcsw); /* 意図したコンテキスト切り替え */
		fprintf(stderr, "ru_nivcsw: %ld\n", current->ru_nivcsw); /* 意図しないコンテキスト切り替え */ 
	}
};

#define ROUNDS 10
#define BATCH_SIZE (10000)
static unsigned char s_message_buf[1024];

void test_glib_sha(void)
{
	unsigned char hash[64];
	GChecksum * sha = g_checksum_new(G_CHECKSUM_SHA256);
	
	struct resource_timer rt[1];
	resource_timer_begin(rt, 0);
	
	for(int i = 0; i < ROUNDS; ++i) {
		for(int j = 0; j < BATCH_SIZE; ++j) {
			gsize size = 32;
			g_checksum_update(sha, s_message_buf, sizeof(s_message_buf));
			g_checksum_get_digest(sha, hash, &size);
			g_checksum_reset(sha);
		}
	}
	
	dump_usuage(rt, __FUNCTION__, 0);
	dump_line("hash: ", hash, 32);
	return;
}

void test_sha(void)
{
	unsigned char hash[64];
	sha256_ctx_t sha[1];
	
	struct resource_timer rt[1];
	resource_timer_begin(rt, 0);
	
	for(int i = 0; i < ROUNDS; ++i) {
		for(int j = 0; j < BATCH_SIZE; ++j) {
			sha256_init(sha);
			sha256_update(sha, s_message_buf, sizeof(s_message_buf));
			sha256_final(sha, hash);
		}
	}
	
	dump_usuage(rt, __FUNCTION__, 0);
	dump_line("hash: ", hash, 32);
	return;
}

void test_gnutls_sha(void)
{
	unsigned char hash[64];
	gnutls_hash_hd_t sha;
	
	struct resource_timer rt[1];
	resource_timer_begin(rt, 0);
	
	for(int i = 0; i < ROUNDS; ++i) {
		for(int j = 0; j < BATCH_SIZE; ++j) {
			gnutls_hash_init(&sha, GNUTLS_DIG_SHA256);
			gnutls_hash(sha, s_message_buf, sizeof(s_message_buf));
			gnutls_hash_output(sha, hash);
		}
	}
	
	dump_usuage(rt, __FUNCTION__, 0);
	dump_line("hash: ", hash, 32);
}

int main(int argc, char **argv)
{
	// fill message buffer with random data
	srand(time(NULL));
	for(int i = 0; i < sizeof(s_message_buf); ++i) {
		s_message_buf[i] = rand() % 256;
	}
	test_glib_sha();
	test_sha();
	test_gnutls_sha();
	return 0;
}

