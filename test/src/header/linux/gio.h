int g_application_run(void* application, int argc, char** argv);
void g_application_quit(void* application);
const void* g_action_get_state_type(void* action);
const char* g_action_get_name(void* action);
void* g_action_get_state(void* action);
int g_variant_type_is_basic(const void* type);
void g_simple_action_set_enabled(void* simple, int enabled);
void* g_simple_action_new(const char* name, const void* parameter_type);
void g_application_hold(void* application);
void g_application_release(void* application);
void g_action_activate(void* action, void* parameter);
typedef void (*GDestroyNotify)(void* data);
void* g_memory_input_stream_new_from_data(uint8_t* data, uint64_t len,
                                          GDestroyNotify destroy);
