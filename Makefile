TARGET=btc-trader
BIN_DIR=bin
LIB_DIR=lib
DATA_DIR=data
LOG_DIR=log
CONF_DIR=conf

DEBUG ?= 1
OPTIMIZE ?= -O2

CC=gcc -std=gnu99 -D_DEFAULT_SOURCE -D_GNU_SOURCE
LINKER=$(CC)

CFLAGS = -Wall -Iinclude -Iutils
LIBS = -lm -lpthread -ljson-c -lcurl -ldb

ifeq ($(DEBUG),1)
CFLAGS += -g -D_DEBUG
OPTIMIZE = -O0
endif


#~ CFLAGS += $(shell pkg-config --cflags glib-2.0)
#~ LIBS += $(shell pkg-config --libs glib-2.0)

CFLAGS += $(shell pkg-config --cflags gnutls)
LIBS += $(shell pkg-config --libs gnutls)

CFLAGS += $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.0)
LIBS += $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.0)

SRC_DIR = src
OBJ_DIR = obj
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

UTILS_SRC_DIR = utils
UTILS_OBJ_DIR = obj/utils
UTILS_SOURCES := $(wildcard $(UTILS_SRC_DIR)/*.c)
UTILS_OBJECTS := $(UTILS_SOURCES:$(UTILS_SRC_DIR)/%.c=$(UTILS_OBJ_DIR)/%.o)

TRADING_AGENCIES_SRC_DIR := src/trading_agencies
TRADING_AGENCIES_OBJ_DIR := obj/trading_agencies
TRADING_AGENCIES_SOURCES := $(wildcard $(TRADING_AGENCIES_SRC_DIR)/*.c)
TRADING_AGENCIES_OBJECTS := $(TRADING_AGENCIES_SOURCES:$(TRADING_AGENCIES_SRC_DIR)/%.c=$(TRADING_AGENCIES_OBJ_DIR)/%.o)

CRYPTO_SRC_DIR = utils/crypto
CRYPTO_OBJ_DIR = obj/utils/crypto
CRYPTO_SOURCES := $(wildcard $(CRYPTO_SRC_DIR)/*.c)
CRYPTO_OBJECTS := $(CRYPTO_SOURCES:$(CRYPTO_SRC_DIR)/%.c=$(CRYPTO_OBJ_DIR)/%.o)

GUI_COMPONENTS_SRC_DIR = src/gui
GUI_COMPONENTS_OBJ_DIR = obj/gui
GUI_COMPONENTS_SOURCES := $(wildcard $(GUI_COMPONENTS_SRC_DIR)/*.c)
GUI_COMPONENTS_OBJECTS := $(GUI_COMPONENTS_SOURCES:$(GUI_COMPONENTS_SRC_DIR)/%.c=$(GUI_COMPONENTS_OBJ_DIR)/%.o)

all: do_init $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJECTS) $(UTILS_OBJECTS) $(TRADING_AGENCIES_OBJECTS) $(CRYPTO_OBJECTS) $(GUI_COMPONENTS_OBJECTS)
	$(LINKER) $(CFLAGS) $(OPTIMIZE) -o $@ $^ $(LIBS)
	
$(OBJECTS): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

$(UTILS_OBJECTS): $(UTILS_OBJ_DIR)/%.o : $(UTILS_SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

$(TRADING_AGENCIES_OBJECTS): $(TRADING_AGENCIES_OBJ_DIR)/%.o : $(TRADING_AGENCIES_SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

$(CRYPTO_OBJECTS): $(CRYPTO_OBJ_DIR)/%.o : $(CRYPTO_SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

$(GUI_COMPONENTS_OBJECTS): $(GUI_COMPONENTS_OBJ_DIR)/%.o : $(GUI_COMPONENTS_SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: do_init clean
do_init:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR) $(UTILS_OBJ_DIR) $(DATA_DIR) $(LOG_DIR) $(CONF_DIR)
	mkdir -p $(TRADING_AGENCIES_OBJ_DIR) $(CRYPTO_OBJ_DIR) $(GUI_COMPONENTS_OBJ_DIR)
	
clean:
	rm -f $(BIN_DIR)/$(TARGET) $(OBJECTS) $(UTILS_OBJECTS) 
	rm -f $(GUI_COMPONENTS_OBJECTS) $(TRADING_AGENCIES_OBJECTS) $(CRYPTO_OBJECTS)
