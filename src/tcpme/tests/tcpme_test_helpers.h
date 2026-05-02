#pragma once

/* Keep assertions enabled in tests regardless of build type. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h> /* atoi */
#include <string.h> /* strrchr */

/* Thread portability: Win32 on Windows (MSVC and MinGW), pthreads elsewhere.
 * TC_THREAD_FN / TC_THREAD_RET — function signature and return value.
 * tc_thread_create / tc_thread_join — uniform create/join interface. */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
typedef HANDLE tc_thread_t;
#  define TC_THREAD_FN  DWORD WINAPI
#  define TC_THREAD_RET 0
static inline int tc_thread_create(tc_thread_t *t, LPTHREAD_START_ROUTINE fn, void *arg) {
  *t = CreateThread(NULL, 0, fn, arg, 0, NULL);
  return *t ? 0 : -1;
}
static inline void tc_thread_join(tc_thread_t t) {
  WaitForSingleObject(t, INFINITE);
  CloseHandle(t);
}
#else
#  include <pthread.h>
typedef pthread_t tc_thread_t;
#  define TC_THREAD_FN  void *
#  define TC_THREAD_RET NULL
static inline int tc_thread_create(tc_thread_t *t, void *(*fn)(void *), void *arg) {
  return pthread_create(t, NULL, fn, arg);
}
static inline void tc_thread_join(tc_thread_t t) { pthread_join(t, NULL); }
#endif

/* Extract the port number from a "IP:port" or "[IPv6]:port" string. */
static inline uint16_t extract_port(const char *addr) {
  const char *colon = strrchr(addr, ':');
  assert(colon != NULL);
  int p = atoi(colon + 1);
  assert(p > 0 && p <= 65535);
  return (uint16_t)p;
}
