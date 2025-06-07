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
#include "util.h"

void get_data_dir(Path_t *path) {
  // TODO: Maybe we need a different name for this. It's also defined
  // as a macro during compilation
  char *datadir = getenv("DEALERSCHOICE_DATADIR");
  if (datadir != NULL) {
    snprintf(path->data, sizeof path->data, "%s", datadir);
    return;
  }
  // This will be changed before the first release. We'll look here and
  strcpy(path->data, "../data");
  return;
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
