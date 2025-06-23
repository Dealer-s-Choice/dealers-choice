/*
 * getlongopt.h — Minimal header-only long option parser
 * Inspired by GNU getopt_long API, implemented originally for cross-platform use.
 */

#ifndef GETLONGOPT_H
#define GETLONGOPT_H

typedef struct {
  const char *name;
  int has_arg; // 0 = no_argument, 1 = required_argument
  int val;
} glopt_option_t;

#define GLOPT_NO_ARG 0
#define GLOPT_REQUIRED_ARG 1
#define GLOPT_OPTIONAL_ARG 2

typedef struct {
  const glopt_option_t *options;
  int optind;
  const char *optarg;
  const char *current;
} glopt_parser_t;

static void glopt_init(glopt_parser_t *p, const glopt_option_t *opts) {
  p->options = opts;
  p->optind = 1;
  p->optarg = NULL;
  p->current = NULL;
}

static int glopt_next(glopt_parser_t *p, int argc, char **argv) {
  p->optarg = NULL;

  while (p->optind < argc) {
    const char *arg = argv[p->optind];

    if (p->current == NULL) {
      if (arg[0] != '-' || arg[1] != '-') {
        // Non-option argument found, treat as error
        fprintf(stderr, "Error: unrecognized argument '%s'\n", arg);
        return '?';
      }
      p->current = arg + 2;
      ++p->optind;
    }

    for (int i = 0; p->options[i].name; ++i) {
      if (strcmp(p->current, p->options[i].name) == 0) {
        if (p->options[i].has_arg == GLOPT_REQUIRED_ARG) {
          if (p->optind >= argc || argv[p->optind][0] == '-') {
            // Next item is missing or looks like another option
            return '?';
          }
          p->optarg = argv[p->optind++];
        } else if (p->options[i].has_arg == GLOPT_OPTIONAL_ARG) {
          if (p->optind < argc && argv[p->optind][0] != '-') {
            p->optarg = argv[p->optind++];
          } else {
            p->optarg = NULL;
          }
        }
        p->current = NULL;
        return p->options[i].val;
      }
    }

    // Unrecognized option
    p->current = NULL;
    return '?';
  }

  return -1;
}

#endif // GETLONGOPT_H
