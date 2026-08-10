const void* webkit_web_view_new();
void webkit_web_view_load_html(void* web_view, const char* content,
                               const char* base_uri);
void webkit_web_view_try_close(void* web_view);
void webkit_web_view_terminate_web_process(void* web_view);
unsigned int webkit_context_menu_get_n_items(void* menu);
void* webkit_context_menu_get_item_at_position(void* menu,
                                               unsigned int position);
void* webkit_context_menu_item_get_gaction(void* item);
int webkit_context_menu_item_get_stock_action(void* item);
int webkit_context_menu_item_is_separator(void* item);
void* webkit_context_menu_get_event(void* menu);
void webkit_context_menu_remove(void* menu, void* item);
void webkit_context_menu_append(void* menu, void* item);
void webkit_context_menu_prepend(void* menu, void* item);
void* webkit_context_menu_item_new_from_gaction(void* action, const char* label,
                                                void* target);
void* webkit_navigation_policy_decision_get_navigation_action(void* decision);
int webkit_navigation_action_get_navigation_type(void* navigation);
void webkit_policy_decision_ignore(void* decision);
const char* webkit_uri_request_get_uri(void* request);
void* webkit_navigation_action_get_request(void* navigation);
void* webkit_response_policy_decision_get_request(void* decision);
void* webkit_web_view_get_settings(void* web_view);
void webkit_settings_set_allow_file_access_from_file_urls(void* settings,
                                                          int allowed);
void webkit_settings_set_allow_universal_access_from_file_urls(void* settings,
                                                               int allowed);
typedef void (*WebKitURISchemeRequestCallback)(void* request, void* user_data);
typedef void (*GDestroyNotify)(void* data);
void* webkit_web_context_get_default();
const char* webkit_uri_request_get_http_method(void* request);
const char* webkit_uri_request_get_uri(void* request);
void* webkit_uri_request_get_http_headers(void* request);
void webkit_web_context_register_uri_scheme(
    void* context, const char* scheme, WebKitURISchemeRequestCallback callback,
    void* user_data, GDestroyNotify user_data_destroy_func);
const char* webkit_uri_scheme_request_get_path(void* request);
const char* webkit_uri_scheme_request_get_uri(void* request);
void webkit_uri_request_set_uri(void* request, const char* uri);
void* webkit_uri_scheme_request_get_http_headers(void* request);
const char* webkit_uri_scheme_request_get_http_method(void* request);
void* webkit_uri_scheme_request_get_http_body(void* request);
const char* webkit_uri_scheme_request_get_scheme(void* request);
uint64_t soup_message_headers_get_content_length(void* hdrs);
const char* soup_message_headers_get_content_type(void* hdrs, void** params);
void webkit_uri_scheme_request_finish(void* request, void* stream,
                                      int64_t stream_length,
                                      const char* content_type);
void webkit_uri_scheme_request_finish_error(void* request, void* error);
void webkit_uri_scheme_request_finish_with_response(void* request,
                                                    void* response);
uint32_t webkit_network_error_quark();
void* webkit_web_view_get_inspector(void* webview);
void webkit_web_inspector_show(void* inspector);

typedef enum {
  WEBKIT_CONTEXT_MENU_ACTION_NO_ACTION,
  WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK,
  WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK_IN_NEW_WINDOW,
  WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_LINK_TO_DISK,
  WEBKIT_CONTEXT_MENU_ACTION_COPY_LINK_TO_CLIPBOARD,
  WEBKIT_CONTEXT_MENU_ACTION_OPEN_IMAGE_IN_NEW_WINDOW,
  WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_IMAGE_TO_DISK,
  WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD,
  WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_URL_TO_CLIPBOARD,
  WEBKIT_CONTEXT_MENU_ACTION_OPEN_FRAME_IN_NEW_WINDOW,
  WEBKIT_CONTEXT_MENU_ACTION_GO_BACK,
  WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD,
  WEBKIT_CONTEXT_MENU_ACTION_STOP,
  WEBKIT_CONTEXT_MENU_ACTION_RELOAD,
  WEBKIT_CONTEXT_MENU_ACTION_COPY,
  WEBKIT_CONTEXT_MENU_ACTION_CUT,
  WEBKIT_CONTEXT_MENU_ACTION_PASTE,
  WEBKIT_CONTEXT_MENU_ACTION_DELETE,
  WEBKIT_CONTEXT_MENU_ACTION_SELECT_ALL,
  WEBKIT_CONTEXT_MENU_ACTION_INPUT_METHODS,
  WEBKIT_CONTEXT_MENU_ACTION_UNICODE,
  WEBKIT_CONTEXT_MENU_ACTION_SPELLING_GUESS,
  WEBKIT_CONTEXT_MENU_ACTION_NO_GUESSES_FOUND,
  WEBKIT_CONTEXT_MENU_ACTION_IGNORE_SPELLING,
  WEBKIT_CONTEXT_MENU_ACTION_LEARN_SPELLING,
  WEBKIT_CONTEXT_MENU_ACTION_IGNORE_GRAMMAR,
  WEBKIT_CONTEXT_MENU_ACTION_FONT_MENU,
  WEBKIT_CONTEXT_MENU_ACTION_BOLD,
  WEBKIT_CONTEXT_MENU_ACTION_ITALIC,
  WEBKIT_CONTEXT_MENU_ACTION_UNDERLINE,
  WEBKIT_CONTEXT_MENU_ACTION_OUTLINE,
  WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT,
  WEBKIT_CONTEXT_MENU_ACTION_OPEN_VIDEO_IN_NEW_WINDOW,
  WEBKIT_CONTEXT_MENU_ACTION_OPEN_AUDIO_IN_NEW_WINDOW,
  WEBKIT_CONTEXT_MENU_ACTION_COPY_VIDEO_LINK_TO_CLIPBOARD,
  WEBKIT_CONTEXT_MENU_ACTION_COPY_AUDIO_LINK_TO_CLIPBOARD,
  WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_CONTROLS,
  WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_LOOP,
  WEBKIT_CONTEXT_MENU_ACTION_ENTER_VIDEO_FULLSCREEN,
  WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PLAY,
  WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PAUSE,
  WEBKIT_CONTEXT_MENU_ACTION_MEDIA_MUTE,
  WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_VIDEO_TO_DISK,
  WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_AUDIO_TO_DISK,
  WEBKIT_CONTEXT_MENU_ACTION_INSERT_EMOJI,
  WEBKIT_CONTEXT_MENU_ACTION_PASTE_AS_PLAIN_TEXT,
  WEBKIT_CONTEXT_MENU_ACTION_CUSTOM
} WebKitContextMenuAction;