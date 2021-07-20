#!/bin/bash

gcc -std=gnu99 -D_GNU_SOURCE -D_DEFAULT_SOURCE -g -Wall \
    -I../include -I../utils \
    -o coincheck-wss coincheck-wss.c \
    $(pkg-config --cflags --libs libsoup-2.4) \
    -lm -lpthread -ljson-c -lcurl
