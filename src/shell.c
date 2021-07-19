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
	GtkWidget * main_panel;
	
	
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

static int shell_run(struct shell_context * shell)
{
	assert(shell && shell->priv);
	shell_private_t * priv = shell->priv;
	
	if(shell->is_running || shell->quit) return -1;
	
	shell->is_running = 1;
	
	GtkWidget * window = priv->window;
	gtk_widget_show_all(window);
	gtk_main();
	
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

static void init_windows(shell_private_t * priv)
{
	assert(priv && priv->shell);
	shell_context_t * shell = priv->shell;
	
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget * grid = gtk_grid_new();
	GtkWidget * header_bar = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "btc-trader");
	
	GtkWidget * hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	GtkWidget * scrolled_win = NULL;
	GtkWidget * left_panel = NULL;
	//~ GtkWidget * info_panel = NULL;
	GtkWidget * main_panel = NULL;
	
	left_panel = gtk_tree_view_new();
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_win), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(scrolled_win), left_panel);
	gtk_widget_set_size_request(scrolled_win, 120, -1);
	gtk_paned_add1(GTK_PANED(hpaned), scrolled_win);
	
	main_panel = gtk_tree_view_new();
	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_win), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(scrolled_win), main_panel);
	gtk_widget_set_size_request(scrolled_win, 120, 300);
	gtk_widget_set_hexpand(scrolled_win, TRUE);
	gtk_widget_set_vexpand(scrolled_win, TRUE);
	gtk_paned_add2(GTK_PANED(hpaned), scrolled_win);
	
	gtk_grid_attach(GTK_GRID(grid), hpaned, 0, 1, 3, 1);
	GtkWidget * statusbar = gtk_statusbar_new();
	gtk_widget_set_margin_top(statusbar, 1);
	gtk_widget_set_margin_bottom(statusbar, 1);
	gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), statusbar, FALSE, TRUE, 0);
	
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(window), 3);
	
	gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
	
	priv->window = window;
	priv->header_bar = header_bar;
	priv->grid = grid;
	priv->statusbar = statusbar;
	priv->left_panel = left_panel;
	priv->main_panel = main_panel;
	
	g_signal_connect_swapped(window, "destroy", G_CALLBACK(shell_stop), shell);
	return;
}
