void* gtk_application_new(const char* application_id, int flags);
void* gtk_application_window_new(void* application);
void gtk_window_set_title(void* window, const char* title);
void gtk_window_set_default_size(void* window, int width, int height);
void gtk_window_set_child(void* window, void* child);
void gtk_widget_show(void* widget);
void gtk_window_present(void* window);
void* gtk_scrolled_window_new(void);
void gtk_scrolled_window_set_policy(void* scrolled_window,
                                    int hscrollbar_policy,
                                    int vscrollbar_policy);
void gtk_scrolled_window_set_child(void* scrolled_window, void* child);