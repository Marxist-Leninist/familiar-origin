#include "core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(core_selftest());
    RamFS fs;fs_init(&fs);assert(!fs_read(&fs,"none"));
    assert(fs_write(&fs,"","x")==-1);
    assert(fs_write(&fs,"abcdefghijklmnopqrstuvwxyz012345","x")==-1);
    char huge[513];memset(huge,'x',512);huge[512]=0;
    assert(fs_write(&fs,"large",huge)==-1);
    char name[2]={'a',0};
    for(unsigned i=0;i<FILE_COUNT;++i){name[0]=(char)('a'+i);assert(!fs_write(&fs,name,"ok"));}
    assert(fs_write(&fs,"full","x")==-2);
    assert(!fs_write(&fs,"a","updated"));assert(text_equal(fs_read(&fs,"a"),"updated"));
    assert(!fs_remove(&fs,"a"));assert(fs_remove(&fs,"a")==-1);assert(!fs_write(&fs,"new","reused"));
    unsigned char buf[64] __attribute__((aligned(16)));Arena a={buf,0,64};
    assert(!arena_alloc(&a,0));assert(!arena_alloc(&a,SIZE_MAX));assert(a.used==0);
    assert(arena_alloc(&a,64)==buf);assert(!arena_alloc(&a,1));
    uint32_t n=0;assert(parse_uint("4294967295",&n)&&n==UINT32_MAX);
    assert(!parse_uint("4294967296",&n));assert(!parse_uint("-1",&n));
    assert(!parse_uint("",&n));assert(!parse_uint(0,&n));assert(!parse_uint("12x",&n));
    assert(parse_uint("0",&n)&&n==0);
    puts("HOST TEST PASS: core self-test, invalid names, bounds, full/reused RAMFS, allocation overflow");
}
