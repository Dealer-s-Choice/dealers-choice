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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

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
#include <string.h>
#include <unistd.h>
#define PATH_SEP '/'
#define mkdir(path, mode) mkdir(path, mode)
#endif

#include "config.h"
#include "dc_config.h"
#include "graphics.h"
#include "util.h"

bool verbose = false;

void get_data_dir(Path_t *path) {
  // TODO: Maybe we need a different name for this. It's also defined
  // as a macro during compilation
  char *datadir = getenv("DEALERSCHOICE_DATADIR");
  if (datadir != NULL) {
    snprintf(path->data, sizeof path->data, "%s", datadir);
    return;
  }
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

char *get_config_dir(void) {
  const char *subdir = DEALERSCHOICE_NAME;
  char *result = NULL;

#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
    size_t len = strlen(path) + strlen(subdir) + 2;
    result = malloc(len);
    if (result)
      snprintf(result, len, "%s\\%s", path, subdir);
  }
#else
  const char *base = getenv("XDG_CONFIG_HOME");
  if (!base) {
    const char *home = getenv("HOME");
    if (!home)
      return NULL;
    size_t len = strlen(home) + strlen("/.config/") + strlen(subdir) + 1;
    result = malloc(len);
    if (result)
      snprintf(result, len, "%s/.config/%s", home, subdir);
  } else {
    size_t len = strlen(base) + strlen("/") + strlen(subdir) + 1;
    result = malloc(len);
    if (result)
      snprintf(result, len, "%s/%s", base, subdir);
  }
#endif

  return result;
}

EPathState check_pathname_state(const char *pathname) {
#ifdef _WIN32
  DWORD attrs = GetFileAttributesA(pathname);
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)
               ? PATH_NOT_FOUND
               : PATH_ERROR;
  return PATH_EXISTS;
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

void *calloc_wrap(const size_t n, const size_t size) {
  void *ptr = calloc(n, size);
  if (ptr)
    return ptr;

  perror("calloc");
  exit(EXIT_FAILURE);
}

#ifdef _WIN32
static long get_path_max(const char *path) {
  (void)path; // unused
  return 260; // Traditional Windows MAX_PATH
}

static long get_name_max(const char *path) {
  (void)path; // unused
  return 255; // Common NTFS limit
}

#else
static long get_single_pathconf_limit(const char *path, int name, long fallback,
                                      const char *label) {
  if (!path) {
    fprintf(stderr, "Error: Path is NULL.\n");
    return -1;
  }

  errno = 0;
  long limit = pathconf(path, name);
  if (limit == -1) {
    if (errno == 0)
      return fallback; // Limit not defined
    perror(label);
    return -1;
  }

  return limit;
}

static long get_path_max(const char *path) {
  return get_single_pathconf_limit(path, _PC_PATH_MAX, PATH_MAX, "PATH_MAX");
}

static long get_name_max(const char *path) {
  return get_single_pathconf_limit(path, _PC_NAME_MAX, NAME_MAX, "NAME_MAX");
}
#endif

int get_pathconf_limits(const char *path, PathconfLimits_t *limits) {
  if (!limits)
    return -1;

  limits->path_max = get_path_max(path);
  if (limits->path_max == -1)
    return -1;

  limits->name_max = get_name_max(path);
  if (limits->name_max == -1)
    return -1;

  return 0;
}

char *real_join_paths(long path_max, const char *first, ...) {
  char *path = calloc(1, path_max);
  if (!path) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  va_list ap;
  va_start(ap, first);
  const char *segment = first;

  size_t len = 0;

  while (segment != NULL) {
    size_t seg_len = strlen(segment);

    // Ensure enough room: +1 for possible '/', +1 for '\0'
    if (len + seg_len + 2 > (size_t)path_max) {
      fprintf(stderr, "Path length exceeds maximum allowed (%ld)\n", path_max);
      free(path);
      exit(EXIT_FAILURE);
    }

    // Add separator if needed
    if (len > 0 && path[len - 1] != '/' && segment[0] != '/') {
      path[len++] = '/';
    }

    strcpy(path + len, segment);
    len += seg_len;

    segment = va_arg(ap, const char *);
  }

  va_end(ap);
  return path;
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

void parse_signed(const char *s, long minv, long maxv, long *out) {
  errno = 0;
  char *endptr;
  long v = strtol(s, &endptr, 0);

  if (errno == ERANGE || *endptr != '\0' || v < minv || v > maxv) {
    fprintf(stderr, "Invalid signed integer value: '%s'\n", s);
    exit(EXIT_FAILURE);
  }

  *out = v;
}

void parse_unsigned(const char *s, unsigned long maxv, unsigned long *out) {
  errno = 0;
  char *endptr;
  unsigned long v = strtoul(s, &endptr, 0);

  if (errno == ERANGE || *endptr != '\0' || v > maxv) {
    fprintf(stderr, "Invalid unsigned integer value: '%s'\n", s);
    exit(EXIT_FAILURE);
  }

  *out = v;
}

char *dc_strdup(const char *s) {
  if (!s)
    return NULL;

  size_t len = strlen(s) + 1;
  char *copy = malloc(len);
  if (!copy)
    return NULL;

  memcpy(copy, s, len);
  return copy;
}
