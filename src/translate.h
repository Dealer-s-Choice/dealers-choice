#ifndef TRANSLATE_H
#define TRANSLATE_H

#ifdef __cplusplus
extern "C" {
#endif

void init_translation(const char *lang, const char *path);
const char *translate(const char *msgid);

#ifdef __cplusplus
}
#endif

#endif 
