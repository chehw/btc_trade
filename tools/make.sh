#!/bin/bash

gcc -std=gnu99 -D_GNU_SOURCE -D_DEFAULT_SOURCE -g -Wall \
    -I../include -I../utils \
    -o coincheck-cli coincheck-cli.c \
    ../src/trading_agency.c ../src/trading_agencies/coincheck.c \
    ../src/json-response.c \
    ../utils/utils.c ../utils/auto_buffer.c \
    $(pkg-config --cflags --libs gnutls) \
    -lm -lpthread -ljson-c -lcurl
