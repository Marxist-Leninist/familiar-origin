/* Original 320x200 indexed-colour framebuffer UI and compact 5x7 font. */
#include "gui.h"
static unsigned char pixels[320*200];
#ifdef ORIGIN_GUI_TEST
static unsigned char test_framebuffer[64000];
static volatile unsigned char *const screen=test_framebuffer;
unsigned gui_test_render_count,gui_test_pointer_bytes;
const unsigned char *gui_test_screen(void){return test_framebuffer;}
#else
static volatile unsigned char *const screen=(volatile unsigned char*)0xa0000;
#endif
static RamFS *disk;static Arena *memory;static void (*dispatch)(char *);
static char loglines[15][41],input[54],filename[32],filetext[512],selected[32],message[41];
static unsigned logrow,logcol,view,field,nn_step,nn_hash,edit_cursor;static char edit_original[512];static int edit_dirty,edit_select;static char nn_loss[20],nn_output[16];static int nn_active;static int dialog,mx=280,my=18,buttons;
static const char letters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:;!?-_/\\()[]+=<>\"'@#%&*|";
static const unsigned char shapes[][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
{0,0,0,0,0,12,12},{0,0,0,0,4,4,8},{0,4,4,0,4,4,0},{0,4,4,0,4,4,8},{4,4,4,4,4,0,4},{14,17,1,2,4,0,4},{0,0,0,31,0,0,0},{0,0,0,0,0,0,31},{1,2,2,4,8,8,16},{16,8,8,4,2,2,1},{2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{14,8,8,8,8,8,14},{14,2,2,2,2,2,14},{0,4,4,31,4,4,0},{0,0,31,0,31,0,0},{2,4,8,16,8,4,2},{8,4,2,1,2,4,8},{10,10,10,0,0,0,0},{4,4,8,0,0,0,0},{14,17,23,21,23,16,14},{10,10,31,10,31,10,10},{17,2,4,8,16,17,0},{12,18,20,8,21,18,13},{0,21,14,31,14,21,0},{4,4,4,4,4,4,4}};
static const unsigned char lower_shapes[26][7]={
{0,0,14,1,15,17,15},{16,16,30,17,17,17,30},{0,0,14,16,16,17,14},{1,1,15,17,17,17,15},{0,0,14,17,31,16,14},{6,8,8,30,8,8,8},{0,0,15,17,15,1,14},{16,16,30,17,17,17,17},{4,0,12,4,4,4,14},{2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},{0,0,26,21,21,21,21},{0,0,30,17,17,17,17},{0,0,14,17,17,17,14},{0,0,30,17,30,16,16},{0,0,15,17,15,1,1},{0,0,22,25,16,16,16},{0,0,15,16,14,1,30},{8,8,30,8,8,9,6},{0,0,17,17,17,19,13},{0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},{0,0,17,17,15,1,14},{0,0,31,2,4,8,31}};
static void rect(int x,int y,int w,int h,unsigned char color){for(int j=0;j<h;++j)for(int i=0;i<w;++i)if(x+i>=0&&x+i<320&&y+j>=0&&y+j<200)pixels[(y+j)*320+x+i]=color;}
static void glyph(int x,int y,char c,unsigned char color){if(c==' ')return;const unsigned char *shape;if(c>='a'&&c<='z')shape=lower_shapes[(unsigned)(c-'a')];else{unsigned index=0;while(letters[index]&&letters[index]!=c)++index;if(!letters[index])return;shape=shapes[index];}for(int j=0;j<7;++j)for(int i=0;i<5;++i)if(shape[j]&(1<<(4-i)))rect(x+i,y+j,1,1,color);}
static void text(int x,int y,const char *s,unsigned char color){for(unsigned i=0;s[i]&&x<315;++i,x+=6)glyph(x,y,s[i],color);}
static void clipped(int x,int y,const char *s,unsigned limit,unsigned char color){for(unsigned i=0;s[i]&&i<limit;++i)glyph(x+(int)i*6,y,s[i],color);}
static void uinttext(char *out,unsigned n){char temp[11];unsigned i=0,j=0;do{temp[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)out[j++]=temp[--i];out[j]=0;}
static void button(int x,int y,int w,const char *label,int active){rect(x,y,w,18,active?3:8);text(x+5,y+5,label,15);}
void gui_clear(void){for(unsigned r=0;r<15;++r)for(unsigned c=0;c<41;++c)loglines[r][c]=0;logrow=logcol=0;}
void gui_char(char c){if(c=='\b'){if(logcol)loglines[logrow][--logcol]=0;return;}if(c=='\n'){logcol=0;++logrow;}else if(c>=32&&c<=126){loglines[logrow][logcol++]=c;loglines[logrow][logcol]=0;if(logcol==40){logcol=0;++logrow;}}if(logrow==15){for(unsigned r=0;r<14;++r)for(unsigned col=0;col<41;++col)loglines[r][col]=loglines[r+1][col];for(unsigned col=0;col<41;++col)loglines[14][col]=0;logrow=14;}}
void gui_input(const char *s){size_t n=text_len(s),start=n>49?n-49:0;unsigned i=0;while(s[start]&&i<49)input[i++]=s[start++];input[i]=0;}
static void editor_render(void){
 rect(76,25,240,151,7);clipped(83,31,filename,31,0);text(83,42,"CTRL-S SAVE | ESC CLOSE",0);rect(82,53,228,78,edit_select?3:15);
 unsigned row=0,col=0,cursor_row=0,cursor_col=0,n=(unsigned)text_len(filetext);for(unsigned i=0;i<edit_cursor;++i){if(filetext[i]=='\n'){++row;col=0;}else if(++col==37){++row;col=0;}}cursor_row=row;cursor_col=col;unsigned top=cursor_row>6?cursor_row-6:0;row=col=0;
 for(unsigned i=0;i<n;++i){if(filetext[i]=='\n'){++row;col=0;continue;}if(row>=top&&row<top+7)glyph(84+(int)col*6,57+(int)(row-top)*10,filetext[i],0);if(++col==37){++row;col=0;}}
 rect(84+(int)cursor_col*6,56+(int)(cursor_row-top)*10,1,8,4);char number[12];uinttext(number,n);text(83,136,number,0);text(108,136,"/511",0);text(144,136,edit_dirty?"UNSAVED / RAM ONLY":"SAVED / RAM ONLY",edit_dirty?4:0);button(82,151,106,"SAVE",0);button(202,151,106,"CLOSE",0);
}
void gui_render(void){
#ifdef ORIGIN_GUI_TEST
 ++gui_test_render_count;
#endif
 rect(0,0,320,200,1);rect(0,0,320,20,0);text(8,7,"FAMILIAR ORIGIN 0.1.3",15);
#if UINTPTR_MAX > 0xffffffffu
 text(236,7,"NATIVE 64",11);
#else
 text(236,7,"NATIVE 32",11);
#endif
 const char *tabs[]={"FILES","NEW FILE","MEMORY","TESTS","CONSOLE","TRAIN NN"};for(unsigned i=0;i<6;++i)button(4,25+(int)i*25,66,tabs[i],view==i&&!dialog);
 rect(76,25,240,151,7);
 if(view==0){text(83,31,"FILES / RAM ONLY",0);unsigned row=0;for(unsigned i=0;i<FILE_COUNT;++i)if(disk->files[i].used){button(82,45+(int)row*15,228,disk->files[i].name,0);++row;}text(83,168,"CLICK A FILE TO READ",0);}
 else if(view==2){char num[12];text(84,35,"MEMORY",0);text(84,53,"ARENA BYTES:",0);uinttext(num,(unsigned)memory->size);text(168,53,num,1);text(84,68,"USED BYTES:",0);uinttext(num,(unsigned)memory->used);text(168,68,num,1);text(84,91,"SINGLE ADDRESS SPACE",0);text(84,105,"NO MEMORY PROTECTION YET",0);
#if UINTPTR_MAX > 0xffffffffu
 text(84,126,"SSE2 SCALAR MATH / NO AVX",0);
#else
 text(84,126,"X87 MATH / NO AVX",0);
#endif
 }
 else if(view==6){char num[12];text(84,33,"GUEST TRANSFORMER",0);text(84,47,"816 WEIGHTS / GUEST CPU",0);text(84,61,"STEP",0);uinttext(num,nn_step);text(120,61,num,1);text(151,61,"/250",0);text(84,74,"LOSS",0);text(120,74,nn_loss,1);text(84,88,"PREDICTION:",0);text(84,101,nn_output,1);text(84,115,"WEIGHT HASH",0);uinttext(num,nn_hash);text(156,115,num,1);text(84,128,nn_active?"TRAINING / INPUT STILL ACTIVE":"RAM ONLY / ONE PHRASE",0);button(82,149,70,"I TRAIN",0);button(158,149,70,"S STOP",0);button(234,149,74,"G SAY",0);}
 else if(view==5){text(83,31,selected,0);const char *data=fs_read(disk,selected);if(data){int x=84,y=47;for(unsigned i=0;data[i]&&y<166;++i){if(data[i]=='\n'||x>=306){x=84;y+=10;if(data[i]=='\n')continue;}glyph(x,y,data[i],0);x+=6;}}else text(84,50,"FILE NOT FOUND",0);button(82,153,70,"EDIT",0);button(160,153,70,"RUN .FO",0);button(238,153,70,"DELETE",0);}
 else {for(unsigned r=0;r<14;++r)clipped(81,31+(int)r*10,loglines[r+1],38,0);}
 rect(0,181,320,19,0);text(4,187,">",11);clipped(14,187,input,50,15);
 if(dialog==3||dialog==4)editor_render();
 if(dialog==4){rect(35,50,252,105,0);rect(37,52,248,101,7);text(45,63,"DISCARD UNSAVED CHANGES?",0);text(45,84,"Y DISCARD / N KEEP EDITING",0);button(44,113,110,"DISCARD",0);button(167,113,110,"KEEP",0);}
 else if(dialog==2){rect(35,50,252,105,0);rect(37,52,248,101,7);text(45,62,"DELETE THIS RAM FILE?",0);clipped(45,83,selected,31,4);button(44,113,110,"DELETE",0);button(167,113,110,"CANCEL",0);}
 else if(dialog==1){rect(35,34,252,133,0);rect(37,36,248,129,7);text(45,43,"NEW FILE / NEVER OVERWRITES",0);text(45,58,"NAME:",0);rect(44,68,233,13,field==0?15:8);clipped(47,71,filename,31,field==0?0:15);text(45,88,"TEXT (UP TO 120 CHARS):",0);rect(44,98,233,13,field==1?15:8);size_t n=text_len(filetext);clipped(47,101,filetext+(n>37?n-37:0),37,field==1?0:15);button(44,121,110,"CREATE",0);button(167,121,110,"CANCEL",0);clipped(44,148,message,39,4);}
 for(unsigned i=0;i<320*200;++i)screen[i]=pixels[i];
 for(int y=0;y<8;++y)for(int x=0;x<=y/2;++x)if(mx+x<320&&my+y<200)screen[(my+y)*320+mx+x]=(unsigned char)(x==0||x==y/2?0:15);
}
static void create_dialog(void){dialog=1;field=0;message[0]=0;filetext[0]=0;for(unsigned n=1;n<10000;++n){char number[12];uinttext(number,n);text_copy(filename,"file");text_copy(filename+4,number);text_copy(filename+text_len(filename),".txt");if(!fs_read(disk,filename))break;}gui_render();}
static void create_file(void){if(fs_read(disk,filename)){text_copy(message,"FILE EXISTS. CHOOSE ANOTHER NAME.");gui_render();return;}char line[170];text_copy(line,"new ");text_copy(line+4,filename);size_t n=text_len(line);line[n++]=' ';text_copy(line+n,filetext);dispatch(line);if(fs_read(disk,filename)){dialog=0;view=0;}else text_copy(message,"INVALID NAME OR RAM FILE TABLE FULL.");gui_render();}
static void delete_file(void){char c[40];text_copy(c,"rm ");text_copy(c+3,selected);dispatch(c);char yes[]="yes";dispatch(yes);dialog=0;view=0;}
int gui_edit(const char *name){const char *data=fs_read(disk,name);if(!data)return -1;text_copy(filename,name);text_copy(selected,name);text_copy(filetext,data);text_copy(edit_original,data);edit_cursor=(unsigned)text_len(data);edit_dirty=edit_select=0;dialog=3;gui_render();return 0;}
static void editor_close(void){if(edit_dirty)dialog=4;else{dialog=0;view=5;}}
static void editor_save(void){const char *current=fs_read(disk,filename);if(!current||!text_equal(current,edit_original))return;char line[552];text_copy(line,"write ");text_copy(line+6,filename);unsigned n=(unsigned)text_len(line);line[n++]=' ';text_copy(line+n,filetext);dispatch(line);current=fs_read(disk,filename);if(current&&text_equal(current,filetext)){dialog=0;view=5;edit_dirty=0;}}
void gui_mouse(unsigned new_buttons,int dx,int dy){int oldx=mx,oldy=my;mx+=dx;my-=dy;if(mx<0)mx=0;if(mx>319)mx=319;if(my<0)my=0;if(my>199)my=199;int clicked=(new_buttons&1)&&!(buttons&1);buttons=(int)new_buttons;
 if(clicked){if(dialog==4){if(my>=113&&my<=131){if(mx>=44&&mx<=154){dialog=0;view=5;}else if(mx>=167&&mx<=277)dialog=3;}}else if(dialog==3){if(my>=151&&my<=169){if(mx>=82&&mx<=188)editor_save();else if(mx>=202&&mx<=308)editor_close();}}else if(dialog==2){if(my>=113&&my<=131){if(mx>=44&&mx<=154)delete_file();else if(mx>=167&&mx<=277)dialog=0;}}else if(dialog){if(my>=68&&my<=81)field=0;else if(my>=98&&my<=111)field=1;else if(my>=121&&my<=139){if(mx>=44&&mx<=154)create_file();else if(mx>=167&&mx<=277)dialog=0;}}
 else if(mx>=4&&mx<=70&&my>=25&&my<175){unsigned tab=(unsigned)(my-25)/25;if(tab==5)tab=6;if(tab==1)create_dialog();else{view=tab;if(tab==0){char c[]="ls";dispatch(c);}else if(tab==2){char c[]="mem";dispatch(c);}else if(tab==3){char c[]="selftest";dispatch(c);}}}
 else if(view==6&&my>=149&&my<=167){char c[10];if(mx>=82&&mx<=152)text_copy(c,"hello");else if(mx>=158&&mx<=228)text_copy(c,"stop");else if(mx>=234&&mx<=308)text_copy(c,"generate");else c[0]=0;if(c[0])dispatch(c);}
 else if(view==5&&mx>=82&&mx<=152&&my>=153&&my<=171){gui_edit(selected);}
 else if(view==5&&mx>=160&&mx<=230&&my>=153&&my<=171){char c[40];text_copy(c,"run ");text_copy(c+4,selected);dispatch(c);}
 else if(view==5&&mx>=238&&mx<=308&&my>=153&&my<=171){dialog=2;}
 else if(view==0&&mx>=82&&mx<=310&&my>=45){unsigned wanted=(unsigned)(my-45)/15,row=0;for(unsigned i=0;i<FILE_COUNT;++i)if(disk->files[i].used){if(row++==wanted){text_copy(selected,disk->files[i].name);view=5;char c[40];text_copy(c,"cat ");text_copy(c+4,selected);dispatch(c);break;}}}}
 if(clicked)gui_render();else{
#ifdef ORIGIN_GUI_TEST
 gui_test_pointer_bytes+=60;
#endif
 for(int yy=0;yy<8;++yy)for(int xx=0;xx<5;++xx)if(oldx+xx<320&&oldy+yy<200)screen[(oldy+yy)*320+oldx+xx]=pixels[(oldy+yy)*320+oldx+xx];
 for(int yy=0;yy<8;++yy)for(int xx=0;xx<=yy/2;++xx)if(mx+xx<320&&my+yy<200)screen[(my+yy)*320+mx+xx]=(unsigned char)(xx==0||xx==yy/2?0:15);}
}
int gui_key(int c){if(!dialog)return 0;
 if(dialog==4){if(c=='y'||c=='Y'){dialog=0;view=5;}else if(c=='n'||c=='N'||c==27)dialog=3;gui_render();return 1;}
 if(dialog==3){unsigned n=(unsigned)text_len(filetext);if(c==27)editor_close();else if(c==19)editor_save();else if(c==1)edit_select=1;
 else if(c==1001){if(edit_cursor)--edit_cursor;edit_select=0;}else if(c==1002){if(edit_cursor<n)++edit_cursor;edit_select=0;}else if(c==1004){edit_cursor=0;edit_select=0;}else if(c==1005){edit_cursor=n;edit_select=0;}
 else if(c=='\b'||c==127||c==1003){if(edit_select){filetext[0]=0;edit_cursor=0;edit_select=0;edit_dirty=1;}else if(c==1003&&edit_cursor<n){for(unsigned i=edit_cursor;i<n;++i)filetext[i]=filetext[i+1];edit_dirty=1;}else if(c!=1003&&edit_cursor){for(unsigned i=edit_cursor-1;i<n;++i)filetext[i]=filetext[i+1];--edit_cursor;edit_dirty=1;}}
 else if((c>=32&&c<=126)||c=='\n'||c=='\r'){if(edit_select){n=0;filetext[0]=0;edit_cursor=0;edit_select=0;}if(n<511){for(unsigned i=n+1;i>edit_cursor;--i)filetext[i]=filetext[i-1];filetext[edit_cursor++]=(char)(c=='\r'?'\n':c);edit_dirty=1;}}
 gui_render();return 1;}
if(c==27){dialog=0;gui_render();return 1;}if(dialog==2){if(c=='\n'||c=='\r')delete_file();gui_render();return 1;}if(c=='\t'){field^=1;gui_render();return 1;}if(c=='\n'||c=='\r'){if(field==0)field=1;else create_file();gui_render();return 1;}char *target=field?filetext:filename;size_t n=text_len(target),limit=field?120:31;if(c=='\b'||c==127){if(n)target[n-1]=0;}else if(c>=32&&c<=126&&n<limit){target[n]=(char)c;target[n+1]=0;}gui_render();return 1;}
void gui_init(RamFS *fs,Arena *arena,void (*execute)(char *)){disk=fs;memory=arena;dispatch=execute;view=0;gui_clear();gui_render();}

void gui_training(unsigned step,const char *loss,const char *output,int active,unsigned fingerprint){nn_step=step;nn_hash=fingerprint;nn_active=active;text_copy(nn_loss,loss);text_copy(nn_output,output);if(!dialog&&step==0)view=6;if(!dialog&&view==6)gui_render();}

void gui_console(void){if(!dialog)view=4;gui_render();}
