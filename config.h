/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* Whether to build swoole as dynamic module */
#define COMPILE_DL_SWOOLE 1

/* have accept4 */
#define HAVE_ACCEPT4 1

/* have clock_gettime */
#define HAVE_CLOCK_GETTIME 1

/* cpu affinity? */
#define HAVE_CPU_AFFINITY 1

/* have daemon */
#define HAVE_DAEMON 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* have epoll */
#define HAVE_EPOLL 1

/* have eventfd */
#define HAVE_EVENTFD 1

/* have execinfo */
#define HAVE_EXECINFO 1

/* have hiredis */
#define HAVE_HIREDIS 1

/* have inotify */
#define HAVE_INOTIFY 1

/* have inotify_init1 */
#define HAVE_INOTIFY_INIT1 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* have kqueue */
/* #undef HAVE_KQUEUE */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* have mkostemp */
#define HAVE_MKOSTEMP 1

/* have pthread_mutex_timedlock */
#define HAVE_MUTEX_TIMEDLOCK 1

/* have nghttp2 */
#define HAVE_NGHTTP2 1

/* have openssl */
#define HAVE_OPENSSL 1

/* have pcre */
#define HAVE_PCRE 1

/* have pthread_barrier_init */
#define HAVE_PTHREAD_BARRIER 1

/* have SO_REUSEPORT? */
#define HAVE_REUSEPORT 1

/* have pthread_rwlock_init */
#define HAVE_RWLOCK 1

/* have sendfile */
#define HAVE_SENDFILE 1

/* have signalfd */
#define HAVE_SIGNALFD 1

/* have pthread_spin_lock */
#define HAVE_SPINLOCK 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* have timerfd */
#define HAVE_TIMERFD 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1
/* do we enable swoole debug */
/* #undef SW_DEBUG */

/* have zlib */
#define SW_HAVE_ZLIB 1

/* enable sockets support */
/* #undef SW_SOCKETS */

/* enable http2.0 support */
/* #undef SW_USE_HTTP2 */

/* enable hugepage support */
/* #undef SW_USE_HUGEPAGE */

/* use jemalloc */
/* #undef SW_USE_JEMALLOC */

/* enable openssl support */
/* #undef SW_USE_OPENSSL */

/* enable async-redis support */
#define SW_USE_REDIS 1

/* enable ringbuffer support */
/* #undef SW_USE_RINGBUFFER */

/* use tcmalloc */
/* #undef SW_USE_TCMALLOC */

/* enable thread support */
/* #undef SW_USE_THREAD */
