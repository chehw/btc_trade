/*
 * shell.c
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

#include "app.h"
#include "shell.h"
#include <gtk/gtk.h>
#include <json-c/json.h>

#include "gui/coincheck-gui.h"

static int shell_load_config(struct shell_context * shell, json_object * jconfig);
static int shell_init(struct shell_context * shell);
static int shell_run(struct shell_context * shell);
static int shell_stop(struct shell_context * shell);

//~ static pthread_once_t s_once_key = PTHREAD_ONCE_INIT;
static int s_gtk_init_flags = 0;

typedef struct shell_private
{
	shell_context_t * shell;
	json_object * jconfig;
	
	GtkWidget * window;
	GtkWidget * grid;
	GtkWidget * statusbar;
	GtkWidget * header_bar;
	
	GtkWidget * left_panel;
	panel_view_t * main_panel;
	
	
}shell_private_t;
static shell_private_t * shell_private_new(shell_context_t * shell);
static void shell_private_free(shell_private_t * priv);

shell_context_t * shell_context_init(shell_context_t * shell, int argc, char ** argv, void * user_data)
{
	if(!s_gtk_init_flags) {
		s_gtk_init_flags = 1;
		gtk_init(&argc, &argv);
	}
	
	if(NULL == shell) {
		shell = calloc(1, sizeof(*shell));
		assert(shell);
	}
	
	shell->user_data = user_data;
	
	shell_private_t * priv = shell_private_new(shell);
	assert(priv && priv == shell->priv);
	
	shell->load_config = shell_load_config;
	shell->init = shell_init;
	shell->run = shell_run;
	shell->stop = shell_stop;
	
	return shell;
}

void shell_context_cleanup(shell_context_t * shell)
{
	if(NULL == shell) return;
	if(shell->priv) {
		shell_private_free(shell->priv);
		shell->priv = NULL;
	}
	return;
}

static int shell_load_config(struct shell_context * shell, json_object * jconfig)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	
	if(jconfig) {
		if(priv->jconfig) json_object_put(priv->jconfig);	// unref
		priv->jconfig = json_object_get(jconfig); // add_ref
	}
	// ...
	return 0;
}

static void init_windows(shell_private_t * priv);
static int shell_init(struct shell_context * shell)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	json_object * jconfig = priv->jconfig;
	if(jconfig) {
		// ...
	}
	
	init_windows(priv);
	return 0;
}

//~ static gboolean on_shell_timer(shell_context_t * shell)
//~ {
	//~ if(!shell->is_running || shell->quit) return G_SOURCE_REMOVE;
	
	//~ panel_view_t * coincheck_panel = shell_get_main_panel(shell, "coincheck");
	//~ coincheck_panel_update_order_book(coincheck_panel);
	
	//~ return G_SOURCE_CONTINUE;
//~ }

extern gboolean coincheck_check_banlance(shell_context_t * shell);
extern gboolean coincheck_update_order_book(shell_context_t * shell);

static int shell_run(struct shell_context * shell)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	
	if(shell->is_running || shell->quit) return -1;
	
	shell->is_running = 1;
	
	// update order_book every 1s
	g_timeout_add(1000, (GSourceFunc)coincheck_update_order_book, shell);
	
	// check balance every 300s
	g_timeout_add(300 * 1000, (GSourceFunc)coincheck_check_banlance, shell);
	
	GtkWidget * window = priv->window;
	gtk_widget_show_all(window);
	gtk_main();
	
	shell->is_running = 0;
	shell->quit = 1;
	return 0;
}

static int shell_stop(struct shell_context * shell)
{
	debug_printf("%s(%p)", __FUNCTION__, shell);
	
	assert(shell && shell->priv);
	//~ shell_private_t * priv = shell->priv;
	if(shell->quit || !shell->is_running) return -1;
	
	gtk_main_quit();
	shell->is_running = 0;
	
	return 0;
}

static shell_private_t * shell_private_new(shell_context_t * shell)
{
	shell_private_t * priv = calloc(1, sizeof(*priv));
	assert(priv);
	
	priv->shell = shell;
	shell->priv = priv;
	
	// ...
	
	return priv;
}
static void shell_private_free(shell_private_t * priv)
{
	if(NULL == priv) return;
	
	//...
	free(priv);
	return;
}

static gboolean on_toggle_action_state(GtkSwitch *widget, gboolean state, shell_context_t * shell)
{
	shell->action_state = state;
	return FALSE;
}

static void init_windows(shell_private_t * priv)
{
	assert(priv && priv->shell);
	shell_context_t * shell = priv->shell;
	
	GtkBuilder * builder = gtk_builder_new();
	GError * gerr = NULL;
	guint res_id = gtk_builder_add_from_file(builder, "ui/app_window.ui", &gerr);
	if(res_id == 0 || gerr) {
		if(gerr) {
			fprintf(stderr, "gtk_builder_add_from_file() failed: %s\n", gerr->message);
			g_error_free(gerr);
			gerr = NULL;
		}
		exit(1);
	}
	
	GtkWidget * window = GTK_WIDGET(gtk_builder_get_object(builder, "app_window"));
	GtkWidget * header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "btc-trader");
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	
	GtkWidget * info_area = GTK_WIDGET(gtk_builder_get_object(builder, "app_window_info_area"));
	assert(info_area);
	shell->info_area = info_area;
	
	GtkWidget * info_bar = gtk_info_bar_new();
	gtk_widget_set_no_show_all (info_bar, TRUE);
	GtkWidget * message_label = gtk_label_new ("");
	
	GtkWidget * content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_bar));
	gtk_container_add (GTK_CONTAINER (content_area), message_label);
	g_signal_connect(info_bar, "response", G_CALLBACK (gtk_widget_hide), NULL);
	shell->info_bar = info_bar;
	shell->message_label = message_label;
	gtk_grid_attach(GTK_GRID(info_area), info_bar, 0, 0, 2, 1);
	
	GtkWidget * toggle_button = gtk_switch_new();
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), toggle_button);
	g_signal_connect(toggle_button, "state-set", G_CALLBACK(on_toggle_action_state), shell);
	shell->action_state = TRUE;
	gtk_switch_set_active(GTK_SWITCH(toggle_button), shell->action_state);

	
	priv->window = window;
	priv->header_bar = header_bar;
	
	priv->left_panel = GTK_WIDGET(gtk_builder_get_object(builder, "left_panel"));
	
	panel_view_t * main_panel = panel_view_init(NULL, "coincheck", shell);
	assert(main_panel);
	panel_view_load_from_builder(main_panel, builder);
	
	priv->main_panel = main_panel;
	g_signal_connect_swapped(window, "destroy", G_CALLBACK(shell_stop), shell);
	
	return;
}

panel_view_t * shell_get_main_panel(shell_context_t * shell, const char * agency_name)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	if(agency_name == NULL) agency_name = "coincheck";
	
	// todo: ...
	if(strcasecmp(agency_name, "coincheck") == 0) return priv->main_panel;
	
	return NULL;
}
