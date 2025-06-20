#ifndef TRANSLATE_H
#define TRANSLATE_H

#ifdef ENABLE_NLS
#include <locale.h>
#endif
#include "gettext.h"
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop(String)

#endif
