/*
 util.c
 https://github.com/Dealer-s-Choice/dealers_choice

 MIT License

 Copyright (c) 2025 Andy Alt

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
// clang-format off
#include "dc_windows.h"
#include <direct.h>
#include <shlobj.h>
#define PATH_SEP '\\'
#define mkdir(path, mode) _mkdir(path)
// clang-format on
#else
#include <fcntl.h>
#include <unistd.h>
#define PATH_SEP '/'
#define mkdir(path, mode) mkdir(path, mode)
#endif

#include "config.h"
#include "dc_config.h"
#include "util.h"

bool verbose = false;
bool dc_debug = false;
bool dc_test_mode = false;

void get_data_dir(Path_t *path) {
  // TODO: Maybe we need a different name for this. It's also defined
  // as a macro during compilation
  char *datadir = getenv("DEALERSCHOICE_DATADIR");
  if (datadir != NULL) {
    snprintf(path->data, sizeof path->data, "%s", datadir);
    return;
  }

#ifdef _WIN32
  // Look for data/ next to the executable — correct for installed builds.
  char exe_path[MAX_PATH];
  if (GetModuleFileNameA(NULL, exe_path, sizeof exe_path) != 0) {
    char *last_sep = strrchr(exe_path, '\\');
    if (last_sep) {
      *last_sep = '\0';
      snprintf(path->data, sizeof path->data, "%s\\data", exe_path);
      if (check_pathname_state(path->data) == PATH_EXISTS)
        return;
    }
  }
#endif

  // This will be changed before the first release. We'll look here and
  if (check_pathname_state("../data") == PATH_EXISTS) {
    strcpy(path->data, "../data");
    return;
  }

  if (check_pathname_state(DEALERSCHOICE_DATADIR) == PATH_EXISTS) {
    snprintf(path->data, sizeof(path->data), "%s", DEALERSCHOICE_DATADIR);
    return;
  }

  fputs("Unable to find data.\n", stderr);
  exit(EXIT_FAILURE);
}

EPathState check_pathname_state(const char *pathname) {
#ifdef _WIN32
  DWORD attrs = GetFileAttributesA(pathname);
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
               ? PATH_NOT_FOUND
               : PATH_ERROR;
  return (attrs & FILE_ATTRIBUTE_DIRECTORY) ? PATH_EXISTS : PATH_ERROR;
#else
  struct stat sb;
  if (stat(pathname, &sb) == 0 && S_ISDIR(sb.st_mode))
    return PATH_EXISTS;
  else if (errno == ENOENT)
    return PATH_NOT_FOUND;
  return PATH_ERROR;
#endif
}

int make_directory(const char *pathname) {
#ifdef _WIN32
  return CreateDirectoryA(pathname, NULL) ? 0 : -1;
#else
  return mkdir(pathname, 0700); // rwx for user only
#endif
}

int make_directory_recursive(const char *path) {
  if (!path || *path == '\0')
    return 0; // empty path is "created"

  // Copy path to a modifiable buffer
  char tmp[512];
  size_t len = strlen(path);
  if (len >= sizeof(tmp))
    return -1;
  strncpy(tmp, path, sizeof(tmp));
  tmp[len] = '\0';

  // Remove trailing separator if any (except root "/")
  if (len > 1 && tmp[len - 1] == PATH_SEP)
    tmp[len - 1] = '\0';

#ifdef _WIN32
  // Stop recursion at a Windows drive root ("C:" or "C:\") — cannot mkdir it.
  if (tmp[1] == ':' && (tmp[2] == '\0' || (tmp[2] == PATH_SEP && tmp[3] == '\0')))
    return 0;
#endif

  // Recursively create parent directory
  char *slash = strrchr(tmp, PATH_SEP);
  if (slash) {
    *slash = '\0';
    if (make_directory_recursive(tmp) != 0)
      return -1;
    *slash = PATH_SEP;
  }

  // Create current directory
  if (mkdir(tmp, 0777) != 0) {
    if (errno == EEXIST)
      return 0; // already exists, that's fine
    return -1;  // other error
  }

  return 0;
}

void *real_calloc_wrap(const size_t n, const size_t size, const char *func, int line) {
  void *ptr = calloc(n, size);
  if (ptr)
    return ptr;

  dc_log(DC_LOG_ERROR, "calloc: %s in %s() at line %d", strerror(errno), func, line);
  exit(EXIT_FAILURE);
}

void *real_malloc_wrap(const size_t size, const char *func, int line) {
  void *ptr = malloc(size);
  if (ptr)
    return ptr;

  dc_log(DC_LOG_ERROR, "malloc: %s in %s() at line %d", strerror(errno), func, line);
  exit(EXIT_FAILURE);
}

void verbose_printf(const char *fmt, ...) {
  if (!verbose)
    return;

  va_list args;
  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
}

void verbose_puts(const char *s) {
  if (verbose && s)
    puts(s);
}

static FILE *dc_log_fp = NULL; /* NULL => stderr */

static void dc_log_close(void) {
  if (dc_log_fp) {
    if (fclose(dc_log_fp) != 0)
      fprintf(stderr, "dc_log: error closing log file: %s\n", strerror(errno));
    dc_log_fp = NULL;
  }
}

void dc_log_set_file(const char *path) {
  FILE *f = fopen(path, "a");
  if (!f) {
    fprintf(stderr, "dc_log: cannot open log file %s: %s\n", path, strerror(errno));
    return;
  }
  /* Unbuffered so a crash still leaves the full log. Was _IOLBF with size 0,
   * but a NULL buffer with size 0 is invalid on MSVC's CRT (size must be > 0),
   * where it trips the invalid-parameter handler; _IONBF ignores buf/size and is
   * valid everywhere. (On Windows _IOLBF was full-buffering anyway -- MSVC maps
   * _IOLBF to _IOFBF -- so unbuffered is the better match for the crash-safety
   * intent regardless.) */
  setvbuf(f, NULL, _IONBF, 0);
  dc_log_fp = f;
  atexit(dc_log_close); /* flush + checked close at normal exit */
}

void dc_log(DCLogLevel_t level, const char *fmt, ...) {
  if (level == DC_LOG_DEBUG) {
    if (!dc_debug)
      return;
  } else if (level != DC_LOG_ERROR && !verbose) {
    return;
  }

  static const char *const tag[] = {"DEBUG", "INFO", "WARN", "ERROR"};
  FILE *out = dc_log_fp ? dc_log_fp : stderr;

  time_t now = time(NULL);
  struct tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &now);
#else
  localtime_r(&now, &tmv);
#endif
  char ts[20];
  strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

  fprintf(out, "%s [%s] ", ts, tag[level]);
  va_list args;
  va_start(args, fmt);
  vfprintf(out, fmt, args);
  va_end(args);
  fputc('\n', out);
}

void parse_signed(const char *s, long minv, long maxv, long *out) {
  errno = 0;
  char *endptr;
  long v = strtol(s, &endptr, 0);

  if (errno == ERANGE || *endptr != '\0' || v < minv || v > maxv) {
    dc_log(DC_LOG_ERROR, "Invalid signed integer value: '%s'", s);
    exit(EXIT_FAILURE);
  }

  *out = v;
}

void parse_unsigned(const char *s, unsigned long maxv, unsigned long *out) {
  errno = 0;
  char *endptr;
  unsigned long v = strtoul(s, &endptr, 0);

  if (errno == ERANGE || *endptr != '\0' || v > maxv) {
    dc_log(DC_LOG_ERROR, "Invalid unsigned integer value: '%s'", s);
    exit(EXIT_FAILURE);
  }

  *out = v;
}

char *dc_strdup(const char *s) {
  if (!s)
    return NULL;

  size_t len = strlen(s) + 1;
  char *copy = malloc_wrap(len);
  memcpy(copy, s, len);
  return copy;
}

char *expand_tilde(const char *path) {
  if (!path || path[0] != '~' || (path[1] != '/' && path[1] != '\0'))
    return dc_strdup(path);

#ifdef _WIN32
  const char *home = getenv("USERPROFILE");
#else
  const char *home = getenv("HOME");
#endif
  if (!home)
    return dc_strdup(path);

  size_t home_len = strlen(home);
  size_t path_len = strlen(path);
  /* home + (path with the leading '~' dropped) + NUL. The second memcpy copies
     path_len bytes (path+1 is path_len-1 chars plus its NUL), so home_len +
     path_len already covers the terminator exactly; the +1 keeps the buffer
     obviously in-bounds for a reader (and the static analyzer). */
  char *result = malloc_wrap(home_len + path_len + 1);
  memcpy(result, home, home_len);
  memcpy(result + home_len, path + 1, path_len);
  return result;
}
