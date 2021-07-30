#!/bin/bash
TARGET=${1-"test_crypto"}

LINKER="gcc -std=gnu99 -g -D_DEBUG -Wall -Iinclude -Iutils -Itests -D_DEFAULT_SOURCE -D_GNU_SOURCE " 

if [[ "$TARGET" == "all" ]] ; then
    echo "build all --> build nothing"
    exit 1
fi

TARGET=$(basename "$TARGET")
TARGET=${TARGET/.[ch]/}

case "$TARGET" in
	test_coincheck_api)
		${LINKER} -o tests/test_coincheck_api \
			tests/test_coincheck_api.c \
			src/trading_agency.c src/trading_agencies/coincheck.c \
			src/json-response.c \
			utils/utils.c utils/auto_buffer.c \
			$(pkg-config --cflags --libs gnutls) \
			-lm -lpthread -ljson-c -lcurl
		;;
	test_zaif_api)
		${LINKER} -o tests/test_zaif_api \
			tests/test_zaif_api.c \
			src/trading_agency.c src/trading_agencies/zaif.c \
			src/json-response.c \
			utils/utils.c utils/auto_buffer.c \
			$(pkg-config --cflags --libs gnutls) \
			-lm -lpthread -ljson-c -lcurl
		;;
	test_crypto|test_urlencode)
		${LINKER} -o tests/test_urlencode \
			tests/test_urlencode.c \
			utils/utils.c utils/auto_buffer.c \
			$(pkg-config --cflags --libs gnutls) \
			-lm -lpthread -ljson-c -lcurl
		;;
		
	test-spin-button)
		${LINKER} -o tests/${TARGET} \
			tests/gui/${TARGET}.c \
			$(pkg-config --cflags --libs gtk+-3.0) \
			-lm -lpthread -ljson-c -lcurl
		;;
		
	test-bank_accounts)
		${LINKER} -o tests/${TARGET} \
			tests/gui/${TARGET}.c \
			src/json-response.c \
			utils/utils.c utils/auto_buffer.c \
			$(pkg-config --cflags --libs gtk+-3.0) \
			-lm -lpthread -ljson-c -lcurl
		;;
	*)
		echo "not found"
		exit 1
		;;
esac

