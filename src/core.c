#include "core.h"
size_t text_len(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
int text_equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
void text_copy(char *d, const char *s) { while ((*d++ = *s++)) {} }
char *next_word(char **cursor) {
    char *p = *cursor;
    while (*p == ' ') ++p;
    if (!*p) { *cursor = p; return 0; }
    char *start = p;
    while (*p && *p != ' ') ++p;
    if (*p) *p++ = 0;
    *cursor = p;
    return start;
}
int parse_uint(const char *s, uint32_t *out) {
    uint32_t n=0;
    if (!s || !*s) return 0;
    while (*s) {
        if (*s<'0' || *s>'9') return 0;
        uint32_t digit=(uint32_t)(*s++-'0');
        if (n>(UINT32_MAX-digit)/10) return 0;
        n=n*10+digit;
    }
    *out=n; return 1;
}
void *arena_alloc(Arena *a, size_t size) {
    if (!size || size > a->size || a->used > a->size) return 0;
    if (a->used > SIZE_MAX - 15) return 0;
    size_t at = (a->used + 15) & ~(size_t)15;
    if (at > a->size || size > a->size - at) return 0;
    a->used = at + size;
    return a->base + at;
}
void fs_init(RamFS *fs) { for (size_t i=0; i<FILE_COUNT; ++i) fs->files[i].used=0; }
static int valid_name(const char *s) {
    size_t n=text_len(s);
    if (!n || n >= NAME_SIZE) return 0;
    for (size_t i=0; i<n; ++i)
        if (!((s[i]>='a' && s[i]<='z') || (s[i]>='A' && s[i]<='Z') ||
              (s[i]>='0' && s[i]<='9') || s[i]=='.' || s[i]=='-' || s[i]=='_')) return 0;
    return 1;
}
const char *fs_read(const RamFS *fs, const char *name) {
    for (size_t i=0; i<FILE_COUNT; ++i)
        if (fs->files[i].used && text_equal(fs->files[i].name,name)) return fs->files[i].data;
    return 0;
}
int fs_write(RamFS *fs, const char *name, const char *data) {
    if (!valid_name(name) || text_len(data)>=DATA_SIZE) return -1;
    int slot=-1;
    for (size_t i=0; i<FILE_COUNT; ++i) {
        if (fs->files[i].used && text_equal(fs->files[i].name,name)) { slot=(int)i; break; }
        if (!fs->files[i].used && slot<0) slot=(int)i;
    }
    if (slot<0) return -2;
    text_copy(fs->files[slot].name,name);
    text_copy(fs->files[slot].data,data);
    fs->files[slot].used=1;
    return 0;
}
int fs_remove(RamFS *fs, const char *name) {
    for (size_t i=0; i<FILE_COUNT; ++i)
        if (fs->files[i].used && text_equal(fs->files[i].name,name)) {
            fs->files[i].used=0; return 0;
        }
    return -1;
}
int core_selftest(void) {
    unsigned char buf[64] __attribute__((aligned(16)));
    Arena a={buf,0,sizeof(buf)};
    if (arena_alloc(&a,1)!=buf || arena_alloc(&a,16)!=buf+16 ||
        arena_alloc(&a,33)!=0 || arena_alloc(&a,32)!=buf+32 || arena_alloc(&a,1)!=0) return 0;
    RamFS fs; fs_init(&fs);
    if (fs_write(&fs,"test","hello") || !text_equal(fs_read(&fs,"test"),"hello")) return 0;
    if (fs_write(&fs,"test","updated") || !text_equal(fs_read(&fs,"test"),"updated")) return 0;
    if (fs_remove(&fs,"test") || fs_read(&fs,"test")) return 0;
    if (fs_write(&fs,"bad/name","x")!=-1) return 0;
    char words[]="  alpha   beta"; char *p=words;
    if (!text_equal(next_word(&p),"alpha") || !text_equal(next_word(&p),"beta") || next_word(&p)) return 0;
    return 1;
}
