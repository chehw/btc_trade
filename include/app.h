#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef debug_printf
#include <stdarg.h>
#ifdef _DEBUG
#define debug_printf(fmt, ...) do { \
		fprintf(stderr, "%s(%d)::" fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
	}while(0)
	
#else
#define debug_printf(fmt, ...) do { } while(0)
#endif
#endif

typedef struct app_context
{
	void * priv;
	void * user_data;
	
	int argc;
	char ** argv;
	
	int (* load_config)(struct app_context * app, const char * conf_file);
	int (* init)(struct app_context * app);
	int (* run)(struct app_context * app);
	int (* stop)(struct app_context * app);
	void (* cleanup)(struct app_context * app);
	
	// private data
	int quit;
	int is_running;
	
}app_context_t;
app_context_t * app_context_init(app_context_t * app, int argc, char ** argv, void * user_data);
void app_context_cleanup(app_context_t * app);

#ifdef __cplusplus
}
#endif
#endif
