/*
 util.h
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

#ifndef __UTIL_H
#define __UTIL_H

extern bool verbose;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

typedef struct {
  char data[2048];
  char config[2048];
  char *server_conf_name;
} Path_t;

typedef enum {
  PATH_EXISTS = 1,
  PATH_NOT_FOUND = 2,
  PATH_ERROR = -1,
} EPathState;

void get_data_dir(Path_t *path);

EPathState check_pathname_state(const char *pathname);

EPathState check_pathname_state(const char *pathname);

int make_directory_recursive(const char *path);

void *real_calloc_wrap(const size_t n, const size_t size, const char *func, int line);
void *real_malloc_wrap(const size_t size, const char *func, int line);

#define calloc_wrap(n, size) real_calloc_wrap(n, size, __func__, __LINE__)
#define malloc_wrap(size) real_malloc_wrap(size, __func__, __LINE__)

void verbose_printf(const char *fmt, ...);

void verbose_puts(const char *s);

typedef enum {
  DC_LOG_INFO,
  DC_LOG_WARN,
  DC_LOG_ERROR,
} DCLogLevel_t;

/* Timestamped logger: prints "YYYY-MM-DD HH:MM:SS [LEVEL] <msg>\n" to stderr.
 * INFO and WARN are gated by --verbose; ERROR always prints. The message must
 * NOT include a trailing newline (dc_log appends one). */
#if defined(__GNUC__)
void dc_log(DCLogLevel_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void dc_log(DCLogLevel_t level, const char *fmt, ...);
#endif

/* Redirect dc_log output to a file (appended, line-buffered). Used by
 * --log-file, mainly so GUI clients with no console can capture diagnostics. */
void dc_log_set_file(const char *path);

void parse_signed(const char *s, long minv, long maxv, long *out);

void parse_unsigned(const char *s, unsigned long maxv, unsigned long *out);

char *dc_strdup(const char *s);

char *expand_tilde(const char *path);

#endif
