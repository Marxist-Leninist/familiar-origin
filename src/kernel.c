#include "core.h"
#include "gui.h"
#define COM1 0x3f8
static RamFS fs;
static char pending_delete[NAME_SIZE];
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
    static unsigned shift, caps, extended;
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
    if(extended){extended=0;return -1;}
    if(s==42){shift|=1;return -1;} if(s==54){shift|=2;return -1;}
    if(s==170){shift&=~1u;return -1;} if(s==182){shift&=~2u;return -1;}
    if(s&0x80)return -1;
    if(s==58){caps^=1;return -1;}
    char c=plain[s];
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
    vendor[12]=0;puts("CPU vendor: ");puts(vendor);puts("; 32-bit protected mode\n");
}
static void prompt(void) {
    puts("origin> ");serial_byte(30);puts("");
    const char *marker="ORIGIN_READY";while(*marker)serial_byte(*marker++);serial_byte(31);gui_render();
}
static void command(char *line) {
    char *p=line; char *cmd=next_word(&p); if(!cmd)return;
    if(pending_delete[0]){
        if(text_equal(cmd,"yes")||text_equal(cmd,"y")){puts(fs_remove(&fs,pending_delete)?"File not found.\n":"Removed from RAM.\n");pending_delete[0]=0;}
        else if(text_equal(cmd,"no")){pending_delete[0]=0;puts("Deletion cancelled.\n");}
        else puts("Deletion pending. Type yes or no; no file changed.\n");
        return;
    }
    static const char *const short_names[]={"h","a","c","m","t","l","r","w","d","b","e","x","q","lf","rf","df","st","n"};
    static const char *const long_names[]={"help","about","cpu","mem","selftest","ls","cat","write","rm","alloc","echo","clear","halt","ls","cat","rm","selftest","new"};
    for(unsigned i=0;i<sizeof(short_names)/sizeof(short_names[0]);++i)if(text_equal(cmd,short_names[i])){cmd=(char*)long_names[i];break;}
    if(text_equal(cmd,"run")){
        cmd=next_word(&p);
        if(!cmd){puts("Built-in utilities: help about cpu mem selftest ls cat\n");return;}
        if(!(text_equal(cmd,"help")||text_equal(cmd,"about")||text_equal(cmd,"cpu")||text_equal(cmd,"mem")||text_equal(cmd,"selftest")||text_equal(cmd,"ls")||text_equal(cmd,"cat"))){puts("External executable not supported. Built-ins: help about cpu mem selftest ls cat\n");return;}
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
    if(text_equal(cmd,"help"))puts("help about cpu mem alloc selftest ls cat write new rm echo clear halt run\nShortcuts: h help, a about, c cpu, m mem, t selftest, l ls, r cat\nw write, n new (no overwrite), d rm, b alloc, e echo, x clear, q halt\nUse: w NAME TEXT | r NAME | d NAME | b BYTES\nFiles are RAM-only: all changes disappear on reboot.\n");
    else if(text_equal(cmd,"about"))puts("Familiar Origin 0.1.1: original research kernel, no Linux kernel.\nBIOS x86 VM prototype. No network, browser, app ABI or process isolation.\nSingle-user console; no password is requested or stored.\nNot safe for sensitive data or use as your daily operating system.\n");
    else if(text_equal(cmd,"cpu"))cpu();
    else if(text_equal(cmd,"mem")){puts("Dedicated kernel arena: ");number((uint32_t)arena.size);puts(" bytes; used: ");number((uint32_t)arena.used);puts("\nRAM files: 8 slots, 511 bytes per file. This is not detected system RAM.\n");}
    else if(text_equal(cmd,"alloc")){
        uint32_t size;char *word=next_word(&p);
        if(!parse_uint(word,&size)||next_word(&p)){puts("Usage: alloc BYTES (positive decimal integer).\n");return;}
        volatile unsigned char *memory=arena_alloc(&arena,size);
        if(!memory)puts("Allocation rejected: zero size or arena full.\n");
        else{memory[0]=0;memory[size-1]=0;puts("Allocated ");number(size);puts(" bytes. No free operation in this prototype.\n");}
    }
    else if(text_equal(cmd,"selftest"))puts(core_selftest()?"SELFTEST PASS: allocator, RAM files, tokenizer\n":"SELFTEST FAIL\n");
    else if(text_equal(cmd,"ls")){for(unsigned i=0;i<FILE_COUNT;++i)if(fs.files[i].used){puts(fs.files[i].name);puts("  ");number((uint32_t)text_len(fs.files[i].data));puts(" bytes\n");}}
    else if(text_equal(cmd,"cat")||text_equal(cmd,"rm")||text_equal(cmd,"write")||text_equal(cmd,"new")){
        char *name=next_word(&p);if(!name){puts("Missing file name. Type help.\n");return;}
        if(text_equal(cmd,"cat")){const char *data=fs_read(&fs,name);if(data){puts(data);puts("\n");}else puts("File not found.\n");}
        else if(text_equal(cmd,"rm")){if(!fs_read(&fs,name))puts("File not found.\n");else{text_copy(pending_delete,name);puts("Delete RAM file ");puts(name);puts("? Type yes or no.\n");}}
        else{if(text_equal(cmd,"new")&&fs_read(&fs,name)){puts("File exists; not overwritten.\n");return;}while(*p==' ')++p;int r=fs_write(&fs,name,p);puts(r==-2?"RAM filesystem full.\n":r?"Invalid name or content too long.\n":"Saved to RAM only.\n");}
    }
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
    serial_init();fs_init(&fs);
    fs_write(&fs,"welcome.txt","Your own kernel is running. Type help to explore.");
    fs_write(&fs,"roadmap.txt","Next: interrupts, memory protection, processes, storage, UEFI and ARM64.\nClaude, Codex and Edge compatibility is a goal, not implemented.");
    gui_init(&fs,&arena,execute_line);mouse_init();
    puts("FAMILIAR ORIGIN 0.1.1\nOriginal BIOS loader + freestanding C kernel\n");
    puts(core_selftest()?"BOOT SELFTEST PASS\n":"BOOT SELFTEST FAIL\n");
    puts("No login required in this single-user VM prototype.\nCommands: h help | l files | r read | w write | d delete | m memory\nMore: a about | c CPU | t tests | n new file | b allocate | e echo | x clear | q halt\nYour files live only in RAM.\n");prompt();
    char line[640];unsigned n=0;int overflow=0,previous_cr=0;
    for(;;){
        int c=(inb(COM1+5)&1)?inb(COM1):keyboard();
        if(c<0){__asm__ volatile("pause");continue;}
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
