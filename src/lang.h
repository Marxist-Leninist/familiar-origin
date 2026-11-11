#ifndef ORIGIN_LANG_H
#define ORIGIN_LANG_H
/* Origin Script v0: bounded interpreter, not Python. */
int lang_run(const char *source,void (*output)(const char *),void (*action)(const char *));
#endif
