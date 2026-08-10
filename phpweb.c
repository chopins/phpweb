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

/* Global variables */
// static smart_str phpweb_output_buf;
static phpweb_ub_stream *phpweb_output_stream = NULL;
static char *phpweb_docroot = NULL;
static char *phpweb_current_script = NULL;
static int phpweb_no_ini = 0;
static char *phpweb_ini_path = NULL;
static HashTable phpweb_ini_entries;
static zend_llist phpweb_extension_lists;
PHPAPI extern char *php_ini_opened_path;
PHPAPI extern char *php_ini_scanned_path;
PHPAPI extern char *php_ini_scanned_files;
static char *php_self = "";

/* Options flags */
static int phpweb_show_help = 0;
static int phpweb_show_modules = 0;
static int phpweb_show_version = 0;
static int phpweb_show_ini = 0;
static int phpweb_show_info = 0;
static char *phpweb_execute_file = NULL;
static char *phpweb_execute_code = NULL;
static char *phpweb_begin_code = NULL;
static char *phpweb_end_code = NULL;

#ifdef PHP_WIN32
static HWND phpweb_hwnd;
static ICoreWebView2Controller *phpweb_controller;
static ICoreWebView2 *phpweb_webview;
#else
static GtkApplication *phpweb_app = NULL;
static GtkWidget *phpweb_window;
static GtkWidget *phpweb_webview;
#endif

/* SAPI module structure */
static sapi_module_struct phpweb_sapi_module = {
    "phpweb",
    "PHPWEB SAPI",
    phpweb_startup,
    php_module_shutdown_wrapper,
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

static size_t phpweb_ub_write(const char *str, size_t len)
{
  // smart_str_appendl(&phpweb_output_buf, str, len);
  phpweb_output_stream->total_len += len;
  g_memory_input_stream_add_bytes(phpweb_output_stream->stream, g_bytes_new(str, len));
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
  return php_module_startup(sapi_module, NULL);
}

static int phpweb_activate(void)
{
  phpweb_output_stream->total_len = 0;
  phpweb_output_stream->stream = G_MEMORY_INPUT_STREAM(g_memory_input_stream_new());
  g_input_stream_set_pending(G_INPUT_STREAM(phpweb_output_stream->stream), NULL);
  if (phpweb_webview)
  {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(phpweb_webview), PHPWEB_WEBVIEW_SCHEME "://output/activate");
  }
  return SUCCESS;
}

static int phpweb_deactivate(void)
{
  if (phpweb_webview)
  {
    g_input_stream_clear_pending(G_INPUT_STREAM(phpweb_output_stream->stream));
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

static void phpweb_execute_php_code(char *code)
{
}

static void *phpweb_execute_php_script(void *file)
{
  zval retval;
  int status;
  zend_file_handle file_handle;
  char *translated_path = NULL;
  char *filename = (char *)file;
  zend_call_stack_init();
  virtual_cwd_activate();

  ZVAL_UNDEF(&retval);
  if (php_request_startup() == FAILURE)
  {
    // smart_str_appends(&phpweb_output_buf, "Error: request startup failure\n");
    goto clear;
  }
  // if (code)
  // {
  //   if (zend_eval_stringl(code, strlen(code), &retval, "phpweb code") == FAILURE)
  //   {
  //     // smart_str_appends(&phpweb_output_buf, "Error executing code\n");
  //   }
  // }
  // else
  if (filename)
  {
    printf("Open file %s\n", filename);
    // zend_stream_init_filename(&file_handle, filename);
    phpweb_seek_file_begin(&file_handle, filename);

    char real_path[MAXPATHLEN];
    if (VCWD_REALPATH(filename, real_path))
    {
      translated_path = strdup(real_path);
    }
    CG(skip_shebang) = 1;
    php_self = filename;
    SG(request_info).path_translated = translated_path ? translated_path : php_self;
    zend_try
    {
      status = php_execute_script(&file_handle);
      // execThread = g_thread_new("php-exec", phpweb_execute_script_thread_func, &file_handle);
      // status = GPOINTER_TO_INT(g_thread_join(execThread);
    }
    zend_end_try();
    if (status == FAILURE)
    {
      // smart_str_appends(&phpweb_output_buf, "Error executing script: ");
      // smart_str_appends(&phpweb_output_buf, filename);
      // smart_str_appendc(&phpweb_output_buf, '\n');
    }
    printf("phpweb_excute_php() success\n");
    zend_destroy_file_handle(&file_handle);
  }
clear:
  printf("phpweb_excute_php() clear\n");
  php_output_end_all();
  php_request_shutdown(NULL);
  return NULL;
}

static void phpweb_execute_php_script_try_new_thread(char* filepath)
{
  // GThread *exec_thread;
  // GError **gerror = NULL;
  // exec_thread = g_thread_try_new("phpexec", phpweb_execute_php_script, filepath, gerror);
  // g_thread_join(exec_thread);
  pthread_t thread;
  pthread_attr_t attr;
  void** ret = NULL;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 16*1024*1024);
  pthread_create(&thread, &attr, phpweb_execute_php_script, filepath);
  pthread_join(thread, ret);
}

static void phpweb_show_in_webview(const char *content, const char *base_uri)
{
#ifdef PHP_WIN32
  char *wrapped = NULL;
  if (base_uri)
  {
    spprintf(&wrapped, 0, "<html><head><base href=\"%s\"></head><body>%s</body></html>", base_uri, content);
    phpweb_webview->lpVtbl->NavigateToString(phpweb_webview, wrapped);
    efree(wrapped);
  }
  else
  {
    phpweb_webview->lpVtbl->NavigateToString(phpweb_webview, content);
  }
#else
  webkit_web_view_load_html(WEBKIT_WEB_VIEW(phpweb_webview), content, base_uri);
#endif
}

static void phpweb_show_file_in_webview(char *filepath)
{
  const char *ext = strrchr(filepath, '.');

  if (ext && (strcasecmp(ext, ".php") == 0))
  {
    if (phpweb_current_script)
      free(phpweb_current_script);
    phpweb_current_script = strdup(filepath);
    // phpweb_execute_php_script(filepath);

    phpweb_execute_php_script_try_new_thread(filepath);
    // GThread *exec_thread;
    // GError **gerror = NULL;
    // exec_thread = g_thread_try_new("phpexec", phpweb_execute_php_script, filepath, gerror);
    // g_thread_join(exec_thread);
    // char base_uri[4096];
    // snprintf(base_uri, sizeof(base_uri), "php://exec/%s/", phpweb_docroot ? phpweb_docroot : "");
    // phpweb_show_in_webview(content, NULL);
  }
  else
  {
#ifdef PHP_WIN32
    char *uri = malloc(strlen(filepath) + 8);
    sprintf(uri, "file:///%s", filepath);
    phpweb_webview->lpVtbl->Navigate(phpweb_webview, uri);
    free(uri);
#else
    char *uri = g_filename_to_uri(filepath, NULL, NULL);
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(phpweb_webview), uri);
    g_free(uri);
#endif
    if (phpweb_current_script)
    {
      free(phpweb_current_script);
      phpweb_current_script = NULL;
    }
  }
}

#ifndef PHP_WIN32
static void on_file_open_complete(GObject *source, GAsyncResult *res, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
  GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
  if (file)
  {
    char *filepath = g_file_get_path(file);
    phpweb_show_file_in_webview(filepath);
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

static void phpweb_handle_refresh(void)
{
  if (phpweb_current_script)
    phpweb_show_file_in_webview(phpweb_current_script);
}

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

zend_always_inline static char *get_phpweb_usage(void)
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
    return;
  php_printf("<html><body><pre>");
  php_printf(get_phpweb_usage());
  php_printf("</pre></body><html>");
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
  printf("gkt run\n");

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

  webkit_web_view_load_html(WEBKIT_WEB_VIEW(phpweb_webview), "<html><body>default</body></html>", "localhost");

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), phpweb_webview);
  gtk_window_set_child(GTK_WINDOW(phpweb_window), scrolledwindow);

  // g_signal_connect(phpweb_webview, "decide-policy", G_CALLBACK(phpweb_navigation_policy_cb), NULL);
  g_signal_connect(phpweb_webview, "context-menu", G_CALLBACK(phpweb_webview_context_menu_cb), NULL);

  gtk_window_present(GTK_WINDOW(phpweb_window));

  /* Defer PHP command-line actions to idle so webview is ready */
  g_idle_add(phpweb_execute_pending, NULL);
}

static gboolean phpweb_execute_pending(gpointer user_data)
{
  if (phpweb_begin_code)
    phpweb_execute_php_code(phpweb_begin_code);

  if (phpweb_execute_file)
  {
    phpweb_show_file_in_webview(phpweb_execute_file);
  }
  else if (phpweb_execute_code)
  {
    phpweb_execute_php_code(phpweb_execute_code);
  }

  if (phpweb_show_modules)
    phpweb_show_module_info();
  if (phpweb_show_version)
    phpweb_show_version_info();
  if (phpweb_show_ini)
    phpweb_show_ini_info();
  if (phpweb_show_info)
    phpweb_show_phpinfo();
  if (phpweb_end_code)
    phpweb_execute_php_code(phpweb_end_code);

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
          phpweb_show_file_in_webview(filename);
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
  const char *exec_prefix = PHPWEB_WEBVIEW_SCHEME "://exec/";
  const char *out_prefix = PHPWEB_WEBVIEW_SCHEME "://output/activate";

  if (strncmp(uri, out_prefix, strlen(out_prefix)) == 0)
  {
    webkit_uri_scheme_request_finish(request, G_INPUT_STREAM(phpweb_output_stream->stream), -1, "text/html");
    phpweb_output_stream->total_len = 0;
    phpweb_output_stream->stream = NULL;
    return;
  }
  else if (strncmp(uri, exec_prefix, strlen(exec_prefix)) == 0)
  {
    const char *rel_path = uri + strlen(exec_prefix);
    char full_path[4096];
    if (phpweb_docroot)
    {
      snprintf(full_path, sizeof(full_path), "%s/%s", phpweb_docroot, rel_path);
    }
    else
    {
      char cwd[2048];
      if (getcwd(cwd, sizeof(cwd)))
      {
        snprintf(full_path, sizeof(full_path), "%s/%s", cwd, rel_path);
      }
      else
      {
        webkit_uri_scheme_request_finish_error(request, g_error_new(webkit_network_error_quark(), WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST, "Request URI %.255s unknown uri", uri));
        return;
      }
    }

    // phpweb_execute_php_script(full_path);
    phpweb_execute_php_script_try_new_thread(full_path);

    // GBytes *bytes = g_bytes_new(content, strlen(content));
    // GInputStream *stream = g_memory_input_stream_new_from_bytes(bytes);
    // webkit_uri_scheme_request_finish(request, stream, strlen(content), "text/html");
    // g_object_unref(stream);
    // g_bytes_unref(bytes);
  }
  else
  {
    webkit_uri_scheme_request_finish_error(request, NULL);
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
  phpweb_handle_refresh();
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
static void phpweb_parse_opts(int argc, char *argv[])
{
  int i;
  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] != '-')
      break;
    if (strcmp(argv[i], "-h") == 0)
    {
      phpweb_show_help = 1;
      printf(get_phpweb_usage());
      exit(0);
    }
    else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
    {
      phpweb_ini_path = argv[++i];
    }
    else if (strcmp(argv[i], "-n") == 0)
    {
      phpweb_no_ini = 1;
    }
    else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
    {
      char *entry = argv[++i];
      zval zv;
      ZVAL_STRING(&zv, entry);
      zend_hash_next_index_insert(&phpweb_ini_entries, &zv);
    }
    else if (strcmp(argv[i], "-z") == 0 && i + 1 < argc)
    {
      zend_llist_add_element(&phpweb_extension_lists, &argv[++i]);
    }
    else if (strcmp(argv[i], "-m") == 0)
    {
      phpweb_show_modules = 1;
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      phpweb_show_version = 1;
    }
    else if (strcmp(argv[i], "--ini") == 0)
    {
      phpweb_show_ini = 1;
    }
    else if (strcmp(argv[i], "-i") == 0)
    {
      phpweb_show_info = 1;
    }
    else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
    {
      phpweb_execute_file = argv[++i];
    }
    else if (strcmp(argv[i], "-F") == 0 && i + 1 < argc)
    {
      phpweb_execute_file = argv[++i];
    }
    else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
    {
      phpweb_execute_code = argv[++i];
    }
    else if (strcmp(argv[i], "-R") == 0 && i + 1 < argc)
    {
      phpweb_execute_code = argv[++i];
    }
    else if (strcmp(argv[i], "-B") == 0 && i + 1 < argc)
    {
      phpweb_begin_code = argv[++i];
    }
    else if (strcmp(argv[i], "-E") == 0 && i + 1 < argc)
    {
      phpweb_end_code = argv[++i];
    }
    else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
    {
      phpweb_docroot = strdup(argv[++i]);
    }
  }
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

int main(int argc, char *argv[])
{
  int exit_status = 0;

  phpweb_init_globals();
  phpweb_parse_opts(argc, argv);

  if (phpweb_show_help)
    return 0;

  sapi_startup(&phpweb_sapi_module);

  phpweb_apply_ini_entries();

  if (phpweb_sapi_module.startup(&phpweb_sapi_module) == FAILURE)
  {
    sapi_shutdown();
    return 1;
  }

#ifdef PHP_WIN32
  if (FAILURE == phpweb_win32_init())
  {
    php_module_shutdown();
    sapi_shutdown();
    return 1;
  }
  phpweb_webview2_setup();

  /* On Windows, we still call before message loop (or later with idle if needed) */
  if (phpweb_begin_code)
    phpweb_execute_php_code(phpweb_begin_code);
  if (phpweb_execute_file)
    phpweb_show_file_in_webview(phpweb_execute_file);
  else if (phpweb_execute_code)
  {
    phpweb_execute_php_code(phpweb_execute_code);
  }
  if (phpweb_show_modules)
    phpweb_show_module_info();
  if (phpweb_show_version)
    phpweb_show_version_info();
  if (phpweb_show_ini)
    phpweb_show_ini_info();
  if (phpweb_show_info)
    phpweb_show_phpinfo();
  if (phpweb_end_code)
    phpweb_execute_php_code(phpweb_end_code);

  phpweb_win32_run();
#else
  phpweb_gtk_run(); /* Actions registered in activate, pending executed via idle */
#endif

  // if(&phpweb_output_buf) smart_str_free(&phpweb_output_buf);
  if (phpweb_current_script)
    free(phpweb_current_script);
  if (phpweb_docroot)
    free(phpweb_docroot);
  zend_hash_destroy(&phpweb_ini_entries);
  zend_llist_destroy(&phpweb_extension_lists);

  php_module_shutdown();
  sapi_shutdown();

  return exit_status;
}

static void phpweb_init_globals(void)
{
  phpweb_output_stream = (phpweb_ub_stream *)malloc(sizeof(phpweb_ub_stream));
  if (phpweb_output_stream)
  {
    phpweb_output_stream->total_len = 0;
    phpweb_output_stream->stream = NULL;
  }
  zend_hash_init(&phpweb_ini_entries, 0, NULL, NULL, 0);
  zend_llist_init(&phpweb_extension_lists, sizeof(char *), NULL, 0);
  phpweb_current_script = NULL;
  phpweb_docroot = NULL;
  phpweb_no_ini = 0;
  phpweb_ini_path = NULL;
}