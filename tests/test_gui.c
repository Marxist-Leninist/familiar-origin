#include "gui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern unsigned gui_test_render_count,gui_test_pointer_bytes;
const unsigned char *gui_test_screen(void);
static RamFS *testfs;
static void execute(char *line){char *p=line;char *cmd=next_word(&p);if(text_equal(cmd,"write")){char *name=next_word(&p);if(*p==' ')++p;assert(fs_write(testfs,name,p)==0);}}

int main(void){static RamFS fs;static unsigned char heap[65536],before[64000];Arena arena={heap,0,sizeof(heap)};fs_init(&fs);fs_write(&fs,"test.txt","original contents");testfs=&fs;gui_init(&fs,&arena,execute);memcpy(before,gui_test_screen(),64000);unsigned frames=gui_test_render_count;for(unsigned i=0;i<1000;++i)gui_mouse(0,i%2?-1:1,0);assert(gui_test_render_count==frames);assert(gui_test_pointer_bytes==60000);assert(memcmp(before,gui_test_screen(),64000)==0);puts("PASS: 1000 pointer moves restore framebuffer exactly, trigger zero full redraws, write at most 60 framebuffer bytes per packet instead of 64020.");gui_mouse(0,-10000,10000);gui_mouse(0,10000,-10000);gui_mouse(0,-10000,-10000);gui_mouse(0,10000,10000);puts("PASS: pointer clamps safely at all four corners.");
assert(gui_edit("test.txt")==0);gui_key(1);for(const char *p="Hello\nWorld";*p;++p)gui_key(*p);gui_key(1001);gui_key(1003);gui_key('d');gui_key(19);assert(strcmp(fs_read(&fs,"test.txt"),"Hello\nWorld")==0);
assert(gui_edit("test.txt")==0);gui_key('!');gui_key(27);gui_key('n');gui_key(27);gui_key('y');assert(strcmp(fs_read(&fs,"test.txt"),"Hello\nWorld")==0);
assert(gui_edit("test.txt")==0);gui_key(1);for(unsigned i=0;i<600;++i)gui_key('x');gui_key(19);assert(strlen(fs_read(&fs,"test.txt"))==511);
puts("PASS native editor: select all, multiline insertion, cursor/delete, shared save, unsaved keep/discard, 511-byte bound.");}
