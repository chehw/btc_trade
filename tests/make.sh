#!/bin/bash
TARGET=${1-"all"}

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
			utils/utils.c utils/auto_buffer.c \
			utils/crypto/hmac256.c utils/crypto/sha256.c \
			-lm -lpthread -ljson-c -lcurl
		;;
	*)
		;;
esac

