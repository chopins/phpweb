typedef unsigned long int pthread_t;
typedef void *(*__start_routine)(void *);
union pthread_attr_t
{
  char __size[__SIZEOF_PTHREAD_ATTR_T];
  long int __align;
};
typedef union pthread_attr_t pthread_attr_t;
typedef struct __pthread_internal_list
{
    struct __pthread_internal_list *__prev;
    struct __pthread_internal_list *__next;
} __pthread_list_t;
//<%%__pthread_mutex_s%%>//
typedef union
{
    unsigned long long int __value64;
    struct
    {
        unsigned int __low;
        unsigned int __high;
    } __value32;
} __atomic_wide_counter;
struct __pthread_cond_s
{
    __atomic_wide_counter __wseq;
    __atomic_wide_counter __g1_start;
    unsigned int __glibc_unused___g_refs[2];
    unsigned int __g_size[2];
    unsigned int __g1_orig_size;
    unsigned int __wrefs;
    unsigned int __g_signals[2];
};

typedef union
{
    char __size[__SIZEOF_PTHREAD_CONDATTR_T];
    int __align;
} pthread_condattr_t;
typedef union
{
    struct __pthread_cond_s __data;
    char __size[__SIZEOF_PTHREAD_COND_T];
    __extension__ long long int __align;
} pthread_cond_t;
typedef union
{
    struct __pthread_mutex_s __data;
    char __size[__SIZEOF_PTHREAD_MUTEX_T];
    long int __align;
} pthread_mutex_t;
typedef union
{
    char __size[__SIZEOF_PTHREAD_MUTEXATTR_T];
    int __align;
} pthread_mutexattr_t;
extern int pthread_create(pthread_t *__newthread,const pthread_attr_t *__attr,void *(*__start_routine)(void *a),void *__arg);
extern void pthread_exit(void *__retval);
extern int pthread_join(pthread_t __th, void **__thread_return);
extern int pthread_detach(pthread_t __th);
pthread_t pthread_self(void);
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr,int *detachstate);

extern int pthread_mutex_init(pthread_mutex_t *__mutex, const pthread_mutexattr_t *__mutexattr);
extern int pthread_mutex_destroy(pthread_mutex_t *__mutex);
extern int pthread_mutex_trylock(pthread_mutex_t *__mutex);
extern int pthread_mutex_lock(pthread_mutex_t *__mutex);
extern int pthread_cond_init(pthread_cond_t *__cond, const pthread_condattr_t *__cond_attr);
extern int pthread_cond_destroy(pthread_cond_t *__cond);
extern int pthread_cond_signal(pthread_cond_t *__cond);
extern int pthread_cond_broadcast(pthread_cond_t *__cond);
extern int pthread_cond_wait(pthread_cond_t *__cond, pthread_mutex_t *__mutex);