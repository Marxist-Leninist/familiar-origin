#include "core.h"
#include "gui.h"
#include "nn.h"
#include "lang.h"
#define COM1 0x3f8
static RamFS fs;
static char pending_delete[NAME_SIZE];
static int fpu_available,training_active,model_initialized;
static float training_loss;
static unsigned char heap[65536] __attribute__((aligned(16)));
static Arena arena={heap,0,sizeof(heap)};
static inline void outb(uint16_t p,uint8_t v) { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static void serial_init(void) {
    outb(COM1+1,0); outb(COM1+3,0x80); outb(COM1,1); outb(COM1+1,0);
    outb(COM1+3,3); outb(COM1+2,0xc7); outb(COM1+4,0x0b);
}
static void serial_byte(char c) {
    /* Bounded polling: an absent UART must never hang the display console. */
    for (unsigned i=0;i<100000;++i) if (inb(COM1+5)&0x20) { outb(COM1,(uint8_t)c); return; }
}
static void clear_screen(void) {gui_clear();}
static void putc(char c) {if(c=='\n')serial_byte('\r');serial_byte(c);gui_char(c);}
static void puts(const char *s) {while(*s) putc(*s++);}
static void number(uint32_t n) {char b[11];unsigned i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)putc(b[--i]);}
static int keyboard(void) {
    static unsigned shift, caps, extended,ctrl;
    static const char plain[128]={
        [1]=27,[2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
        [12]='-',[13]='=',[14]='\b',[15]='\t',[16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',
        [22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]='\n',[30]='a',[31]='s',[32]='d',
        [33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',[39]=';',[40]='\'',[41]='`',[43]='\\',
        [44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',[51]=',',[52]='.',[53]='/',[57]=' '
    };
    static const char symbols[]="!@#$%^&*()";
    uint8_t status=inb(0x64);if(!(status&1))return -1;
    uint8_t s=inb(0x60);
    if(status&32){static unsigned count;static uint8_t packet[3];if(count==0&&!(s&8))return -1;packet[count++]=s;if(count==3){count=0;if(!(packet[0]&0xc0))gui_mouse(packet[0]&7,(int)(int8_t)packet[1],(int)(int8_t)packet[2]);}return -1;}
    if(s==0xe0 || s==0xe1){extended=1;return -1;}
    if(extended){extended=0;if(s==75)return 1001;if(s==77)return 1002;if(s==83)return 1003;if(s==71)return 1004;if(s==79)return 1005;return -1;}
    if(s==29){ctrl=1;return -1;}if(s==157){ctrl=0;return -1;}
    if(s==42){shift|=1;return -1;} if(s==54){shift|=2;return -1;}
    if(s==170){shift&=~1u;return -1;} if(s==182){shift&=~2u;return -1;}
    if(s&0x80)return -1;
    if(s==58){caps^=1;return -1;}
    char c=plain[s];if(ctrl&&c=='s')return 19;if(ctrl&&c=='a')return 1;
    if(c>='a'&&c<='z') {if(!!shift^caps)c=(char)(c-'a'+'A');}
    else if(shift) {
        if(s>=2&&s<=11)c=symbols[s-2];
        else switch(c){case '-':c='_';break;case '=':c='+';break;case '[':c='{';break;case ']':c='}';break;
            case ';':c=':';break;case '\'':c='"';break;case '`':c='~';break;case '\\':c='|';break;
            case ',':c='<';break;case '.':c='>';break;case '/':c='?';break;default:break;}
    }
    return c?c:-1;
}
static void cpu(void) {
    uint32_t a=0,b,c,d; char vendor[13];
    __asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d));
    for(unsigned i=0;i<4;++i){vendor[i]=(char)(b>>(8*i));vendor[4+i]=(char)(d>>(8*i));vendor[8+i]=(char)(c>>(8*i));}
    vendor[12]=0;puts("CPU vendor: ");puts(vendor);
#if UINTPTR_MAX > 0xffffffffu
    unsigned lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080u));puts("; 64-bit long mode; pointer bytes: ");number((unsigned)sizeof(void*));puts("; EFER.LMA: ");number((lo>>10)&1);puts("\n");
#else
    puts("; 32-bit protected mode\n");
#endif
}
static void prompt(void) {
    puts("origin> ");serial_byte(30);puts("");
    const char *marker="ORIGIN_READY";while(*marker)serial_byte(*marker++);serial_byte(31);gui_render();
}
static void loss_text(char out[20],float value){unsigned n=(unsigned)(value*1000000.0f),whole=n/1000000,fraction=n%1000000;out[0]=(char)('0'+whole%10);out[1]='.';for(unsigned i=0;i<6;++i){out[7-i]=(char)('0'+fraction%10);fraction/=10;}out[8]=0;}
static void model_report(void){char output[16],loss[20];nn_generate(output);loss_text(loss,training_loss);puts("NN step ");number(nn_steps());puts(" loss ");puts(loss);puts(" weights ");number(nn_fingerprint());puts(" predicted [");puts(output);puts("]\n");gui_training(nn_steps(),loss,output,training_active,nn_fingerprint());}
static void training_tick(void){if(!training_active)return;training_loss=nn_train_step();if(nn_steps()>=NN_STEPS)training_active=0;if(nn_steps()%10==0||!training_active)model_report();if(!training_active){puts("GUEST TRANSFORMER TRAINING COMPLETE\n");}}
static int fpu_init(void){unsigned a=1,b,c,d;__asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d));if(!(d&1))return 0;uintptr_t cr0;__asm__ volatile("mov %%cr0,%0":"=r"(cr0));cr0=(cr0&~12u)|2u;__asm__ volatile("mov %0,%%cr0; fninit"::"r"(cr0):"memory");return 1;}
static void command(char *line);
static void script_action(const char *action){char copy[16];text_copy(copy,action);command(copy);}
static void command(char *line) {
    char *p=line; char *cmd=next_word(&p); if(!cmd)return;
    if(pending_delete[0]){
        if(text_equal(cmd,"yes")||text_equal(cmd,"y")){puts(fs_remove(&fs,pending_delete)?"File not found.\n":"Removed from RAM.\n");pending_delete[0]=0;}
        else if(text_equal(cmd,"no")){pending_delete[0]=0;puts("Deletion cancelled.\n");}
        else puts("Deletion pending. Type yes or no; no file changed.\n");
        return;
    }
    static const char *const short_names[]={"h","a","c","m","t","l","r","w","d","b","e","x","q","lf","rf","df","st","n","i","s","g","o"};
    static const char *const long_names[]={"help","about","cpu","mem","selftest","ls","cat","write","rm","alloc","echo","clear","halt","ls","cat","rm","selftest","new","hello","stop","generate","edit"};
    for(unsigned i=0;i<sizeof(short_names)/sizeof(short_names[0]);++i)if(text_equal(cmd,short_names[i])){cmd=(char*)long_names[i];break;}
    if(text_equal(cmd,"run")){
        cmd=next_word(&p);
        if(!cmd){puts("Built-in utilities: help about cpu mem selftest ls cat hello\n");return;}
        if(!(text_equal(cmd,"help")||text_equal(cmd,"about")||text_equal(cmd,"cpu")||text_equal(cmd,"mem")||text_equal(cmd,"selftest")||text_equal(cmd,"ls")||text_equal(cmd,"cat")||text_equal(cmd,"hello"))){
            const char *source=fs_read(&fs,cmd);size_t length=text_len(cmd);
            if(source&&length>3&&text_equal(cmd+length-3,".fo")){gui_console();puts("Running Origin Script: ");puts(cmd);puts("\n");if(!lang_run(source,puts,script_action))puts("Origin Script complete.\n");return;}
            puts("External executable not supported. Built-ins: help about cpu mem selftest ls cat hello\nOrigin Script: run NAME.fo (a real RAM file). No Python runtime yet.\n");return;}
        puts("Running built-in utility: ");puts(cmd);puts("\n");
    }
    if(text_equal(cmd,"read"))cmd="cat";
    else if(text_equal(cmd,"delete")||text_equal(cmd,"remove"))cmd="rm";
    else if(text_equal(cmd,"list"))cmd="ls";
    else if(text_equal(cmd,"test"))cmd="selftest";
    else if(text_equal(cmd,"show")){char *what=next_word(&p);if(what){
        if(text_equal(what,"files"))cmd="ls";else if(text_equal(what,"memory"))cmd="mem";
        else if(text_equal(what,"cpu"))cmd="cpu";else if(text_equal(what,"help"))cmd="help";
    }}
    if(text_equal(cmd,"help"))puts("help about cpu mem alloc selftest ls cat write new rm echo clear halt run hello stop generate nnstatus\nShortcuts: h help, a about, c cpu, m mem, t selftest, l ls, r cat\nw write, n new (no overwrite), d rm, b alloc, e echo, x clear, q halt\nNeural: i hello/train, s stop, g generate. Actual guest training. Editor: o NAME or edit NAME.\nUse: w NAME TEXT | r NAME | d NAME | b BYTES\nFiles are RAM-only: all changes disappear on reboot.\n");
    else if(text_equal(cmd,"about"))puts("Familiar Origin 0.1.3: original research kernel, no Linux kernel.\nBIOS x86 VM prototype. No network, browser, app ABI or process isolation.\nSingle-user console; no password is requested or stored.\nNot safe for sensitive data or use as your daily operating system.\n");
    else if(text_equal(cmd,"cpu"))cpu();
    else if(text_equal(cmd,"mem")){puts("Dedicated kernel arena: ");number((uint32_t)arena.size);puts(" bytes; used: ");number((uint32_t)arena.used);puts("\nRAM files: 8 slots, 511 bytes per file. This is not detected system RAM.\n");}
    else if(text_equal(cmd,"alloc")){
        uint32_t size;char *word=next_word(&p);
        if(!parse_uint(word,&size)||next_word(&p)){puts("Usage: alloc BYTES (positive decimal integer).\n");return;}
        volatile unsigned char *memory=arena_alloc(&arena,size);
        if(!memory)puts("Allocation rejected: zero size or arena full.\n");
        else{memory[0]=0;memory[size-1]=0;puts("Allocated ");number(size);puts(" bytes. No free operation in this prototype.\n");}
    }
    else if(text_equal(cmd,"hello")||text_equal(cmd,"train")){if(!fpu_available){puts("This guest CPU has no supported x87 FPU. Training unavailable.\n");return;}nn_init(7);model_initialized=1;training_active=1;training_loss=nn_loss(0);puts("Training actual guest Transformer: 816 parameters, 250 bounded steps.\nPress s or type stop to stop. Weights and data stay in guest RAM.\n");model_report();}
    else if(text_equal(cmd,"stop")){training_active=0;puts("Guest training stopped. Current weights retained in RAM.\n");if(model_initialized)model_report();}
    else if(text_equal(cmd,"generate")||text_equal(cmd,"nnstatus")){if(model_initialized)model_report();else puts("Train first with hello or i. No model is initialized.\n");}
    else if(text_equal(cmd,"selftest"))puts(core_selftest()?"SELFTEST PASS: allocator, RAM files, tokenizer\n":"SELFTEST FAIL\n");
    else if(text_equal(cmd,"ls")){for(unsigned i=0;i<FILE_COUNT;++i)if(fs.files[i].used){puts(fs.files[i].name);puts("  ");number((uint32_t)text_len(fs.files[i].data));puts(" bytes\n");}}
    else if(text_equal(cmd,"cat")||text_equal(cmd,"rm")||text_equal(cmd,"write")||text_equal(cmd,"new")){
        char *name=next_word(&p);if(!name){puts("Missing file name. Type help.\n");return;}
        if(text_equal(cmd,"cat")){const char *data=fs_read(&fs,name);if(data){puts(data);puts("\n");}else puts("File not found.\n");}
        else if(text_equal(cmd,"rm")){if(!fs_read(&fs,name))puts("File not found.\n");else{text_copy(pending_delete,name);puts("Delete RAM file ");puts(name);puts("? Type yes or no.\n");}}
        else{if(text_equal(cmd,"new")&&fs_read(&fs,name)){puts("File exists; not overwritten.\n");return;}while(*p==' ')++p;int r=fs_write(&fs,name,p);puts(r==-2?"RAM filesystem full.\n":r?"Invalid name or content too long.\n":"Saved to RAM only.\n");}
    }
    else if(text_equal(cmd,"code")||text_equal(cmd,"k")){while(*p==' ')++p;gui_console();if(!lang_run(p,puts,script_action))puts("Origin Script complete.\n");}
    else if(text_equal(cmd,"language")){puts("Origin Script v0: real bounded interpreter, not Python.\nlet x = 2 | set x to x+1 | print x | say \"text\"\nrepeat COUNT ... end (separate lines in a .fo RAM file)\ntrain hello | stop training | generate hello\nRun: run NAME.fo | one line: code print 2+3*4\nLimits: 511 source bytes, 8 variables, 64 lines, 1000 statements.\n");}
    else if(text_equal(cmd,"edit")){char *name=next_word(&p);if(!name||gui_edit(name))puts("File not found. Use edit NAME or o NAME.\n");else puts("Native editor opened. Ctrl-S saves; Escape closes with unsaved confirmation.\n");}
    else if(text_equal(cmd,"echo")){while(*p==' ')++p;puts(p);puts("\n");}
    else if(text_equal(cmd,"clear")){clear_screen();puts("Console cleared.\n");}
    else if(text_equal(cmd,"halt")){puts("CPU halted. Close the VM to stop it.\n");for(;;)__asm__ volatile("cli; hlt");}
    else puts("Unknown command. Type help.\n");
}
static void execute_line(char *line){
    /* A new graphical action must not confirm a stale, differently named target. */
    if(pending_delete[0]&&!text_equal(line,"yes")&&!text_equal(line,"no")){pending_delete[0]=0;puts("Previous deletion cancelled by new graphical action.\n");}
    command(line);prompt();
}
static void ps2_wait(void){for(unsigned i=0;i<100000;++i)if(!(inb(0x64)&2))return;}
static void mouse_command(uint8_t command){ps2_wait();outb(0x64,0xd4);ps2_wait();outb(0x60,command);for(unsigned i=0;i<100000;++i)if(inb(0x64)&1){(void)inb(0x60);break;}}
static void mouse_init(void){ps2_wait();outb(0x64,0xa8);mouse_command(0xf6);mouse_command(0xf4);}
void kernel_main(void) {
    serial_init();fpu_available=fpu_init();fs_init(&fs);
    fs_write(&fs,"welcome.txt","Your own kernel is running. Type help to explore.");
    fs_write(&fs,"roadmap.txt","Next: interrupts, memory protection, processes, storage, UEFI and ARM64.\nClaude, Codex and Edge compatibility is a goal, not implemented.");
    gui_init(&fs,&arena,execute_line);mouse_init();
    puts("FAMILIAR ORIGIN 0.1.3\nOriginal BIOS loader + freestanding C kernel\n");
    puts(core_selftest()?"BOOT SELFTEST PASS\n":"BOOT SELFTEST FAIL\n");
    puts("No login required in this single-user VM prototype.\nCommands: h help | l files | r read | w write | d delete | m memory\nMore: a about | c CPU | t tests | n new file | b allocate | e echo | x clear | q halt\nYour files live only in RAM.\n");prompt();
    char line[640];unsigned n=0;int overflow=0,previous_cr=0;
    for(;;){
        int c=(inb(COM1+5)&1)?inb(COM1):keyboard();
        if(c<0){training_tick();__asm__ volatile("pause");continue;}
        if(gui_key(c))continue;
        if(c=='\n'&&previous_cr){previous_cr=0;continue;}previous_cr=(c=='\r');
        if(c=='\n'||c=='\r'){
            putc('\n');line[n]=0;
            if(overflow)puts("Input too long; command discarded.\n");else command(line);
            n=0;line[0]=0;overflow=0;gui_input(line);prompt();
        }else if(c=='\b'||c==127){if(!overflow&&n){--n;putc('\b');}}
        else if(c>=32&&c<=126){if(n<sizeof(line)-1&&!overflow){line[n++]=(char)c;putc((char)c);}else overflow=1;}
        line[n]=0;gui_input(line);gui_render();
    }
}
