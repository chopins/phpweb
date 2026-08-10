#include "php.h"
#include "zend.h"
#include "zend_extensions.h"
#include "php_ini.h"
#include "php_globals.h"
#include "php_main.h"
#include "php_output.h"
#include "php_variables.h"

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

typedef struct _phpweb_ub_stream
{
    GMemoryInputStream *stream;
    gsize total_len;
} phpweb_ub_stream;

/* Forward declarations */
static void phpweb_init_globals(void);
static size_t phpweb_ub_write(const char *str, size_t len);
static void phpweb_log_message(const char *message, int syslog_type_int);
static void phpweb_flush(void *server_context);
static void phpweb_send_header(sapi_header_struct *sapi_header, void *server_context);
static char *phpweb_read_cookies(void);
static int phpweb_startup(sapi_module_struct *sapi_module);
static int phpweb_activate(void);
static int phpweb_deactivate(void);

static void phpweb_execute_php(char *filename, const char *code);
static void phpweb_show_in_webview(const char *content, const char *base_uri);
static void phpweb_show_file_in_webview(char *filepath);
static void phpweb_handle_open_file(void);
static void phpweb_handle_refresh(void);
static void phpweb_show_module_info(void);
static void phpweb_show_version_info(void);
static void phpweb_show_ini_info(void);
static void phpweb_show_phpinfo(void);
static void phpweb_show_help_info(void);
static void phpweb_quit(void);

#ifdef PHP_WIN32
static int phpweb_win32_init(void);
static void phpweb_win32_run(void);
static void phpweb_webview2_setup(void);
#else
static void phpweb_gtk_run(void);
static void phpweb_gtk_activate_cb(GtkApplication *app, gpointer user_data);

static gboolean phpweb_navigation_policy_cb(WebKitWebView *web_view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer user_data);
static gboolean phpweb_webview_context_menu_cb(WebKitWebView *web_view, WebKitContextMenu *menu, GdkEvent *event, WebKitHitTestResult *hit_test_result, gpointer user_data);
static void phpweb_webview_uri_scheme_cb(WebKitURISchemeRequest *request, gpointer user_data);
static gboolean phpweb_execute_pending(gpointer user_data);
static void phpweb_register_gactions(GtkApplication *app);
static void phpweb_open_file_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_refresh_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_modules_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_version_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_ini_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_info_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_help_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void phpweb_quit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif