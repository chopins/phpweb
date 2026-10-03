/*
   +----------------------------------------------------------------------+
   | PHPWEB SAPI Module for PHP 8.3+                                      |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "zend.h"
#include "zend_extensions.h"
#include "php_ini.h"
#include "php_globals.h"
#include "php_ini_builder.h"
#include "php_main.h"
#include "php_output.h"
#include "php_variables.h"
#include <pthread.h>
#include "ext/standard/info.h"
#include "ext/standard/php_standard.h"
#include "SAPI.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"
#include "zend_smart_str.h"
#include "zend_stream.h"
#include "zend_portability.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef PHP_WIN32
#include <windows.h>
#include <WebView2.h>
#include <wrl.h>
#else
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <gio/gio.h>
#endif

#define PHPWEB_WEBVIEW_SCHEME "php-webview"
#include "phpweb.h"
#include "php_getopt.h"

static phpweb_sapi_globals *phpweb_global = NULL;

static int phpweb_no_ini = 0;
static char *phpweb_ini_path = NULL;
static HashTable phpweb_ini_entries;

PHPAPI extern char *php_ini_opened_path;
PHPAPI extern char *php_ini_scanned_path;
PHPAPI extern char *php_ini_scanned_files;
static char *php_self = "";

#ifdef PHP_WIN32
static HWND phpweb_hwnd;
static ICoreWebView2Controller *phpweb_controller;
static ICoreWebView2 *phpweb_webview;
#else
static GtkApplication *phpweb_app = NULL;
static GtkWidget *phpweb_window;
static GtkWidget *phpweb_webview;
#endif

static zend_module_entry phpweb_module_entry;
/* SAPI module structure */
static sapi_module_struct phpweb_sapi_module = {
	"phpweb",
	"PHPWEB SAPI",
	phpweb_startup,
	phpweb_shutdown,
	phpweb_activate,
	phpweb_deactivate,
	phpweb_ub_write,
	phpweb_flush,
	NULL,
	NULL,
	php_error,
	NULL,
	NULL,
	phpweb_send_header,
	NULL,
	phpweb_read_cookies,
	NULL,
	phpweb_log_message,
	NULL,
	NULL,
	STANDARD_SAPI_MODULE_PROPERTIES};

static const char HARDCODED_INI[] =
	"implicit_flush=1\n"
	"output_buffering=0\n"
	"max_execution_time=0\n"
	"max_input_time=-1\n";

const opt_struct PHPWEB_OPTIONS[] = {
	{'h', 0, "help"},
	{'c', 0, "php-ini"},
	{'n', 0, "no-php-ini"},
	{'d', 0, "define"},
	{'m', 0, "modules"},
	{'v', 0, "version"},
	{'i', 0, "info"},
	{'f', 1, "file"},
	{'t', 1, "workroot"},
	{'r', 1, "run"},
	{'a', 1, "interactive"},
	{10, 1, "ini"},
	{11, 1, "num"},
	{'-', 0, NULL}};

/* {{{ PHP_MINIT_FUNCTION */
static PHP_MINIT_FUNCTION(phpweb)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
static PHP_MSHUTDOWN_FUNCTION(phpweb)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
static PHP_MINFO_FUNCTION(phpweb)
{
	DISPLAY_INI_ENTRIES();
}
/* }}} */

static int module_name_cmp(Bucket *f, Bucket *s)
{
	return strcasecmp(((zend_module_entry *)Z_PTR(f->val))->name,
					  ((zend_module_entry *)Z_PTR(s->val))->name);
}

static void print_modules(void)
{
	HashTable sorted_registry;
	zend_module_entry *module;

	zend_hash_init(&sorted_registry, 64, NULL, NULL, 1);
	zend_hash_copy(&sorted_registry, &module_registry, NULL);
	zend_hash_sort(&sorted_registry, module_name_cmp, 0);
	ZEND_HASH_MAP_FOREACH_PTR(&sorted_registry, module)
	{
		php_printf("%s\n", module->name);
	}
	ZEND_HASH_FOREACH_END();
	zend_hash_destroy(&sorted_registry);
}

static void print_extension_info(zend_extension *ext)
{
	php_printf("%s\n", ext->name);
}

static int extension_name_cmp(const zend_llist_element **f, const zend_llist_element **s)
{
	zend_extension *fe = (zend_extension *)(*f)->data;
	zend_extension *se = (zend_extension *)(*s)->data;
	return strcmp(fe->name, se->name);
}

static void print_extensions(void)
{
	zend_llist sorted_exts;

	zend_llist_copy(&sorted_exts, &zend_extensions);
	sorted_exts.dtor = NULL;
	zend_llist_sort(&sorted_exts, extension_name_cmp);
	zend_llist_apply(&sorted_exts, (llist_apply_func_t)print_extension_info);
	zend_llist_destroy(&sorted_exts);
}

static void phpweb_execute_set_header(WebKitURISchemeResponse *response)
{
	int code = 200;
	char *status_line = "OK";
	if (SG(sapi_headers).http_response_code)
	{
		code = SG(sapi_headers).http_response_code;
		printf("code %d\n", code);
	}
	if (SG(sapi_headers).http_status_line)
	{
		status_line = SG(sapi_headers).http_status_line;
		printf(status_line);
	}
	if (SG(sapi_headers).mimetype)
	{
		printf(SG(sapi_headers).mimetype);
	}
	webkit_uri_scheme_response_set_status(response, code, status_line);
	webkit_uri_scheme_response_set_content_type(response, "text/html");
	// headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
	// soup_message_headers_append(headers, "key", "value");
	// webkit_uri_scheme_response_set_http_headers(response, headers);
}

static size_t phpweb_ub_write(const char *str, size_t len)
{
	int thread_idx = 0;
	if (phpweb_global->cli_opt_flag == PHPWEB_CLI_OPT_SHOW_HELP)
	{
#ifdef PHP_WRITE_STDOUT
		zend_long ret;

		ret = write(STDOUT_FILENO, str, len);
		if (ret <= 0)
			return 0;
		return ret;
#else
		size_t ret;

		ret = fwrite(str, 1, MIN(leb, 16384), stdout);
		return ret;
#endif
	}
	else
	{
#ifdef ZTS
		phpweb_sapi_thread_ctx *ctx = (phpweb_sapi_thread_ctx *)SG(server_context);
		thread_idx = ctx->thread_idx;
#endif
		printf("thread idx: %d\n", thread_idx);
		phpweb_global->ub_total_len[thread_idx] += len;
		g_memory_input_stream_add_bytes(phpweb_global->ub_stream[thread_idx], g_bytes_new(str, len));
		if (ctx->response_state == PHPWEB_RESPONSE_READY)
		{
			WebKitURISchemeResponse *response = webkit_uri_scheme_response_new(G_INPUT_STREAM(phpweb_global->ub_stream[thread_idx]), -1);
			phpweb_execute_set_header(response);
			webkit_uri_scheme_request_finish_with_response(ctx->request, response);
			ctx->response_state = PHPWEB_RESPONSE_PENDING;
		}
	}
	return len;
}

static void phpweb_flush(void *server_context)
{
	(void)server_context;
}

static void phpweb_send_header(sapi_header_struct *sapi_header, void *server_context)
{
	(void)sapi_header;
	(void)server_context;
}

static char *phpweb_read_cookies(void) { return NULL; }

static void phpweb_log_message(const char *message, int syslog_type_int)
{
	(void)syslog_type_int;

	// smart_str_appends(&phpweb_output_buf, message);
	// smart_str_appendc(&phpweb_output_buf, '\n');
}
static int phpweb_shutdown(sapi_module_struct *sapi_module)
{
	printf("free thread\n");
	if (phpweb_global->threads != NULL)
	{
		tsrm_mutex_lock(phpweb_global->lock);
		phpweb_global->thread_state = PHPWEB_T_STOP;
		pthread_cond_broadcast(&phpweb_global->cond);
		tsrm_mutex_unlock(phpweb_global->lock);
		for (int i = 0; i < phpweb_global->thread_count; i++)
		{
			pthread_join(phpweb_global->threads[i], NULL);
		}
		efree(phpweb_global->threads);
		efree(phpweb_global->ub_stream);
		efree(phpweb_global->ub_total_len);
		tsrm_mutex_free(phpweb_global->lock);
		pthread_cond_destroy(&phpweb_global->cond);
	}
	return php_module_shutdown_wrapper(sapi_module);
}
static int phpweb_startup(sapi_module_struct *sapi_module)
{
	if (phpweb_no_ini)
	{
		sapi_module->php_ini_path_override = NULL;
		sapi_module->php_ini_ignore = 1;
	}
	else if (phpweb_ini_path)
	{
		sapi_module->php_ini_path_override = phpweb_ini_path;
	}
	zend_signal_startup();
	return php_module_startup(sapi_module, &phpweb_module_entry);
}

static int phpweb_activate(void)
{
	int thread_idx = 0;
	if (SG(server_context))
	{
#ifdef ZTS
		thread_idx = ((phpweb_sapi_thread_ctx *)SG(server_context))->thread_idx;
#endif
		phpweb_global->ub_stream[thread_idx] = G_MEMORY_INPUT_STREAM(g_memory_input_stream_new());
	}
	return SUCCESS;
}

static int phpweb_deactivate(void)
{
	int thread_idx = 0;
	if (phpweb_webview)
	{
#ifdef ZTS
		thread_idx = ((phpweb_sapi_thread_ctx *)SG(server_context))->thread_idx;
#endif
		g_input_stream_clear_pending(G_INPUT_STREAM(phpweb_global->ub_stream[thread_idx]));
		if (phpweb_global->cli_opt_flag != PHPWEB_CLI_OPT_URI)
		{
			// ub_bytes = g_input_stream_read_bytes(G_INPUT_STREAM(phpweb_global->ub_stream), phpweb_global->ub_total_len, NULL, NULL);
			// webkit_web_view_load_bytes(WEBKIT_WEB_VIEW(phpweb_webview), ub_bytes, "text/html", NULL, NULL);
			return SUCCESS;
		}
	}
	return SUCCESS;
}

static zend_result phpweb_seek_file_begin(zend_file_handle *file_handle, char *script_file)
{
	FILE *fp = VCWD_FOPEN(script_file, "rb");
	if (!fp)
	{
		fprintf(stderr, "Could not open input file: %s\n", script_file);
		return FAILURE;
	}

	zend_stream_init_fp(file_handle, fp, script_file);
	file_handle->primary_script = 1;
	return SUCCESS;
}

static void *phpweb_execute_thread(void *arg)
{
	WebKitURISchemeRequest *request = NULL;
	php_url *php_url_resource;
	const char *uri;
	// SoupMessageHeaders *headers;
	char realpath[MAXPATHLEN];
	char *code = NULL;
	int iscode = 0;
	int thread_idx = (intptr_t)arg;
#ifdef ZTS
	ts_resource(0);
	phpweb_sapi_thread_ctx *ctx = malloc(sizeof(phpweb_sapi_thread_ctx));
	ctx->thread_idx = thread_idx;
	SG(server_context) = (void *)ctx;
#endif

	while (1)
	{
		phpweb_global->ub_stream[thread_idx] = G_MEMORY_INPUT_STREAM(g_memory_input_stream_new());
		g_input_stream_set_pending(G_INPUT_STREAM(phpweb_global->ub_stream[thread_idx]), NULL);
		tsrm_mutex_lock(phpweb_global->lock);
		if (phpweb_global->thread_state == PHPWEB_T_WAIT)
		{
			pthread_cond_wait(&phpweb_global->cond, phpweb_global->lock);
		}
		if (phpweb_global->thread_state == PHPWEB_T_STOP)
		{
			tsrm_mutex_unlock(phpweb_global->lock);
			printf("unlock and exit thread\n");
			return NULL;
		}

		request = phpweb_global->request;
		ctx->request = request;
		phpweb_global->request = NULL;
		phpweb_global->thread_state = PHPWEB_T_WAIT;
		tsrm_mutex_unlock(phpweb_global->lock);
		uri = webkit_uri_scheme_request_get_uri(request);
		printf("request uri: %s\n", uri);
		php_url_resource = php_url_parse(uri);
		iscode = strcmp(ZSTR_VAL(php_url_resource->host), "file");
		ctx->response_state = PHPWEB_RESPONSE_READY;
		if (!iscode)
		{
			VCWD_REALPATH(ZSTR_VAL(php_url_resource->path), realpath);
			php_url_free(php_url_resource);
			phpweb_execute_php_script(realpath, iscode);
		}
		else
		{
			code = strdup(ZSTR_VAL(php_url_resource->query));
			php_url_free(php_url_resource);
			phpweb_execute_php_script(code, iscode);
		}
		ctx->response_state = PHPWEB_RESPONSE_WAITING;
		printf("response\n");

		phpweb_global->ub_total_len[thread_idx] = 0;
		phpweb_global->ub_stream[thread_idx] = NULL;
	}
#ifdef ZTS
	free(SG(server_context));
	ts_free_thread();
#endif
	return NULL;
}

static void *phpweb_execute_php_script(char *file_or_code, int iscode)
{
	zval retval;
	int status;
	zend_file_handle file_handle;
	zend_call_stack_init();
	virtual_cwd_activate();

	ZVAL_UNDEF(&retval);
	if (php_request_startup() == FAILURE)
	{
		printf("php_request_startup() failure\n");
		goto clear;
	}
	if (iscode)
	{
		if (zend_eval_stringl(file_or_code, strlen(file_or_code), &retval, "phpweb code") == FAILURE)
		{
			printf("eval string error\n");
		}
	}
	else
	{
		printf("Open file %s\n", file_or_code);
		// zend_stream_init_filename(&file_handle, filename);
		phpweb_seek_file_begin(&file_handle, file_or_code);

		CG(skip_shebang) = 1;
		php_self = file_or_code;
		SG(request_info).path_translated = file_or_code;
		zend_try
		{
			status = php_execute_script(&file_handle);
			// execThread = g_thread_new("php-exec", phpweb_execute_script_thread_func, &file_handle);
			// status = GPOINTER_TO_INT(g_thread_join(execThread);
		}
		zend_end_try();
		if (status == FAILURE)
		{
			printf("excute php error\n");
			// smart_str_appends(&phpweb_output_buf, "Error executing script: ");
			// smart_str_appends(&phpweb_output_buf, filename);
			// smart_str_appendc(&phpweb_output_buf, '\n');
		}

		zend_destroy_file_handle(&file_handle);
	}
clear:
	php_output_end_all();
	php_request_shutdown(NULL);
	return NULL;
}

static void phpweb_execute_notify_thread(WebKitURISchemeRequest *request)
{
	pthread_mutex_lock(phpweb_global->lock);
	phpweb_global->request = g_object_ref(request);
	pthread_cond_signal(&phpweb_global->cond);
	pthread_mutex_unlock(phpweb_global->lock);
}

// static void phpweb_show_in_webview(const char *content, const char *base_uri)
// {
// #ifdef PHP_WIN32
//   char *wrapped = NULL;
//   if (base_uri)
//   {
//     spprintf(&wrapped, 0, "<html><head><base href=\"%s\"></head><body>%s</body></html>", base_uri, content);
//     phpweb_webview->lpVtbl->NavigateToString(phpweb_webview, wrapped);
//     efree(wrapped);
//   }
//   else
//   {
//     phpweb_webview->lpVtbl->NavigateToString(phpweb_webview, content);
//   }
// #else
//   webkit_web_view_load_html(WEBKIT_WEB_VIEW(phpweb_webview), content, base_uri);
// #endif
// }

// static void phpweb_show_file_in_webview(char *filepath)
// {
// 	const char *ext = strrchr(filepath, '.');

// 	if (ext && (strcasecmp(ext, ".php") == 0))
// 	{
// 		if (phpweb_current_script)
// 			free(phpweb_current_script);
// 		phpweb_current_script = strdup(filepath);
// 		// phpweb_execute_php_script(filepath);

// 		// phpweb_execute_notify_thread(filepath);
// 		//  GThread *exec_thread;
// 		//  GError **gerror = NULL;
// 		//  exec_thread = g_thread_try_new("phpexec", phpweb_execute_php_script, filepath, gerror);
// 		//  g_thread_join(exec_thread);
// 		//  char base_uri[4096];
// 		//  snprintf(base_uri, sizeof(base_uri), "php://exec/%s/", phpweb_docroot ? phpweb_docroot : "");
// 		//  phpweb_show_in_webview(content, NULL);
// 	}
// 	else
// 	{
// #ifdef PHP_WIN32
// 		char *uri = malloc(strlen(filepath) + 8);
// 		sprintf(uri, "file:///%s", filepath);
// 		phpweb_webview->lpVtbl->Navigate(phpweb_webview, uri);
// 		free(uri);
// #else
// 		char *uri = g_filename_to_uri(filepath, NULL, NULL);
// 		webkit_web_view_load_uri(WEBKIT_WEB_VIEW(phpweb_webview), uri);
// 		g_free(uri);
// #endif
// 		if (phpweb_current_script)
// 		{
// 			free(phpweb_current_script);
// 			phpweb_current_script = NULL;
// 		}
// 	}
// }

#ifndef PHP_WIN32
static void on_file_open_complete(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
	GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if (file)
	{
		char *filepath = g_file_get_path(file);
		// phpweb_show_file_in_webview(filepath);
		g_free(filepath);
		g_object_unref(file);
	}
}
#endif

static void phpweb_handle_open_file(void)
{
#ifdef PHP_WIN32
	OPENFILENAMEA ofn;
	char file[2048] = "";
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = phpweb_hwnd;
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file);
	ofn.lpstrFilter = "All Files\0*.*\0PHP Files\0*.php\0HTML Files\0*.html;*.htm\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileNameA(&ofn))
		phpweb_show_file_in_webview(file);
#else
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, "Open File");
	gtk_file_dialog_open(dialog, GTK_WINDOW(phpweb_window), NULL, on_file_open_complete, NULL);
	g_object_unref(dialog);
#endif
}

// static void phpweb_handle_refresh(void)
// {
// 	if (phpweb_current_script)
// 		phpweb_show_file_in_webview(phpweb_current_script);
// }

/* Display helper functions: wrap in request to capture php_printf output */
static void phpweb_show_module_info(void)
{

	if (php_request_startup() == FAILURE)
		return;
	php_printf("<html><body><pre>");
	php_printf("[PHP Modules]\n");
	print_modules();
	php_printf("\n[Zend Modules]\n");
	print_extensions();
	php_printf("</pre></body><html>");
	php_output_end_all();
	php_request_shutdown(NULL);
}

static void phpweb_show_version_info(void)
{

	if (php_request_startup() == FAILURE)
		return;
	php_printf("<html><body><pre>");
	php_print_version(&phpweb_sapi_module);
	php_printf("</pre></body><html>");
	php_request_shutdown(NULL);
}

static void phpweb_show_ini_info(void)
{

	if (php_request_startup() == FAILURE)
		return;
	php_printf("<html><body><pre>");
	zend_printf("Configuration File (php.ini) Path: \"%s\"\n", PHP_CONFIG_FILE_PATH);
	if (php_ini_opened_path)
	{
		zend_printf("Loaded Configuration File:         \"%s\"\n", php_ini_opened_path);
	}
	else
	{
		zend_printf("Loaded Configuration File:         (none)\n");
	}
	if (php_ini_scanned_path)
	{
		zend_printf("Scan for additional .ini files in: \"%s\"\n", php_ini_scanned_path);
	}
	else
	{
		zend_printf("Scan for additional .ini files in: (none)\n");
	}
	zend_printf("Additional .ini files parsed:      %s\n", php_ini_scanned_files ? php_ini_scanned_files : "(none)");
	php_printf("</pre></body><html>");
	php_request_shutdown(NULL);
}

static void phpweb_show_phpinfo(void)
{
	if (php_request_startup() == FAILURE)
	{
		return;
	}

	php_print_info(PHP_INFO_ALL & ~PHP_INFO_CREDITS);
	php_output_end_all();
	php_request_shutdown(NULL);
}

static void phpweb_init_thread_pool(void)
{
	phpweb_global->lock = tsrm_mutex_alloc();
	pthread_cond_init(&phpweb_global->cond, NULL);
	phpweb_global->threads = (pthread_t *)emalloc(sizeof(pthread_t) * phpweb_global->thread_count);

	phpweb_global->ub_total_len = (gsize *)emalloc(phpweb_global->thread_count * sizeof(gsize));
	phpweb_global->ub_stream = (GMemoryInputStream **)emalloc(sizeof(GMemoryInputStream *) * phpweb_global->thread_count);

	if (phpweb_global->threads == NULL)
	{
		return;
	}
	for (int i = 0; i < phpweb_global->thread_count; i++)
	{
		phpweb_global->ub_total_len[i] = 0;

		if (pthread_create(&phpweb_global->threads[i], NULL, phpweb_execute_thread, (void *)(intptr_t)i) != 0)
		{
			return;
		}
	}
}
zend_always_inline static char *phpweb_get_phpweb_usage(void)
{
	return "Usage: phpweb [options] [--] [args...]\n"
		   "  -c <path>       Look for php.ini file in this directory\n"
		   "  -n               No php.ini file will be used\n"
		   "  -d foo[=bar]     Define INI entry foo with value 'bar'\n"
		   "  -z <file>        Load Zend extension <file>\n"
		   "  -m               Show compiled in modules\n"
		   "  -v               Show version number\n"
		   "  --ini            Show configuration file names\n"
		   "  -i               Show PHP info (phpinfo())\n"
		   "  -h               This help (printed to console, no GUI)\n"
		   "  -f <file>        Execute <file>\n"
		   "  -r <code>        Run PHP <code> without using script tags <?..?>\n"
		   "  -B <begin>       PHP code to execute before processing input lines\n"
		   "  -E <end>         PHP code to execute after processing input lines\n"
		   "  -t <docroot>     Specify document root for relative paths\n"
		   "  args...          Arguments passed to script ($argv, $argc)\n";
}

static void phpweb_show_help_info(void)
{

	if (php_request_startup() == FAILURE)
	{
		return;
	}
	if (phpweb_global->cli_opt_flag == PHPWEB_CTX_OPT_SHOW_HELP)
	{
		php_printf("<html><body><pre>");
	}
	php_printf(phpweb_get_phpweb_usage());
	if (phpweb_global->cli_opt_flag == PHPWEB_CTX_OPT_SHOW_HELP)
	{
		php_printf("</pre></body><html>");
	}
	php_request_shutdown(NULL);
}

static void phpweb_quit(void)
{
#ifdef PHP_WIN32
	PostQuitMessage(0);
#else
	gtk_window_close(GTK_WINDOW(phpweb_window));
#endif
}

#ifdef PHP_WIN32
/* Windows stubs would go here, omitted for brevity but must be present */
static int phpweb_win32_init(void) { return SUCCESS; }
static void phpweb_win32_run(void) {}
static void phpweb_webview2_setup(void) {}
#else
/* GTK/Linux implementation */
static void phpweb_gtk_run(void)
{
	printf("gtk run\n");

	phpweb_app = gtk_application_new("org.php.phpweb", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(phpweb_app, "activate", G_CALLBACK(phpweb_gtk_activate_cb), NULL);

	webkit_web_context_register_uri_scheme(webkit_web_context_get_default(),
										   PHPWEB_WEBVIEW_SCHEME, (WebKitURISchemeRequestCallback)phpweb_webview_uri_scheme_cb, NULL, NULL);

	g_application_run(G_APPLICATION(phpweb_app), 0, NULL);
	g_object_unref(phpweb_app);
}

static void phpweb_gtk_activate_cb(GtkApplication *app, gpointer user_data)
{
	/* Register actions here so they are available for context menu */
	phpweb_register_gactions(app);

	phpweb_window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(phpweb_window), "PHPWEB");
	gtk_window_set_default_size(GTK_WINDOW(phpweb_window), 1024, 768);
	GtkWidget *scrolledwindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), 1, 1);

	phpweb_webview = webkit_web_view_new();

	webkit_web_view_load_html(WEBKIT_WEB_VIEW(phpweb_webview), "<html><body>Open PHP file</body></html>", "localhost");

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), phpweb_webview);
	gtk_window_set_child(GTK_WINDOW(phpweb_window), scrolledwindow);

	g_signal_connect(phpweb_webview, "decide-policy", G_CALLBACK(phpweb_navigation_policy_cb), NULL);
	g_signal_connect(phpweb_webview, "context-menu", G_CALLBACK(phpweb_webview_context_menu_cb), NULL);

	gtk_window_present(GTK_WINDOW(phpweb_window));

	/* Defer PHP command-line actions to idle so webview is ready */
	g_idle_add(phpweb_do_cli_cmd, NULL);
}

static void phpweb_set_relative_exec_uri(void)
{
	char resolved_path[MAXPATHLEN];
	char *rel;
	if (IS_ABSOLUTE_PATH(phpweb_global->exec_uri, strlen(phpweb_global->exec_uri)))
	{
		return;
	}
	VCWD_REALPATH(phpweb_global->exec_uri, resolved_path);

	if (strncmp(resolved_path, CWDG(cwd).cwd, CWDG(cwd).cwd_length) != 0)
	{
		printf("cant not pass other path\n");
		return;
	}
	rel = resolved_path + CWDG(cwd).cwd_length + 1;
	if ((*rel - 1) != DEFAULT_SLASH)
	{
		printf("cant not pass other path\n");
		return;
	}

	free(phpweb_global->exec_uri);
	phpweb_global->exec_uri = estrdup(rel);
}

static gboolean phpweb_do_cli_cmd(gpointer user_data)
{
	char *exec_uri = NULL;
	if (phpweb_global->cwd)
	{
		virtual_chdir(phpweb_global->cwd);
	}
	else
	{
		phpweb_global->cwd = CWDG(cwd).cwd;
	}

	phpweb_init_thread_pool();
	switch (phpweb_global->cli_opt_flag)
	{
	case PHPWEB_CLI_OPT_URI:
		if (strlen(phpweb_global->exec_uri) <= CWDG(cwd).cwd_length + 1)
		{
			break;
		}
		phpweb_set_relative_exec_uri();
		spprintf(&exec_uri, 0, PHPWEB_WEBVIEW_SCHEME "://file/%s", phpweb_global->exec_uri);
		webkit_web_view_load_uri(WEBKIT_WEB_VIEW(phpweb_webview), exec_uri);
		efree(exec_uri);
		break;
	case PHPWEB_CLI_OPT_CODE:
		spprintf(&exec_uri, 0, PHPWEB_WEBVIEW_SCHEME "://code/?%s", phpweb_global->exec_uri);
		webkit_web_view_load_uri(WEBKIT_WEB_VIEW(phpweb_webview), exec_uri);
		efree(exec_uri);

		break;
	case PHPWEB_CLI_OPT_SHOW_MODULES:
		phpweb_show_module_info();
		break;
	case PHPWEB_CLI_OPT_SHOW_INFO:
		phpweb_show_phpinfo();
		break;
	case PHPWEB_CLI_OPT_SHOW_VERSION:
		phpweb_show_version_info();
		break;
	case PHPWEB_CLI_OPT_SHOW_INI:
		phpweb_show_ini_info();
		break;
	default:
		break;
	}
	return FALSE; /* run once */
}

static gboolean phpweb_navigation_policy_cb(WebKitWebView *web_view, WebKitPolicyDecision *decision,
											WebKitPolicyDecisionType type, gpointer user_data)
{
	if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
	{
		WebKitNavigationPolicyDecision *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
		WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
		WebKitURIRequest *request = webkit_navigation_action_get_request(action);
		const char *uri = webkit_uri_request_get_uri(request);

		if (uri && g_str_has_prefix(uri, "file://"))
		{
			char *filename = g_filename_from_uri(uri, NULL, NULL);
			if (filename)
			{
				const char *ext = strrchr(filename, '.');
				if (ext && strcasecmp(ext, ".php") == 0)
				{
					webkit_policy_decision_ignore(decision);
					// phpweb_show_file_in_webview(filename);
					g_free(filename);
					return TRUE;
				}
				g_free(filename);
			}
		}
		else if (uri && g_str_has_prefix(uri, PHPWEB_WEBVIEW_SCHEME "://exec/"))
		{
			printf("phpweb_navigation_policy_cb() exec\n");
			webkit_policy_decision_ignore(decision);
			webkit_web_view_load_uri(web_view, uri);
			return TRUE;
		}
	}
	return FALSE;
}

static void phpweb_webview_uri_scheme_cb(WebKitURISchemeRequest *request, gpointer user_data)
{

	const char *uri = webkit_uri_scheme_request_get_uri(request);

	if (g_str_has_prefix(uri, PHPWEB_WEBVIEW_SCHEME))
	{
		phpweb_execute_notify_thread(request);
		// webkit_uri_scheme_request_finish(request, G_INPUT_STREAM(phpweb_global->ub_stream), -1, "text/html");
		// phpweb_global->ub_total_len = 0;
		// phpweb_global->ub_stream = NULL;
		return;
	}
	else
	{
		webkit_uri_scheme_request_finish_error(request, g_error_new_literal(webkit_network_error_quark(), WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL, "unknown-protocol"));
	}
}

static gboolean phpweb_webview_context_menu_cb(WebKitWebView *web_view, WebKitContextMenu *menu,
											   GdkEvent *event, WebKitHitTestResult *hit_test_result, gpointer user_data)
{

	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "open-file"), "Open File...", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "refresh"), "Refresh", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());

	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "php-modules"), "PHP Modules (-m)", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "php-version"), "PHP Version (-v)", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "php-ini"), "Loaded ini (--ini)", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "php-info"), "PHP Info (-i)", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());

	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "help"), "Help", NULL));
	webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());
	webkit_context_menu_append(menu, webkit_context_menu_item_new_from_gaction(
										 g_action_map_lookup_action(G_ACTION_MAP(phpweb_app), "quit"), "Quit", NULL));
	WebKitWebInspector *inspector = webkit_web_view_get_inspector(web_view);
	webkit_web_inspector_show(inspector);

	return FALSE;
}

/* GAction callbacks */
static void phpweb_open_file_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_handle_open_file();
}
static void phpweb_refresh_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	// phpweb_handle_refresh();
}
static void phpweb_modules_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_show_module_info();
}
static void phpweb_version_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_show_version_info();
}
static void phpweb_ini_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_show_ini_info();
}
static void phpweb_info_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_show_phpinfo();
}
static void phpweb_help_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_show_help_info();
}
static void phpweb_quit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	phpweb_quit();
}

static void phpweb_register_gactions(GtkApplication *app)
{
	static GActionEntry entries[] = {
		{"open-file", phpweb_open_file_action, NULL, NULL, NULL, {0}},
		{"refresh", phpweb_refresh_action, NULL, NULL, NULL, {0}},
		{"php-modules", phpweb_modules_action, NULL, NULL, NULL, {0}},
		{"php-version", phpweb_version_action, NULL, NULL, NULL, {0}},
		{"php-ini", phpweb_ini_action, NULL, NULL, NULL, {0}},
		{"php-info", phpweb_info_action, NULL, NULL, NULL, {0}},
		{"help", phpweb_help_action, NULL, NULL, NULL, {0}},
		{"quit", phpweb_quit_action, NULL, NULL, NULL, {0}}};
	g_action_map_add_action_entries(G_ACTION_MAP(app), entries, G_N_ELEMENTS(entries), NULL);
}
#endif

/* Command-line parsing (unchanged) */
static int phpweb_parse_opts(int argc, char *argv[])
{
	int c;
	char *php_optarg = NULL;
	int php_optind = 1;
	char *ini_path_override = NULL;
	struct php_ini_builder ini_builder;
	int optflag = 0;
	char *workdir = NULL;
	char *exec_uri = NULL;

	php_ini_builder_init(&ini_builder);
	while ((c = php_getopt(argc, argv, PHPWEB_OPTIONS, &php_optarg, &php_optind, 1, 2)) != -1)
	{
		switch (c)
		{
		case 'h':
			return PHPWEB_CLI_OPT_SHOW_HELP;
		case 'v':
			return PHPWEB_CLI_OPT_SHOW_VERSION;
		case 'i':
			return PHPWEB_CLI_OPT_SHOW_INFO;
		case 10:
			return PHPWEB_CLI_OPT_SHOW_INI;
		case 'm':
			return PHPWEB_CLI_OPT_SHOW_MODULES;
		case 'n':
			phpweb_sapi_module.php_ini_ignore = 1;
			break;
		case 'a':
			return PHPWEB_CLI_OPT_INTERACTIVE;
		case 'f':
			if (exec_uri)
			{
				fprintf(stdout, "You can use -f or -r only once.\n");
				return PHPWEB_CLI_OPT_ERROR;
			}
			exec_uri = php_optarg;
			optflag = PHPWEB_CLI_OPT_URI;
			break;
		case 'c':
			if (ini_path_override)
			{
				free(ini_path_override);
			}
			ini_path_override = strdup(php_optarg);
			phpweb_sapi_module.php_ini_path_override = ini_path_override;
			break;
		case 'd':
			php_ini_builder_define(&ini_builder, php_optarg);
			break;
		case 'r':
			if (exec_uri)
			{
				fprintf(stdout, "You can use -f or -r only once.\n");
				return PHPWEB_CLI_OPT_ERROR;
			}
			exec_uri = php_optarg;
			optflag = PHPWEB_CLI_OPT_CODE;
			break;
		case 't':
			workdir = php_optarg;
			break;
		case 11:
			char *end = NULL;
			long thread_num = strtol(php_optarg, &end, 10);
			if (*end != '\0')
			{
				fprintf(stdout, "thread number is non-numeric");
				return PHPWEB_CLI_OPT_ERROR;
			}
			if (end == php_optarg)
			{
				break;
			}
			if (thread_num < 1 || thread_num > 1000)
			{
				fprintf(stdout, "thread number must greater 1");
				return PHPWEB_CLI_OPT_ERROR;
			}
			phpweb_global->thread_count = (zend_long)thread_num;
			break;
		default:
			return PHPWEB_CLI_OPT_NO_ARG;
		}
	}
	if (workdir)
	{
		phpweb_global->cwd = strdup(workdir);
	}
	if (argc > optind && !exec_uri && strcmp(argv[php_optind - 1], "--"))
	{
		exec_uri = argv[php_optind];
		php_optind++;
	}

	if (exec_uri)
	{
		phpweb_global->exec_uri = strdup(exec_uri);
	}
	php_ini_builder_prepend_literal(&ini_builder, HARDCODED_INI);
	phpweb_sapi_module.ini_entries = php_ini_builder_finish(&ini_builder);
	phpweb_sapi_module.php_ini_ignore_cwd = 1;
	phpweb_sapi_module.executable_location = argv[0];

	return optflag;
}

static void phpweb_apply_ini_entries(void)
{
	zval *entry;
	ZEND_HASH_FOREACH_VAL(&phpweb_ini_entries, entry)
	{
		if (Z_TYPE_P(entry) == IS_STRING)
		{
			char *buf = estrdup(Z_STRVAL_P(entry));
			char *val = strchr(buf, '=');
			if (val)
			{
				*val++ = '\0';
				zend_string *name = zend_string_init(buf, strlen(buf), 0);
				zend_string *new_val = zend_string_init(val, strlen(val), 0);
				zend_alter_ini_entry(name, new_val, PHP_INI_SYSTEM, PHP_INI_STAGE_STARTUP);
				zend_string_release(name);
				zend_string_release(new_val);
			}
			efree(buf);
		}
	}
	ZEND_HASH_FOREACH_END();
}

static zend_module_entry phpweb_module_entry = {
	STANDARD_MODULE_HEADER,
	"phpweb",
	NULL,
	PHP_MINIT(phpweb),
	PHP_MSHUTDOWN(phpweb),
	NULL,
	NULL,
	PHP_MINFO(phpweb),
	PHP_VERSION,
	STANDARD_MODULE_PROPERTIES};

int main(int argc, char *argv[])
{
	int exit_status = 0;

	phpweb_init_globals();
	int optflag = phpweb_parse_opts(argc, argv);

	if (optflag == PHPWEB_CLI_OPT_ERROR)
	{
		phpweb_free_globals();
		return 1;
	}

	phpweb_global->cli_opt_flag = optflag;
#ifdef ZTS
	php_tsrm_startup_ex(phpweb_global->thread_count);
#ifdef PHP_WIN32
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
#endif

	sapi_startup(&phpweb_sapi_module);
	phpweb_apply_ini_entries();
	phpweb_sapi_thread_ctx *ctx = malloc(sizeof(phpweb_sapi_thread_ctx));
	ctx->thread_idx = 0;
	SG(server_context) = (void *)ctx;
	if (phpweb_sapi_module.startup(&phpweb_sapi_module) == FAILURE)
	{
		phpweb_free_globals();
		sapi_shutdown();
		return 1;
	}

	if (optflag == PHPWEB_CLI_OPT_SHOW_HELP)
	{
		phpweb_show_help_info();
	}
	else
	{

#ifdef PHP_WIN32
		if (FAILURE == phpweb_win32_init())
		{
			php_module_shutdown();
			sapi_shutdown();
			return 1;
		}
		phpweb_webview2_setup();

		phpweb_win32_run();
#else
		phpweb_gtk_run(); /* Actions registered in activate, pending executed via idle */
#endif
	}
#ifdef ZTS
	free(SG(server_context));
#endif
	php_module_shutdown();
	sapi_shutdown();
	return exit_status;
}

static void phpweb_init_globals(void)
{
	phpweb_global = (phpweb_sapi_globals *)malloc(sizeof(phpweb_sapi_globals));
	if (phpweb_global)
	{
		phpweb_global->thread_count = PHPWEB_THREAD_DEFAULT_NUM;
		phpweb_global->thread_state = PHPWEB_T_WAIT;
	}
	zend_hash_init(&phpweb_ini_entries, 0, NULL, NULL, 0);
}

static void phpweb_free_globals(void)
{
	zend_hash_destroy(&phpweb_ini_entries);
	free(phpweb_global->ub_total_len);
	free(phpweb_global->ub_stream);
	free(phpweb_global);
}
