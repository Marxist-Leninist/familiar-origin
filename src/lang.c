/* Original tiny integer/statement interpreter. Explicit resource limits. */
#include "core.h"
#include "lang.h"
#define MAX_LINES 64
#define MAX_VARS 8
#define MAX_STEPS 1000
static char program[512],*lines[MAX_LINES],names[MAX_VARS][16];
static int32_t values[MAX_VARS];static unsigned count,vars,steps,current_line;
static const char *error,*cursor;static unsigned expression_depth;
static void (*emit)(const char *);static void (*invoke)(const char *);
static int space(char c){return c==' '||c=='\t'||c=='\r';}
static void skip(void){while(space(*cursor))++cursor;}
static int letter(char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
static int digit(char c){return c>='0'&&c<='9';}
static int identifier(char name[16]){skip();unsigned n=0;if(!letter(*cursor)){error="Expected a variable name.";return 0;}while(letter(*cursor)||digit(*cursor)){if(n>=15){error="Variable name exceeds 15 characters.";return 0;}name[n++]=*cursor++;}name[n]=0;return 1;}
static int32_t checked(int64_t n){if(n>2147483647LL||n< -2147483647LL-1){error="Integer overflow.";return 0;}return (int32_t)n;}
static int32_t expression(void);
static int32_t atom(void){skip();if(++expression_depth>8){error="Expression nesting exceeds 8.";--expression_depth;return 0;}int32_t result=0;if(*cursor=='('){++cursor;result=expression();skip();if(*cursor!=')')error="Expected closing parenthesis.";else ++cursor;}else if(*cursor=='-'||*cursor=='+'){char op=*cursor++;result=atom();if(op=='-')result=checked(-(int64_t)result);}else if(digit(*cursor)){uint32_t n=0;while(digit(*cursor)){unsigned d=(unsigned)(*cursor++-'0');if(n>214748364u||(n==214748364u&&d>7)){error="Integer literal is too large.";break;}n=n*10+d;}result=(int32_t)n;}else if(letter(*cursor)){char name[16];if(identifier(name)){unsigned i=0;while(i<vars&&!text_equal(name,names[i]))++i;if(i==vars)error="Unknown variable.";else result=values[i];}}else error="Expected an integer, variable or parenthesized expression.";--expression_depth;return result;}
static int32_t product(void){int32_t a=atom();skip();while(!error&&(*cursor=='*'||*cursor=='/')){char op=*cursor++;int32_t b=atom();if(error)break;if(op=='*')a=checked((int64_t)a*b);else if(!b)error="Division by zero.";else if(a==(-2147483647-1)&&b==-1)error="Integer overflow.";else a/=b;skip();}return a;}
static int32_t expression(void){int32_t a=product();skip();while(!error&&(*cursor=='+'||*cursor=='-')){char op=*cursor++;int32_t b=product();a=checked(op=='+'?(int64_t)a+b:(int64_t)a-b);skip();}return a;}
static void integer(char out[16],int32_t value){char temp[12];unsigned n=0,j=0;uint32_t u=value<0?0u-(uint32_t)value:(uint32_t)value;if(value<0)out[j++]='-';do{temp[n++]=(char)('0'+u%10);u/=10;}while(u);while(n)out[j++]=temp[--n];out[j]=0;}
static int starts(const char *s,const char *word){unsigned i=0;while(word[i]){if(s[i]!=word[i])return 0;++i;}return !s[i]||space(s[i]);}
static const char *after(const char *s){while(*s&&!space(*s))++s;while(space(*s))++s;return s;}
static void print_statement(void){skip();char out[512];unsigned n=0;if(*cursor=='"'){++cursor;while(*cursor&&*cursor!='"'){char c=*cursor++;if(c=='\\'){c=*cursor++;if(c=='n')c='\n';else if(c!='"'&&c!='\\'){error="Only newline, quote and backslash escapes are supported.";return;}}if(n>=sizeof(out)-1){error="Printed string is too long.";return;}out[n++]=c;}if(*cursor!='"'){error="Unclosed string.";return;}++cursor;out[n]=0;}else{int32_t v=expression();integer(out,v);}skip();if(*cursor)error="Unexpected text after print expression.";if(!error){emit(out);emit("\n");}}
static void assign_statement(void){char name[16];if(!identifier(name))return;skip();if(*cursor=='=')++cursor;else if(starts(cursor,"to"))cursor=after(cursor);else{error="Use let NAME = expression or set NAME to expression.";return;}int32_t value=expression();skip();if(*cursor)error="Unexpected text after assignment.";if(error)return;unsigned i=0;while(i<vars&&!text_equal(names[i],name))++i;if(i==vars){if(vars==MAX_VARS){error="At most 8 variables are supported.";return;}text_copy(names[vars++],name);}values[i]=value;}
static int run_block(unsigned begin,unsigned end,unsigned depth){if(depth>4){error="Loop nesting exceeds 4.";return 0;}for(unsigned line=begin;line<end&&!error;++line){current_line=line+1;const char *s=lines[line];while(space(*s))++s;if(!*s||*s=='#')continue;if(++steps>MAX_STEPS){error="Execution stopped at 1000 statements.";break;}expression_depth=0;cursor=after(s);
 if(starts(s,"print")||starts(s,"say")||starts(s,"show")||starts(s,"p"))print_statement();
 else if(starts(s,"let")||starts(s,"set")||starts(s,"l"))assign_statement();
 else if(starts(s,"repeat")){int32_t times=expression();skip();if(*cursor)error="Unexpected text after repeat count.";if(times<0||times>32)error="Repeat count must be between 0 and 32.";if(error)break;unsigned level=1,close=line+1;for(;close<end;++close){const char *q=lines[close];while(space(*q))++q;if(starts(q,"repeat"))++level;else if(starts(q,"end")&&!--level)break;}if(close==end){error="Repeat needs a matching end.";break;}for(int32_t i=0;i<times&&!error;++i)run_block(line+1,close,depth+1);line=close;}
 else if(text_equal(s,"end"))error="Unexpected end.";
 else if(text_equal(s,"train hello")){if(invoke)invoke("hello");}
 else if(text_equal(s,"stop training")){if(invoke)invoke("stop");}
 else if(text_equal(s,"generate hello")){if(invoke)invoke("generate");}
 else if(starts(s,"prnit")||starts(s,"pritn")||starts(s,"pirnt"))error="Unknown statement. Did you mean print? Nothing was guessed or executed.";
 else error="Unknown statement. Use print, let, repeat/end or train hello.";
 }return !error;}
int lang_run(const char *source,void (*output)(const char *),void (*action)(const char *)){emit=output;invoke=action;error=0;count=vars=steps=current_line=0;if(text_len(source)>511){emit("Origin Script: source exceeds 511 bytes.\n");return -1;}text_copy(program,source);lines[count++]=program;for(unsigned i=0;program[i];++i)if(program[i]=='\n'){program[i]=0;if(count==MAX_LINES){emit("Origin Script: source exceeds 64 lines.\n");return -1;}lines[count++]=program+i+1;}
 int ok=run_block(0,count,0);if(!ok){char n[16];integer(n,(int32_t)current_line);emit("Origin Script error, line ");emit(n);emit(": ");emit(error);emit("\n");return -1;}return 0;}
