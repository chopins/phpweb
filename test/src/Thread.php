<?php

namespace Toknot\Webui;

use FFI;
use FFI\CData;

/**
 * @method int pthread_create(pthread_t_ptr $__newthread,pthread_attr_t_ptr $__attr, callable $__start_routine,void_ptr $__arg);
 * @method void pthread_exit(void_ptr $__retval);
 * @method int pthread_join(pthread_t $__th, void_ptr_ptr $__thread_return);
 * @method int pthread_detach(pthread_t $__th);
 * @method int pthread_mutex_init(pthread_mutex_t_ptr $__mutex, const pthread_mutexattr_t_ptr $__mutexattr);
 * @method int pthread_mutex_destroy(pthread_mutex_t_ptr $__mutex);
 * @method int pthread_mutex_trylock(pthread_mutex_t_ptr $__mutex);
 * @method int pthread_mutex_lock(pthread_mutex_t_ptr $__mutex);
 * @method int pthread_cond_init(pthread_cond_t_ptr $__cond, const pthread_condattr_t_ptr $__cond_attr);
 * @method int pthread_cond_destroy(pthread_cond_t_ptr $__cond);
 * @method int pthread_cond_signal(pthread_cond_t_ptr $__cond);
 * @method int pthread_cond_broadcast(pthread_cond_t_ptr $__cond);
 * @method int pthread_cond_wait(pthread_cond_t_ptr $__cond, pthread_mutex_t_ptr $__mutex);
 */
class Thread
{
    const EAGAIN = 11;
    const EINVAL = 22;
    const EPERM = 1;
    const EDEADLK = 0;
    const ESRCH = 3;

    /* Detach state.  */
    const PTHREAD_CREATE_JOINABLE = 0,
        PTHREAD_CREATE_DETACHED = 1;

    /* Mutex types.  */
    const PTHREAD_MUTEX_TIMED_NP = 0,
        PTHREAD_MUTEX_RECURSIVE_NP = 1,
        PTHREAD_MUTEX_ERRORCHECK_NP = 2,
        PTHREAD_MUTEX_ADAPTIVE_NP = 3,
        PTHREAD_MUTEX_NORMAL = self::PTHREAD_MUTEX_TIMED_NP,
        PTHREAD_MUTEX_RECURSIVE = self::PTHREAD_MUTEX_RECURSIVE_NP,
        PTHREAD_MUTEX_ERRORCHECK = self::PTHREAD_MUTEX_ERRORCHECK_NP,
        PTHREAD_MUTEX_DEFAULT = self::PTHREAD_MUTEX_NORMAL,
        PTHREAD_MUTEX_FAST_NP = self::PTHREAD_MUTEX_TIMED_NP;

    /* Robust mutex or not flags.  */
    const PTHREAD_MUTEX_STALLED = 0,
        PTHREAD_MUTEX_STALLED_NP = self::PTHREAD_MUTEX_STALLED,
        PTHREAD_MUTEX_ROBUST = 1,
        PTHREAD_MUTEX_ROBUST_NP = self::PTHREAD_MUTEX_ROBUST;

    /* Mutex protocols.  */
    const PTHREAD_PRIO_NONE = 0,
        PTHREAD_PRIO_INHERIT = 1,
        PTHREAD_PRIO_PROTECT = 2;

    /* Read-write lock types.  */
    const PTHREAD_RWLOCK_PREFER_READER_NP = 0,
        PTHREAD_RWLOCK_PREFER_WRITER_NP = 1,
        PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP = 2,
        PTHREAD_RWLOCK_DEFAULT_NP = self::PTHREAD_RWLOCK_PREFER_READER_NP;

    /* Scheduler inheritance.  */
    const PTHREAD_INHERIT_SCHED = 0,
        PTHREAD_EXPLICIT_SCHED = 1;

    /* Scope handling.  */
    const PTHREAD_SCOPE_SYSTEM = 0,
        PTHREAD_SCOPE_PROCESS = 1;

    /* Process shared or private flag.  */
    const PTHREAD_PROCESS_PRIVATE = 0,
        PTHREAD_PROCESS_SHARED = 1;

    /* Cancellation */
    const  PTHREAD_CANCEL_ENABLE = 0,
        PTHREAD_CANCEL_DISABLE = 1;

    const PTHREAD_CANCEL_DEFERRED = 0,
        PTHREAD_CANCEL_ASYNCHRONOUS = 1;

    const PTHREAD_ONCE_INIT = 0;
    const PTHREAD_BARRIER_SERIAL_THREAD = -1;


    const ERROR = [
        self::EAGAIN => 'Insufficient resources to create another thread.',
        self::EINVAL => [
            'create' => 'Invalid settings in attr.',
            'join' => 'Another thread is already waiting to join with this thread or thread is not a joinable thread.',
            'detach' =>  'thread is not a joinable thread.'
        ],
        self::EPERM => 'No permission to set the scheduling policy and parameters specified in attr.',
        self::ESRCH => 'No thread with the ID thread could be found.',

    ];
    private static ?FFI $ffi = null;
    private ?int $threadid = null;
    private function __construct($threadid)
    {
        $this->threadid = $threadid;
    }
    private static function init()
    {
        if (self::$ffi) {
            return;
        }
        $isX86_64 = php_uname('m') === 'x86_64';
        $define = self::defineSizeof($isX86_64);
        $code = file_get_contents(__DIR__ . '/header/'. strtolower(PHP_OS_FAMILY).'/pthread.h');
        $code = str_replace('//<%%__pthread_mutex_s%%>//', self::defineStructPthreadMutexS($isX86_64), $code);
        $code = str_replace(array_keys($define), $define, $code);
        self::$ffi = FFI::cdef($code);
    }

    public static function getCData(mixed $v, string &$decl = ''): CData
    {
        $cv = null;
        if (is_bool($v)) {
            $v = (int)$v;
        }
        if (is_int($v)) {
            $type = PHP_INT_SIZE == 4 ? 'int32_t' : 'int64_t';
            $decl = "$type $decl;";
            $cv = self::$ffi->new($type, false, true);
            $cv->cdata = $v;
        } else if (is_float($v)) {
            $type = 'double';
            $decl = "$type $decl;";
            $cv = self::$ffi->new($type, false, true);
            $cv->cdata = $v;
        } else if (is_string($v)) {
            $len = strlen($v);
            $type = 'char[' . $len + 1 . ']';
            $decl = "char {$decl}[" . $len + 1 . '];';
            $cv = self::$ffi->new($type, false, true);
            FFI::memcpy($cv, $v, $len);
        }
        return $cv;
    }
    public static function getStuctCData(array $array)
    {
        static $si = 0;
        $struct = 'typedef struct {';
        $data = [];
        foreach ($array as $k => $v) {
            $k = "__m_$k";
            $decl = $k;
            $data[$k] = self::getCData($v, $decl);
            $struct .= $decl;
        }
        $struct .= "} __s_{$si};";
        $si++;
        $s = self::$ffi->new(self::$ffi->type($struct), false, true);
        foreach ($data as $n => $d) {
            $s->$n = $d;
            FFI::free($d);
        }
        return $s;
    }

    public function join()
    {
        $ret = self::$ffi->new('void*', false, true);
        $status = self::pthread_join($this->threadid, FFI::addr($ret));
        if ($status !== 0) {
            if ($status == self::EINVAL) {
                $msg = self::ERROR[self::EINVAL]['join'];
            } else {
                $msg = self::ERROR[$status] ?? '';
            }
            throw new \RuntimeException("pthread join error: $msg", $status);
        }
        return $ret;
    }

    public function detach()
    {
        $status = self::pthread_detach($this->threadid);
        if ($status !== 0) {
            if ($status == self::EINVAL) {
                $msg = self::ERROR[self::EINVAL]['detach'];
            } else {
                $msg = self::ERROR[$status] ?? '';
            }
            throw new \RuntimeException("pthread detach error: $msg", $status);
        }
    }

    public static function run($func, mixed ...$args): Thread
    {
        self::init();
        $threadid = self::$ffi->new('pthread_t', false, true);
        $argc = count($args);
        $arg1 = $arg = null;
        if ($argc === 1) {
            $arg = self::getCData($args[0]);
        } else if ($argc > 1) {
            $arg = self::getStuctCData($args);
        }
        if ($argc > 0) {
            $arg1 = self::$ffi->cast('void*', FFI::addr($arg));
        }

        $status = self::pthread_create(FFI::addr($threadid), NULL, 'test', $arg1);
        if ($status !== 0) {
            if ($status == self::EINVAL) {
                $msg = self::ERROR[self::EINVAL]['create'];
            } else {
                $msg = self::ERROR[$status] ?? '';
            }
            throw new \RuntimeException("create pthread error: $msg", $status);
        }

        return new static($threadid->cdata);
    }

    public static function __callStatic(string $name, array $arguments = [])
    {
        self::init();
        return self::$ffi->$name(...$arguments);
    }

    public static function defineSizeof(bool $isX86_64): array
    {
        $define = [
            '__SIZEOF_PTHREAD_MUTEXATTR_T' => 4,
            '__SIZEOF_PTHREAD_COND_T' => 48,
            '__SIZEOF_PTHREAD_CONDATTR_T' => 4,
            '__SIZEOF_PTHREAD_RWLOCKATTR_T' => 8,
            '__SIZEOF_PTHREAD_BARRIERATTR_T' => 4,
        ];
        if ($isX86_64) {
            if (PHP_INT_SIZE == 8) {
                return [
                    ...$define,
                    '__SIZEOF_PTHREAD_MUTEX_T' => 40,
                    '__SIZEOF_PTHREAD_ATTR_T' => 56,
                    '__SIZEOF_PTHREAD_RWLOCK_T' => 56,
                    '__SIZEOF_PTHREAD_BARRIER_T' => 32,
                ];
            } else {
                return [
                    ...$define,
                    '__SIZEOF_PTHREAD_MUTEX_T' => 32,
                    '__SIZEOF_PTHREAD_ATTR_T' => 32,
                    '__SIZEOF_PTHREAD_RWLOCK_T' => 44,
                    '__SIZEOF_PTHREAD_BARRIER_T' => 20,
                ];
            }
        } else {
            return [
                ...$define,
                '__SIZEOF_PTHREAD_MUTEX_T' => 24,
                '__SIZEOF_PTHREAD_ATTR_T' => 36,
                '__SIZEOF_PTHREAD_RWLOCK_T' => 32,
                '__SIZEOF_PTHREAD_BARRIER_T' => 20,
            ];
        }
    }

    public static function defineStructPthreadMutexS(bool $isX86_64): string
    {
        $struct = 'struct __pthread_mutex_s{int __lock;unsigned int __count;int __owner;';
        if ($isX86_64) {
            $struct .= 'unsigned int __nusers;';
        }
        $struct .= 'int __kind;';
        if ($isX86_64) {
            $struct .= 'short __spins;short __elision;__pthread_list_t __list;';
        } else {
            $struct .= 'unsigned int __nusers;union{struct {short __espins;short __eelision;} __elision_data;__pthread_slist_t __list;};';
        }
        return $struct . '};';
    }
}
