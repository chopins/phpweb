typedef unsigned long gulong;
typedef unsigned long gsize;
typedef int (*GCallback)(void* p1, void* p2, void* p3, void* p4);
typedef void (*GClosureNotify)(void* data, void* closure);
void g_object_unref(void* object);
void g_signal_handler_disconnect(void* instance, gulong handler_id);
gulong g_signal_connect_data(void* instance, const char* detailed_signal,
                             GCallback c_handler, void* data,
                             GClosureNotify destroy_data, int connect_flags);
void g_clear_object(void** object_ptr);
void g_signal_emit_by_name(void* ins, const char* signal);
void g_object_set(void* object, const char* first_property_name, int p1,
                  void* p2);