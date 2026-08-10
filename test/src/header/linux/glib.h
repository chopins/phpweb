typedef int (*GSourceFunc)(void* user_data);
typedef void (*GDestroyNotify)(void* data);
void* g_main_context_default();
int g_main_context_iteration(void* context, int may_block);
int g_main_context_pending(void* context);
void g_main_context_dispatch(void* context);
unsigned int g_idle_add_full(int priority, GSourceFunc f, void* data,
                             GDestroyNotify notify);
unsigned int g_idle_add(GSourceFunc function, void* data);
int g_source_remove(unsigned int tag);
void* g_error_new(uint32_t domain, int code, const char* format);
char* g_uri_unescape_string(const char* escaped_string,
                            const char* illegal_characters);