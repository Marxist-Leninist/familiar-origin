#ifndef ORIGIN_CORE_H
#define ORIGIN_CORE_H
#include <stddef.h>
#include <stdint.h>
#define FILE_COUNT 8
#define NAME_SIZE 32
#define DATA_SIZE 512
typedef struct { char name[NAME_SIZE]; char data[DATA_SIZE]; int used; } RamFile;
typedef struct { RamFile files[FILE_COUNT]; } RamFS;
typedef struct { unsigned char *base; size_t used, size; } Arena;
size_t text_len(const char *s);
int text_equal(const char *a, const char *b);
void text_copy(char *dst, const char *src);
char *next_word(char **cursor);
int parse_uint(const char *s, uint32_t *out);
void *arena_alloc(Arena *a, size_t size);
void fs_init(RamFS *fs);
const char *fs_read(const RamFS *fs, const char *name);
int fs_write(RamFS *fs, const char *name, const char *data);
int fs_remove(RamFS *fs, const char *name);
int core_selftest(void);
#endif
