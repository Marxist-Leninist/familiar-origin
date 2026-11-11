#include "lang.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char output[8192],action[32];
static void emit(const char *s){assert(strlen(output)+strlen(s)<sizeof(output));strcat(output,s);}
static void invoke(const char *s){strcpy(action,s);}
static int run(const char *s){output[0]=action[0]=0;return lang_run(s,emit,invoke);}
int main(void){assert(!run("let n = 2\nrepeat 3\nset n to n * 2\nend\nsay n\nprint (4 + 2) * 3\nprint \"hello\\nworld\""));assert(!strcmp(output,"16\n18\nhello\nworld\n"));assert(!run("train hello"));assert(!strcmp(action,"hello"));assert(!run("p -12 / 5"));assert(!strcmp(output,"-2\n"));assert(run("print 1/0"));assert(strstr(output,"Division by zero"));assert(run("print 2147483647+1"));assert(run("print x"));assert(run("repeat 33\nprint 1\nend"));assert(run("repeat 2\nprint 1"));assert(run("prnit 2"));assert(strstr(output,"Did you mean print"));assert(!*action);assert(run("repeat 32\nrepeat 32\nprint 0\nend\nend"));assert(strstr(output,"1000 statements"));puts("PASS Origin Script: arithmetic, precedence, variables, loops, strings, guest action, errors and bounds.");}
