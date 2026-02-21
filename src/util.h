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

char *real_join_paths(long path_max, const char *first, ...);
#define join_paths(...) real_join_paths(__VA_ARGS__, NULL)

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

typedef struct {
  long path_max;
  long name_max;
} PathconfLimits_t;

void get_data_dir(Path_t *path);

EPathState check_pathname_state(const char *pathname);

EPathState check_pathname_state(const char *pathname);

char *get_config_dir(void);

int make_directory_recursive(const char *path);

void *calloc_wrap(const size_t n, const size_t size);

int get_pathconf_limits(const char *path, PathconfLimits_t *limits);

void verbose_printf(const char *fmt, ...);

void verbose_puts(const char *s);

void parse_signed(const char *s, long minv, long maxv, long *out);

void parse_unsigned(const char *s, unsigned long maxv, unsigned long *out);

#endif
