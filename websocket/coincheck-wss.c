/*
 * coincheck-wss.c
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

#include <json-c/json.h>
#include <libsoup/soup.h>

static const char * s_connection_state_name[] = 
{
	[SOUP_CONNECTION_NEW] = "SOUP_CONNECTION_NEW", 
	[SOUP_CONNECTION_CONNECTING] = "SOUP_CONNECTION_CONNECTING", 
	[SOUP_CONNECTION_IDLE] = "SOUP_CONNECTION_IDLE", 
	[SOUP_CONNECTION_IN_USE] = "SOUP_CONNECTION_IN_USE", 
	[SOUP_CONNECTION_REMOTE_DISCONNECTED] = "SOUP_CONNECTION_REMOTE_DISCONNECTED", 
	[SOUP_CONNECTION_DISCONNECTED] = "SOUP_CONNECTION_DISCONNECTED", 
};
static const char * connection_state_to_string(SoupConnectionState state)
{
	if(state < SOUP_CONNECTION_NEW || state > SOUP_CONNECTION_DISCONNECTED) return "unknown error";
	return s_connection_state_name[state];
}

typedef struct websocket_client_context
{
	void * priv;
	void * user_data;
	
	SoupConnectionState state;
	SoupSession * session;
	SoupMessage * msg;
	SoupWebsocketConnection * conn;
	
	void (* on_connected)(SoupWebsocketConnection * conn, struct websocket_client_context * wss);
	void (* on_message)(SoupWebsocketConnection * conn, gint type, GBytes * message, struct websocket_client_context * wss);
	void (* on_closed)(SoupWebsocketConnection * conn, struct websocket_client_context * wss);
	void (* on_closing)(SoupWebsocketConnection * conn, struct websocket_client_context * wss);
	void (* on_pong)(SoupWebsocketConnection * conn, GBytes * message, struct websocket_client_context * wss);
	void (* on_error)(SoupWebsocketConnection * conn, GError * gerr, struct websocket_client_context * wss);
}websocket_client_context_t;

static void on_connection_state_changed(GObject * conn, GAsyncResult * result, websocket_client_context_t * wss)
{
	SoupConnectionState new_state = SOUP_CONNECTION_NEW;
	g_object_get(conn, "state", &new_state, NULL);
	fprintf(stderr, "state changed: from %s(%d) to %s(%d)\n", 
		connection_state_to_string(wss->state), wss->state, 
		connection_state_to_string(new_state), new_state);
	wss->state = new_state;
	return;
}
static void on_session_connection_created(SoupSession * session, GObject * conn, websocket_client_context_t * wss)
{
	g_object_get(conn, "state", &wss->state, NULL);
	g_signal_connect(conn, "notify::state", G_CALLBACK(on_connection_state_changed), wss);
}

static void on_connected(SoupSession * session, GAsyncResult * result, websocket_client_context_t * wss)
{
	GError * gerr = NULL;
	SoupWebsocketConnection * conn = soup_session_websocket_connect_finish(session, result, &gerr);
	if(gerr) {
		fprintf(stderr, "[ERROR]: %s(%d): %s\n", __FILE__, __LINE__, gerr->message);
		g_error_free(gerr);
		gerr = NULL;
	}
	
	if(conn) {
		wss->conn = conn;
		if(wss->on_message) g_signal_connect(conn, "message", G_CALLBACK(wss->on_message), wss);
		if(wss->on_closed)  g_signal_connect(conn, "closed", G_CALLBACK(wss->on_closed), wss);
		if(wss->on_closing) g_signal_connect(conn, "closing", G_CALLBACK(wss->on_closing), wss);
		if(wss->on_pong)    g_signal_connect(conn, "pong", G_CALLBACK(wss->on_pong), wss);
		if(wss->on_error)   g_signal_connect(conn, "error", G_CALLBACK(wss->on_error), wss);
	}
	
	if(wss->on_connected) wss->on_connected(conn, wss);
	return;
}

gboolean websocket_client_connect(websocket_client_context_t * wss, const char * uri)
{
	assert(wss && uri);
	
	if(NULL == wss->conn 
		|| wss->state == SOUP_CONNECTION_NEW
		|| wss->state == SOUP_CONNECTION_DISCONNECTED
		|| wss->state == SOUP_CONNECTION_REMOTE_DISCONNECTED)
	{
		SoupSession * session = soup_session_new();
		SoupMessage * msg = soup_message_new(SOUP_METHOD_GET, uri);
		
		g_signal_connect(session, "connection-created", G_CALLBACK(on_session_connection_created), wss);
		soup_session_websocket_connect_async(session, msg, 
			NULL, 
			(char **)NULL,
			NULL, 
			(GAsyncReadyCallback)on_connected, 
			wss);
	}
	return TRUE;
}

static void on_connected_coincheck(SoupWebsocketConnection * conn, websocket_client_context_t * wss);
static void on_message_coincheck(SoupWebsocketConnection * conn, gint type, GBytes * message, websocket_client_context_t * wss);
static void on_closed_coincheck(SoupWebsocketConnection * conn, websocket_client_context_t * wss);
int main(int argc, char **argv)
{
	static const char * coincheck_wss_uri = "wss://ws-api.coincheck.com/";
	
	GMainLoop * loop = NULL;
	gboolean ok = FALSE;
	websocket_client_context_t wss[1];
	memset(wss, 0, sizeof(wss));
	
	wss->on_connected = on_connected_coincheck;
	wss->on_message = on_message_coincheck;
	wss->on_closed = on_closed_coincheck;
	
	ok = websocket_client_connect(wss, coincheck_wss_uri);
	assert(ok);
	
	loop = g_main_loop_new(NULL, FALSE);
	assert(loop);
	g_main_loop_run(loop);
	
	g_main_loop_unref(loop);
	return 0;
}

static void on_connected_coincheck(SoupWebsocketConnection * conn, websocket_client_context_t * wss)
{
	static const char * channels[] = {
		"btc_jpy-orderbook", 
		"btc_jpy-trades",
	};
	
	json_object * jsubscribe = json_object_new_object();
	assert(jsubscribe);
	json_object_object_add(jsubscribe, "type", json_object_new_string("subscribe"));
	
	int num_channels = sizeof(channels) / sizeof(channels[0]);
	for(int i = 0; i < num_channels; ++i) {
		json_object_object_add(jsubscribe, "channel", json_object_new_string(channels[i]));
		fprintf(stderr, "subscribe: %s\n", channels[i]); 
		soup_websocket_connection_send_text(conn, json_object_to_json_string_ext(jsubscribe, JSON_C_TO_STRING_PLAIN));
	}
	json_object_put(jsubscribe);
	return;
}

static void on_message_coincheck(SoupWebsocketConnection * conn, gint type, GBytes * message, websocket_client_context_t * wss)
{
	switch(type) {
	case SOUP_WEBSOCKET_DATA_TEXT:
		break;
	case SOUP_WEBSOCKET_DATA_BINARY:
	default:
		return;
	}
	gsize cb_text = 0;
	const char * text = g_bytes_get_data(message, &cb_text);
	
	fwrite(text, cb_text, 1, stdout);
	fflush(stdout);
	//~ g_bytes_unref(message);
	
	return;
}
static void on_closed_coincheck(SoupWebsocketConnection * conn, websocket_client_context_t * wss)
{
	fprintf(stderr, "%s(%p)\n", __FUNCTION__, conn);
	return;
}
