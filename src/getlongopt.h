/*
 * getlongopt.h — Minimal header-only long option parser
 * Inspired by GNU getopt_long API, implemented originally for cross-platform use.
 */

#ifndef GETLONGOPT_H
#define GETLONGOPT_H

#include <stddef.h>
#include <string.h>

typedef struct {
  const char *name;
  int has_arg; /* 0 = no_argument, 1 = required_argument, 2 = optional_argument */
  int val;
  int short_char; /* optional single-char short form, e.g. 'h' for -h (0 = none) */
} glopt_option_t;

#define GLOPT_NO_ARG 0
#define GLOPT_REQUIRED_ARG 1
#define GLOPT_OPTIONAL_ARG 2

typedef struct {
  const glopt_option_t *options;
  int optind;
  const char *optarg;
} glopt_parser_t;

static void glopt_init(glopt_parser_t *p, const glopt_option_t *opts) {
  p->options = opts;
  p->optind = 1;
  p->optarg = NULL;
}

static int glopt_next(glopt_parser_t *p, int argc, char **argv) {
  p->optarg = NULL;

  if (p->optind >= argc)
    return -1;

  const char *arg = argv[p->optind++];

  /* Short option: -X */
  if (arg[0] == '-' && arg[1] != '-' && arg[1] != '\0' && arg[2] == '\0') {
    int sc = (unsigned char)arg[1];
    for (int i = 0; p->options[i].name; ++i) {
      if (p->options[i].short_char && p->options[i].short_char == sc) {
        if (p->options[i].has_arg == GLOPT_REQUIRED_ARG) {
          if (p->optind >= argc || argv[p->optind][0] == '-')
            return '?';
          p->optarg = argv[p->optind++];
        } else if (p->options[i].has_arg == GLOPT_OPTIONAL_ARG) {
          if (p->optind < argc && argv[p->optind][0] != '-')
            p->optarg = argv[p->optind++];
        }
        return p->options[i].val;
      }
    }
    return '?';
  }

  /* Long option: --name or --name=value */
  if (arg[0] != '-' || arg[1] != '-')
    return '?';

  const char *name = arg + 2;
  const char *eq = strchr(name, '=');
  size_t name_len = eq ? (size_t)(eq - name) : strlen(name);

  for (int i = 0; p->options[i].name; ++i) {
    if (strncmp(name, p->options[i].name, name_len) == 0 && p->options[i].name[name_len] == '\0') {
      if (eq) {
        p->optarg = eq + 1;
      } else if (p->options[i].has_arg == GLOPT_REQUIRED_ARG) {
        if (p->optind >= argc || argv[p->optind][0] == '-')
          return '?';
        p->optarg = argv[p->optind++];
      } else if (p->options[i].has_arg == GLOPT_OPTIONAL_ARG) {
        if (p->optind < argc && argv[p->optind][0] != '-')
          p->optarg = argv[p->optind++];
      }
      return p->options[i].val;
    }
  }

  return '?';
}

#endif /* GETLONGOPT_H */
