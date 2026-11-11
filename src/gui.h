#ifndef ORIGIN_GUI_H
#define ORIGIN_GUI_H
#include "core.h"
void gui_init(RamFS *fs,Arena *arena,void (*execute)(char *));
void gui_render(void);
void gui_char(char c);
void gui_clear(void);
void gui_input(const char *s);
int gui_key(int c);
void gui_console(void);
int gui_edit(const char *name);
void gui_training(unsigned step,const char *loss,const char *output,int active,unsigned fingerprint);
void gui_mouse(unsigned buttons,int dx,int dy);
#endif
