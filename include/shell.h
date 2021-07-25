#ifndef BTC_TRADER_SHELL_H_
#define BTC_TRADER_SHELL_H_

#include <stdio.h>
#include <json-c/json.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>

typedef struct shell_context
{
	void * priv;
	void * user_data;
	
	int quit;
	int is_running;
	
	int action_state; // 0. update info manually; 1: automatically
	
	GtkWidget * info_area;
	GtkWidget * info_bar;
	GtkWidget * message_label;
	
	int (* load_config)(struct shell_context * shell, json_object * jconfig);
	int (* init)(struct shell_context * shell);
	int (* run)(struct shell_context * shell);
	int (* stop)(struct shell_context * shell);
}shell_context_t;

shell_context_t * shell_context_init(shell_context_t * shell, int argc, char ** argv, void * user_data);
void shell_context_cleanup(shell_context_t * shell);

struct app_context;
shell_context_t * app_context_get_shell(struct app_context * app);
#ifdef __cplusplus
}
#endif
#endif
