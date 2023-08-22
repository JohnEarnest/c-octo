/**
*
*  octo_compiler.h
*
*  a compiler for Octo CHIP-8 assembly language,
*  suitable for embedding in other tools and environments.
*  depends only upon the C standard library.
*
*  the public interface is octo_compile_str(char*);
*  the result will contain a 64k ROM image in the
*  'rom' field of the returned octo_program.
*  octo_free_program can clean up the entire structure
*  when a consumer is finished using it.
*
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/**
*
*  Fundamental Data Structures
*
**/

#define OCTO_LIST_BLOCK_SIZE 16
#define OCTO_RAM_MAX         (64*1024)
#define OCTO_INTERN_MAX      (64*1024)
#define OCTO_ERR_MAX         4096
#define OCTO_DESTRUCTOR(x) ((void(*)(void*))x)
double octo_sign(double x){return x<0?-1: x>0?1: 0;}
double octo_max (double x,double y){return x<y?y:x;}
double octo_min (double x,double y){return x<y?x:y;}

typedef struct {int count,space;void** data;} octo_list;
void octo_list_init(octo_list* list) {
  list->count=0;
  list->space=OCTO_LIST_BLOCK_SIZE;
  list->data=malloc(sizeof(void*)*OCTO_LIST_BLOCK_SIZE);
}
void octo_list_destroy(octo_list* list,void items(void*)) {
  if(items)for(int z=0;z<list->count;z++)items(list->data[z]);
  free(list->data);
}
void octo_list_grow(octo_list* list) {
  if(list->count<list->space)return;
  list->data=realloc(list->data,(sizeof(void*))*(list->space+=OCTO_LIST_BLOCK_SIZE));
}
void octo_list_append(octo_list* list, void* item) {
  octo_list_grow(list);
  list->data[list->count++]=item;
}
void octo_list_insert(octo_list* list, void* item, int index) {
  octo_list_grow(list);
  for(int z=list->count;z>index;z--) list->data[z]=list->data[z-1];
  list->data[index]=item;
  list->count++;
}
void* octo_list_remove(octo_list* list, int index) {
  void* ret=list->data[index];
  for(int z=index;z<list->count-1;z++) list->data[z]=list->data[z+1];
  list->count--, list->data[list->count]=NULL; // paranoia
  return ret;
}
void* octo_list_get(octo_list* list, int index) {
  return list->data[index];
}
void octo_list_set(octo_list* list, int index, void* value) {
  list->data[index]=value;
}

// I could just use lists directly, but this abstraction clarifies intent:
typedef struct{octo_list values;}octo_stack;
void  octo_stack_init    (octo_stack*stack){octo_list_init(&stack->values);}
void  octo_stack_destroy (octo_stack*stack,void items(void*)){octo_list_destroy(&stack->values,items);}
void  octo_stack_push    (octo_stack*stack,void*item){octo_list_append(&stack->values,item);}
void* octo_stack_pop     (octo_stack*stack){return octo_list_remove(&stack->values,stack->values.count-1);}
int   octo_stack_is_empty(octo_stack*stack){return stack->values.count<1;}

// linear access should be good enough for a start;
// octo programs rarely have more than a few thousand constants:
typedef struct {octo_list keys, values;} octo_map;
void octo_map_init(octo_map* map){
  octo_list_init(&map->keys);
  octo_list_init(&map->values);
}
void octo_map_destroy(octo_map* map,void items(void*)){
  octo_list_destroy(&map->keys,NULL);
  octo_list_destroy(&map->values,items);
}
void* octo_map_get(octo_map* map, char* key){
  for(int z=0;z<map->keys.count;z++) {
    if(octo_list_get(&map->keys,z)==key) return octo_list_get(&map->values,z);
  }
  return NULL;
}
void* octo_map_remove(octo_map* map, char* key){
  for(int z=0;z<map->keys.count;z++) {
    if(octo_list_get(&map->keys,z)==key) {
      void* prev=octo_list_get(&map->values,z);
      octo_list_remove(&map->keys,z);
      octo_list_remove(&map->values,z);
      return prev;
    }
  }
  return NULL;
}
void* octo_map_set(octo_map* map, char* key, void* value){
  for(int z=0;z<map->keys.count;z++) {
    if(octo_list_get(&map->keys,z)==key) {
      void* prev=octo_list_get(&map->values,z);
      octo_list_set(&map->values,z,value);
      return prev;
    }
  }
  octo_list_append(&map->keys,  key);
  octo_list_append(&map->values,value);
  return NULL;
}

/**
*
*  Compiler State
*
**/

#define OCTO_TOK_STR 0
#define OCTO_TOK_NUM 1
#define OCTO_TOK_EOF 2
typedef struct {
  int type;
  int line;
  int pos;
  union {
    char*  str_value;
    double num_value;
  };
} octo_tok;

octo_tok* octo_make_tok_null(int line,int pos){
  octo_tok*r=malloc(sizeof(octo_tok));
  return r->type=OCTO_TOK_EOF, r->line=line, r->pos=pos, r->str_value="", r;
}
octo_tok* octo_make_tok_num(int n){
  octo_tok*r=malloc(sizeof(octo_tok));
  return r->type=OCTO_TOK_NUM, r->line=0, r->pos=0, r->num_value=n, r;
}
octo_tok* octo_tok_copy(octo_tok*x){
  octo_tok*r=malloc(sizeof(octo_tok));
  return memcpy(r,x,sizeof(octo_tok)), r;;
}
void octo_tok_list_insert(octo_list*dst,octo_list*src,int index){
  for(int z=0;z<src->count;z++) octo_list_insert(dst,octo_tok_copy(octo_list_get(src,z)),index++);
}
char* octo_tok_value(octo_tok*t,char*d){
  if(t->type==OCTO_TOK_EOF)snprintf(d,255,"<end of file>");
  if(t->type==OCTO_TOK_STR)snprintf(d,255,"'%s'",t->str_value);
  if(t->type==OCTO_TOK_NUM)snprintf(d,255,"%d",(int)t->num_value);
  return d;
}

typedef struct { double value; char is_mutable;                       } octo_const;
typedef struct { int    value;                                        } octo_reg;
typedef struct { int    value; char is_long;                          } octo_pref;
typedef struct { int line,pos; octo_list addrs;                       } octo_proto;
typedef struct { int calls; octo_list args,body;                      } octo_macro;
typedef struct { int calls; char values[256]; octo_macro* modes[256]; } octo_smode;
typedef struct { int addr,line,pos; char* type;                       } octo_flow;
typedef struct { int type,base,len; char* format;                     } octo_mon;

octo_const* octo_make_const(double v,char m){octo_const*r=calloc(1,sizeof(octo_const));r->value=v,r->is_mutable=m;                       return r;}
octo_reg  * octo_make_reg  (int v)          {octo_reg  *r=calloc(1,sizeof(octo_reg  ));r->value=v;                                       return r;}
octo_pref * octo_make_pref (int a,char l)   {octo_pref *r=calloc(1,sizeof(octo_pref ));r->value=a;r->is_long=l;                          return r;}
octo_proto* octo_make_proto(int l,int p)    {octo_proto*r=calloc(1,sizeof(octo_proto));octo_list_init(&r->addrs);r->line=l,r->pos=p;     return r;}
octo_macro* octo_make_macro(void)           {octo_macro*r=calloc(1,sizeof(octo_macro));octo_list_init(&r->args),octo_list_init(&r->body);return r;}
octo_smode* octo_make_smode(void)           {octo_smode*r=calloc(1,sizeof(octo_smode));                                                  return r;}
octo_flow * octo_make_flow (int a,int l,int p,char*t){octo_flow*r=calloc(1,sizeof(octo_flow));r->addr=a,r->line=l,r->pos=p,r->type=t;    return r;}
octo_mon  * octo_make_mon  (void)           {octo_mon  *r=calloc(1,sizeof(octo_mon  ));                                                  return r;}

void octo_free_tok  (octo_tok  *x) {free(x);}
void octo_free_const(octo_const*x) {free(x);}
void octo_free_reg  (octo_reg  *x) {free(x);}
void octo_free_pref (octo_pref *x) {free(x);}
void octo_free_proto(octo_proto*x) {octo_list_destroy(&x->addrs,OCTO_DESTRUCTOR(octo_free_pref));free(x);}
void octo_free_macro(octo_macro*x) {octo_list_destroy(&x->args,NULL);octo_list_destroy(&x->body,OCTO_DESTRUCTOR(octo_free_tok));free(x);}
void octo_free_smode(octo_smode*x) {for(int z=0;z<256;z++)if(x->modes[z])octo_free_macro(x->modes[z]);free(x);}
void octo_free_flow (octo_flow *x) {free(x);}
void octo_free_mon  (octo_mon  *x) {free(x);}

typedef struct {
  // string interning table
  size_t    strings_used;
  char      strings[OCTO_INTERN_MAX];

  // tokenizer
  char*     source;
  char*     source_root;
  int       source_line;
  int       source_pos;
  octo_list tokens;

  // compiler
  char       has_main;    // do we need a trampoline for 'main'?
  int        here;
  int        length;
  char       rom [OCTO_RAM_MAX];
  char       used[OCTO_RAM_MAX];
  octo_map   constants;   // name -> octo_const
  octo_map   aliases;     // name -> octo_reg
  octo_map   protos;      // name -> octo_proto
  octo_map   macros;      // name -> octo_macro
  octo_map   stringmodes; // name -> octo_smode
  octo_stack loops;       // [octo_flow]
  octo_stack branches;    // [octo_flow]
  octo_stack whiles;      // [octo_flow], value=-1 indicates a marker

  // debugging
  char*      breakpoints[OCTO_RAM_MAX];
  octo_map   monitors; // name -> octo_mon

  // error reporting
  char       is_error;
  char       error[OCTO_ERR_MAX];
  int        error_line;
  int        error_pos;
} octo_program;

void octo_free_program(octo_program*p){
  free(p->source_root);
  octo_list_destroy (&p->tokens     ,OCTO_DESTRUCTOR(octo_free_tok  ));
  octo_map_destroy  (&p->constants  ,OCTO_DESTRUCTOR(octo_free_const));
  octo_map_destroy  (&p->aliases    ,OCTO_DESTRUCTOR(octo_free_reg  ));
  octo_map_destroy  (&p->protos     ,OCTO_DESTRUCTOR(octo_free_proto));
  octo_map_destroy  (&p->macros     ,OCTO_DESTRUCTOR(octo_free_macro));
  octo_map_destroy  (&p->stringmodes,OCTO_DESTRUCTOR(octo_free_smode));
  octo_stack_destroy(&p->loops      ,OCTO_DESTRUCTOR(octo_free_flow ));
  octo_stack_destroy(&p->branches   ,OCTO_DESTRUCTOR(octo_free_flow ));
  octo_stack_destroy(&p->whiles     ,OCTO_DESTRUCTOR(octo_free_flow ));
  octo_map_destroy  (&p->monitors   ,OCTO_DESTRUCTOR(octo_free_mon  ));
  free(p);
}

/**
*
*  Tokenization
*
**/

int octo_interned_len(char* name){
  return (name[-2]<<8)|name[-1];
}
char* octo_intern_counted(octo_program* p,char* name,int length){
  size_t index=2;
  while(index<p->strings_used) {
    if(memcmp(name,p->strings+index,length+1)==0) return p->strings+index;
    index+=octo_interned_len(p->strings+index)+3; // [ \0 , len-lo , len-hi ]
  }
  if(p->strings_used+length+1>=OCTO_INTERN_MAX){
    return p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Internal Error: exhausted string interning table."), "";
  }
  memcpy(p->strings+index,name,length);
  p->strings[index-2]=0xFF&(length>>8);
  p->strings[index-1]=0xFF&length;
  p->strings_used+=length+3;
  return p->strings+index;
}
char* octo_intern(octo_program* p, char* name){
  return octo_intern_counted(p,name,strlen(name));
}

int octo_is_end(octo_program*p) {
  return p->tokens.count==0 && p->source[0]=='\0';
}
char octo_next_char(octo_program*p) {
  char c=p->source[0];
  if(c=='\0') return '\0';
  if(c=='\n') p->source_line++, p->source_pos=0;
  else                          p->source_pos++;
  p->source++;
  return c;
}
char octo_peek_char(octo_program*p) {
  return p->source[0]=='\0'?'\0' : p->source[0];
}
void octo_skip_whitespace(octo_program*p) {
  while(1){
    char c=octo_peek_char(p);
    if(c=='#'){ // line comments
      octo_next_char(p);
      while(1){
        char cc=octo_peek_char(p);
        if(cc=='\0'||cc=='\n') break;
        octo_next_char(p);
      }
    }
    else if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n')break;
    octo_next_char(p);
  }
}
void octo_fetch_token(octo_program*p) {
  if(octo_is_end(p)){p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Unexpected EOF.");return;}
  if(p->is_error) return;
  octo_tok* t=malloc(sizeof(octo_tok));
  octo_list_append(&p->tokens, t);
  t->line=p->source_line, t->pos=p->source_pos;
  char str_buffer[4096]; int index=0;
  if(p->source[0]=='"'){
    octo_next_char(p);
    while(1){
      char c=octo_next_char(p);
      if(c=='\0'){
        p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Missing a closing \" in a string literal.");
        p->error_line=p->source_line, p->error_pos=p->source_pos;
        return;
      }
      if(c=='"'){
        octo_next_char(p);
        break;
      }
      if(c=='\\'){
        char ec=octo_next_char(p);
        if(ec=='\0'){
          p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Missing a closing \" in a string literal.");
          p->error_line=p->source_line, p->error_pos=p->source_pos;
          return;
        }
        if      (ec=='t' ) str_buffer[index++]='\t';
        else if (ec=='n' ) str_buffer[index++]='\n';
        else if (ec=='r' ) str_buffer[index++]='\r';
        else if (ec=='v' ) str_buffer[index++]='\v';
        else if (ec=='0' ) str_buffer[index++]='\0';
        else if (ec=='\\') str_buffer[index++]='\\';
        else if (ec=='"' ) str_buffer[index++]='"';
        else{
          p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Unrecognized escape character '%c' in a string literal.",ec);
          p->error_line=p->source_line, p->error_pos=p->source_pos-1;
          return;
        }
      }
      else{
        str_buffer[index++]=c;
      }
      if(index>=4095){
        p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"String literals must be < 4096 characters long.");
        p->error_line=p->source_line, p->error_pos=p->source_pos;
      }
    }
    str_buffer[index++]='\0';
    t->type=OCTO_TOK_STR, t->str_value=octo_intern_counted(p,str_buffer,index-1);
  }
  else{
    // string or number
    while(1){
      char c=octo_next_char(p);
      if (c==' '||c=='\t'||c=='\r'||c=='\n'||c=='#'||c=='\0')break;
      str_buffer[index++]=c;
    }
    str_buffer[index++]='\0';
    char* float_end;
    double float_val=strtod(str_buffer,&float_end);
    if(float_end[0]=='\0'){
      t->type=OCTO_TOK_NUM, t->num_value=float_val;
    }
    else if(str_buffer[0]=='0'&&str_buffer[1]=='b'){
      t->type=OCTO_TOK_NUM, t->num_value=strtol(str_buffer+2,NULL,2);
    }
    else if(str_buffer[0]=='0'&&str_buffer[1]=='x'){
      t->type=OCTO_TOK_NUM, t->num_value=strtol(str_buffer+2,NULL,16);
    }
    else if(str_buffer[0]=='-'&&str_buffer[1]=='0'&&str_buffer[2]=='b'){
      t->type=OCTO_TOK_NUM, t->num_value=-strtol(str_buffer+3,NULL,2);
    }
    else if(str_buffer[0]=='-'&&str_buffer[1]=='0'&&str_buffer[2]=='x'){
      t->type=OCTO_TOK_NUM, t->num_value=-strtol(str_buffer+3,NULL,16);
    }
    else{
      t->type=OCTO_TOK_STR, t->str_value=octo_intern(p,str_buffer);
    }
    // this is handy for debugging internal errors:
    // if (t->type==OCTO_TOK_STR) printf("RAW TOKEN: %p %s\n", (void*)t->str_value, t->str_value);
    // if (t->type==OCTO_TOK_NUM) printf("RAW TOKEN: %f\n", t->num_value);
  }
  octo_skip_whitespace(p);
}
octo_tok* octo_next(octo_program*p) {
  if(p->tokens.count==0) octo_fetch_token(p);
  if(p->is_error) return octo_make_tok_null(p->source_line,p->source_pos);
  octo_tok*r=octo_list_remove(&p->tokens,0);
  p->error_line=r->line, p->error_pos=r->pos;
  return r;
}
octo_tok* octo_peek(octo_program*p) {
  if(p->tokens.count==0) octo_fetch_token(p);
  if(p->is_error) return octo_make_tok_null(p->source_line,p->source_pos);
  return octo_list_get(&p->tokens,0);
}
int octo_peek_match(octo_program*p,char*name,int index){
  while(!p->is_error&&!octo_is_end(p)&&p->tokens.count<index+1) octo_fetch_token(p);
  if(octo_is_end(p)||p->is_error) return 0;
  octo_tok*t=octo_list_get(&p->tokens,index);
  return t->type==OCTO_TOK_STR&&strcmp(t->str_value,name)==0;
}
int octo_match(octo_program*p,char*name){
  if(octo_peek_match(p,name,0)) return octo_free_tok(octo_next(p)),1;
  return 0;
}

/**
*
*  Parsing
*
**/

char* octo_reserved_words[]={
  ":=","|=","&=","^=","-=","=-","+=",">>=","<<=","==","!=","<",">",
  "<=",">=","key","-key","hex","bighex","random","delay",":",":next",":unpack",
  ":breakpoint",":proto",":alias",":const",":org",";","return","clear","bcd",
  "save","load","buzzer","if","then","begin","else","end","jump","jump0",
  "native","sprite","loop","while","again","scroll-down","scroll-up","scroll-right","scroll-left",
  "lores","hires","loadflags","saveflags","i","audio","plane",":macro",":calc",":byte",
  ":call",":stringmode",":assert",":monitor",":pointer","pitch",
};
int octo_is_reserved(char*name){
  for(size_t z=0;z<sizeof(octo_reserved_words)/sizeof(char*);z++)if(strcmp(name,octo_reserved_words[z])==0)return 1;
  return 0;
}
int octo_check_name(octo_program*p,char*name,char*kind){
  if(p->is_error)return 0;
  if(strncmp("OCTO_",name,5)==0||octo_is_reserved(name)){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"The name '%s' is reserved and cannot be used for a %s.",name,kind);
    return 0;
  }
  return 1;
}
char* octo_string(octo_program*p){
  if(p->is_error)return"";
  octo_tok*t=octo_next(p);
  if(t->type!=OCTO_TOK_STR){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected a string, got %d.",(int)t->num_value);
    octo_free_tok(t);
    return "";
  }
  char*n=t->str_value; octo_free_tok(t);
  return n;
}
char* octo_identifier(octo_program*p,char*kind){
  if(p->is_error)return"";
  octo_tok*t=octo_next(p);
  if(t->type!=OCTO_TOK_STR){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected a name for a %s, got %d.",kind,(int)t->num_value);
    octo_free_tok(t);
    return "";
  }
  char*n=t->str_value; octo_free_tok(t);
  if(!octo_check_name(p,n,kind))return "";
  return n;
}
void octo_expect(octo_program*p,char*name){
  if(p->is_error)return;
  octo_tok*t=octo_next(p);
  if(t->type!=OCTO_TOK_STR||strcmp(t->str_value,name)!=0){
    char d[256];
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected %s, got %s.",name,octo_tok_value(t,d));
  }
  octo_free_tok(t);
}

int octo_is_register(octo_program*p,char*name){
  if(octo_map_get(&p->aliases,name)!=NULL)return 1;
  if(octo_interned_len(name)!=2)return 0;
  if(name[0]!='v'&&name[0]!='V')return 0;
  return isxdigit(name[1]);
}
int octo_peek_is_register(octo_program*p){
  octo_tok*t=octo_peek(p);
  return t->type==OCTO_TOK_STR&&octo_is_register(p,t->str_value);
}
int octo_register(octo_program*p){
  if(p->is_error)return 0;
  octo_tok*t=octo_next(p);
  if(t->type!=OCTO_TOK_STR||!octo_is_register(p,t->str_value)){
    char d[256];
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected register, got %s.",octo_tok_value(t,d));
    return octo_free_tok(t), 0;
  }
  octo_reg*a=octo_map_get(&p->aliases,t->str_value);
  if(a!=NULL)return octo_free_tok(t), a->value;
  char c=tolower(t->str_value[1]);
  return octo_free_tok(t), isdigit(c)?c-'0': 10+(c-'a');
}

int octo_value_range(octo_program*p,int n,int mask){
  if(mask==0xF   &&(n<   0||n>mask)) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Argument %d does not fit in 4 bits- must be in range [0,15].",n);
  if(mask==0xFF  &&(n<-128||n>mask)) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Argument %d does not fit in a byte- must be in range [-128,255].",n);
  if(mask==0xFFF &&(n<   0||n>mask)) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Argument %d does not fit in 12 bits.",n);
  if(mask==0xFFFF&&(n<   0||n>mask)) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Argument %d does not fit in 16 bits.",n);
  return n&mask;
}
void octo_value_fail(octo_program*p,char*w,char*n,int undef){
  if(p->is_error)return;
  if     (octo_is_register(p,n))p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected %s value, but found the register %s.",w,n);
  else if(octo_is_reserved(n))  p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected %s value, but found the keyword '%s'. Missing a token?",w,n);
  else if(undef)                p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected %s value, but found the undefined name '%s'.",w,n);
}
int octo_value_4bit(octo_program*p){
  if(p->is_error)return 0;
  octo_tok*t=octo_next(p);
  if(t->type==OCTO_TOK_NUM){
    int n=t->num_value; octo_free_tok(t);
    return octo_value_range(p,n,0xF);
  }
  char*n=t->str_value; octo_free_tok(t);
  octo_const*c=octo_map_get(&p->constants,n);
  if(c!=NULL)return octo_value_range(p,c->value,0xF);
  return octo_value_fail(p,"a 4-bit",n,1),0;
}
int octo_value_8bit(octo_program*p){
  if(p->is_error)return 0;
  octo_tok*t=octo_next(p);
  if(t->type==OCTO_TOK_NUM){
    int n=t->num_value; octo_free_tok(t);
    return octo_value_range(p,n,0xFF);
  }
  char*n=t->str_value; octo_free_tok(t);
  octo_const*c=octo_map_get(&p->constants,n);
  if(c!=NULL)return octo_value_range(p,c->value,0xFF);
  return octo_value_fail(p,"an 8-bit",n,1),0;
}
int octo_value_12bit(octo_program*p){
  if(p->is_error)return 0;
  octo_tok*t=octo_next(p);
  if(t->type==OCTO_TOK_NUM){
    int n=t->num_value; octo_free_tok(t);
    return octo_value_range(p,n,0xFFF);
  }
  char*n=t->str_value; int proto_line=t->line, proto_pos=t->pos; octo_free_tok(t);
  octo_const*c=octo_map_get(&p->constants,n);
  if(c!=NULL)return octo_value_range(p,c->value,0xFFF);
  octo_value_fail(p,"a 12-bit",n,0);
  if(p->is_error)return 0;
  if(!octo_check_name(p,n,"label"))return 0;
  octo_proto*pr=octo_map_get(&p->protos,n);
  if(pr==NULL)octo_map_set(&p->protos,n,pr=octo_make_proto(proto_line, proto_pos));
  octo_list_append(&pr->addrs,octo_make_pref(p->here,0));
  return 0;
}
int octo_value_16bit(octo_program*p,int can_forward_ref,int offset){
  if(p->is_error)return 0;
  octo_tok*t=octo_next(p);
  if(t->type==OCTO_TOK_NUM){
    int n=t->num_value; octo_free_tok(t);
    return octo_value_range(p,n,0xFFFF);
  }
  char*n=t->str_value; int proto_line=t->line, proto_pos=t->pos; octo_free_tok(t);
  octo_const*c=octo_map_get(&p->constants,n);
  if(c!=NULL)return octo_value_range(p,c->value,0xFFFF);
  octo_value_fail(p,"a 16-bit",n,0);
  if(p->is_error)return 0;
  if(!octo_check_name(p,n,"label"))return 0;
  if(!can_forward_ref){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"The reference to '%s' may not be forward-declared.",n);
    return 0;
  }
  octo_proto*pr=octo_map_get(&p->protos,n);
  if(pr==NULL)octo_map_set(&p->protos,n,pr=octo_make_proto(proto_line, proto_pos));
  octo_list_append(&pr->addrs,octo_make_pref(p->here+offset,1));
  return 0;
}
octo_const* octo_value_constant(octo_program*p){
  octo_tok*t=octo_next(p);
  if(p->is_error)return octo_make_const(0,0);
  if(t->type==OCTO_TOK_NUM){
    int n=t->num_value;
    return octo_free_tok(t),octo_make_const(n,0);
  }
  char*n=t->str_value; octo_free_tok(t);
  octo_const*c=octo_map_get(&p->constants,n);
  if(c!=NULL)return octo_make_const(c->value,0);
  if(octo_map_get(&p->protos,n)!=NULL) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"A constant reference to '%s' may not be forward-declared.",n);
  return octo_value_fail(p,"a constant",n,1), octo_make_const(0,0);
}

void octo_macro_body(octo_program*p,char*desc,char*name,octo_macro*m){
  if(p->is_error)return;
  octo_expect(p,"{");
  if(p->is_error){snprintf(p->error,OCTO_ERR_MAX,"Expected '{' for definition of %s '%s'.",desc,name);return;}
  int depth=1;
  while(!octo_is_end(p)){
    octo_tok*t=octo_peek(p);
    if(t->type==OCTO_TOK_STR&&strcmp(t->str_value,"{")==0)depth++;
    if(t->type==OCTO_TOK_STR&&strcmp(t->str_value,"}")==0)depth--;
    if(depth==0)break;
    octo_list_append(&m->body,octo_next(p));
  }
  octo_expect(p,"}");
  if(p->is_error)snprintf(p->error,OCTO_ERR_MAX,"Expected '}' for definition of %s '%s'.",desc,name);
}

/**
*
*  Compile-time Calculation
*
**/

double octo_calc_expr(octo_program*p,char*name); // ooh, co-recursion!
double octo_calc_terminal(octo_program*p,char*name){
  // NUMBER | CONSTANT | LABEL | VREGISTER | '(' expression ')'
  if(octo_peek_is_register(p))return octo_register(p);
  if(octo_match(p,"PI"  ))return 3.141592653589793;
  if(octo_match(p,"E"   ))return 2.718281828459045;
  if(octo_match(p,"HERE"))return p->here;
  octo_tok*t=octo_next(p);
  if(t->type==OCTO_TOK_NUM){
    double r=t->num_value;
    octo_free_tok(t);
    return r;
  }
  char*n=t->str_value;
  octo_free_tok(t);
  if(octo_map_get(&p->protos,n)!=NULL){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Cannot use forward declaration '%s' when calculating constant '%s'.",n,name);
    return 0;
  }
  octo_const*c=octo_map_get(&p->constants,n);
  if(c!=NULL)return c->value;
  if(strcmp(n,"(")!=0){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Found undefined name '%s' when calculating constant '%s'.",n,name);
    return 0;
  }
  double r=octo_calc_expr(p,name);
  octo_expect(p,")");
  return r;
}
double octo_calc_expr(octo_program*p,char*name){
  // UNARY expression
  if(octo_match(p,"strlen")) return octo_interned_len(octo_string(p));
  if(octo_match(p,"-"     )) return -octo_calc_expr(p,name);
  if(octo_match(p,"~"     )) return ~((int)octo_calc_expr(p,name));
  if(octo_match(p,"!"     )) return !((int)octo_calc_expr(p,name));
  if(octo_match(p,"sin"   )) return sin(octo_calc_expr(p,name));
  if(octo_match(p,"cos"   )) return cos(octo_calc_expr(p,name));
  if(octo_match(p,"tan"   )) return tan(octo_calc_expr(p,name));
  if(octo_match(p,"exp"   )) return exp(octo_calc_expr(p,name));
  if(octo_match(p,"log"   )) return log(octo_calc_expr(p,name));
  if(octo_match(p,"abs"   )) return fabs(octo_calc_expr(p,name));
  if(octo_match(p,"sqrt"  )) return sqrt(octo_calc_expr(p,name));
  if(octo_match(p,"sign"  )) return octo_sign(octo_calc_expr(p,name));
  if(octo_match(p,"ceil"  )) return ceil(octo_calc_expr(p,name));
  if(octo_match(p,"floor" )) return floor(octo_calc_expr(p,name));
  if(octo_match(p,"@"     )) return 0xFF&(p->rom[0xFFFF&((int)octo_calc_expr(p,name))]);

  // expression BINARY expression
  double r=octo_calc_terminal(p,name);
  if(octo_match(p,"-"     )) return r-octo_calc_expr(p,name);
  if(octo_match(p,"+"     )) return r+octo_calc_expr(p,name);
  if(octo_match(p,"*"     )) return r*octo_calc_expr(p,name);
  if(octo_match(p,"/"     )) return r/octo_calc_expr(p,name);
  if(octo_match(p,"%"     )) return ((int)r)%((int)octo_calc_expr(p,name));
  if(octo_match(p,"&"     )) return ((int)r)&((int)octo_calc_expr(p,name));
  if(octo_match(p,"|"     )) return ((int)r)|((int)octo_calc_expr(p,name));
  if(octo_match(p,"^"     )) return ((int)r)^((int)octo_calc_expr(p,name));
  if(octo_match(p,"<<"    )) return ((int)r)<<((int)octo_calc_expr(p,name));
  if(octo_match(p,">>"    )) return ((int)r)>>((int)octo_calc_expr(p,name));
  if(octo_match(p,"pow"   )) return pow(r,octo_calc_expr(p,name));
  if(octo_match(p,"min"   )) return octo_min(r,octo_calc_expr(p,name));
  if(octo_match(p,"max"   )) return octo_max(r,octo_calc_expr(p,name));
  if(octo_match(p,"<"     )) return r<octo_calc_expr(p,name);
  if(octo_match(p,">"     )) return r>octo_calc_expr(p,name);
  if(octo_match(p,"<="    )) return r<=octo_calc_expr(p,name);
  if(octo_match(p,">="    )) return r>=octo_calc_expr(p,name);
  if(octo_match(p,"=="    )) return r==octo_calc_expr(p,name);
  if(octo_match(p,"!="    )) return r!=octo_calc_expr(p,name);
  // terminal
  return r;
}
double octo_calculated(octo_program*p,char*name){
  octo_expect(p,"{");
  double r=octo_calc_expr(p,name);
  octo_expect(p,"}");
  return r;
}

/**
*
*  ROM construction
*
**/

void octo_append(octo_program*p, char byte){
  if (p->is_error) return;
  if (p->here>0xFFFF){
    p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"ROM space is full.");
    return;
  }
  if (p->here>0x200 && p->used[p->here]) {
    p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Data overlap. Address 0x%0X has already been defined.",p->here);
    return;
  }
  p->rom[p->here]=byte, p->used[p->here]=1, p->here++;
}
void octo_instruction(octo_program*p, char a, char b){
  octo_append(p, a), octo_append(p, b);
}
void octo_immediate(octo_program*p, char op, int nnn){
  octo_instruction(p, op | ((nnn >> 8) & 0xF), (nnn & 0xFF));
}
void octo_jump(octo_program*p, int addr, int dest){
  if (p->is_error) return;
  p->rom[addr  ]=(0x10 | ((dest >> 8) & 0xF)), p->used[addr  ]=1;
  p->rom[addr+1]=(dest & 0xFF),                p->used[addr+1]=1;
}

/**
*
*  The Compiler proper
*
**/

void octo_pseudo_conditional(octo_program*p,int reg,int sub,int comp){
  if(octo_peek_is_register(p))octo_instruction(p, 0x8F, octo_register(p)<<4);
  else                        octo_instruction(p, 0x6F, octo_value_8bit(p));
  octo_instruction(p, 0x8F, (reg<<4)|sub);
  octo_instruction(p, comp, 0);
}
void octo_conditional(octo_program*p,int negated){
  int reg=octo_register(p);
  octo_tok*t=octo_peek(p);
  char d[256]; octo_tok_value(t,d);
  if(p->is_error)return;
  char* n=octo_string(p);
  #define octo_ca(pos,neg) (strcmp(n,negated?neg:pos)==0)
  if(octo_ca("==","!=")){
    if(octo_peek_is_register(p))octo_instruction(p, 0x90|reg, octo_register(p)<<4);
    else                        octo_instruction(p, 0x40|reg, octo_value_8bit(p));
  }
  else if(octo_ca("!=","==")){
    if(octo_peek_is_register(p))octo_instruction(p, 0x50|reg, octo_register(p)<<4);
    else                        octo_instruction(p, 0x30|reg, octo_value_8bit(p));
  }
  else if(octo_ca("key","-key"))octo_instruction(p, 0xE0|reg, 0xA1);
  else if(octo_ca("-key","key"))octo_instruction(p, 0xE0|reg, 0x9E);
  else if(octo_ca(">","<="))octo_pseudo_conditional(p,reg,0x5,0x4F);
  else if(octo_ca("<",">="))octo_pseudo_conditional(p,reg,0x7,0x4F);
  else if(octo_ca(">=","<"))octo_pseudo_conditional(p,reg,0x7,0x3F);
  else if(octo_ca("<=",">"))octo_pseudo_conditional(p,reg,0x5,0x3F);
  else{
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Expected conditional operator, got %s.",d);
  }
}

void octo_resolve_label(octo_program*p,int offset){
  int target=(p->here)+offset;
  char*n=octo_identifier(p,"label");
  if(p->is_error)return;
  if(octo_map_get(&p->constants,n)!=NULL){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"The name '%s' has already been defined.",n); return;
  }
  if(octo_map_get(&p->aliases,n)!=NULL){
    p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"The name '%s' is already used by an alias.",n); return;
  }
  if((target==0x202||target==0x200)&&(strcmp(n,"main")==0)){
    p->has_main=0, p->here=target=0x200;
    p->rom[0x200]=0, p->used[0x200]=0;
    p->rom[0x201]=0, p->used[0x201]=0;
  }
  octo_map_set(&p->constants,n,octo_make_const(target,0));
  if(octo_map_get(&p->protos,n)==NULL)return;

  octo_proto*pr=octo_map_remove(&p->protos,n);
  for(int z=0;z<pr->addrs.count;z++){
    octo_pref*pa=octo_list_get(&pr->addrs,z);
    if(pa->is_long&&(p->rom[pa->value]&0xF0)==0x60){// :unpack long target
      p->rom[pa->value+1]=target>>8;
      p->rom[pa->value+3]=target;
    }
    else if(pa->is_long){// i := long target
      p->rom[pa->value  ]=target>>8;
      p->rom[pa->value+1]=target;
    }
    else if((target&0xFFF)!=target){
      p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Value 0x%0X for label '%s' does not fit in 12 bits.",target,n);
      break;
    }
    else if((p->rom[pa->value]&0xF0)==0x60){// :unpack target
      p->rom[pa->value+1]=((p->rom[pa->value+1])&0xF0) | ((target>>8)&0xF);
      p->rom[pa->value+3]=target;
    }
    else{
      p->rom[pa->value  ]=((p->rom[pa->value  ])&0xF0) | ((target>>8)&0xF);
      p->rom[pa->value+1]=target;
    }
  }
  octo_free_proto(pr);
}

void octo_compile_statement(octo_program*p){
  if(p->is_error)return;
  int peek_line=octo_peek(p)->line, peek_pos=octo_peek(p)->pos;
  if(octo_peek_is_register(p)){
    int r=octo_register(p);
    if(octo_match(p,":=")){
      if(octo_peek_is_register(p))     octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x0);
      else if (octo_match(p,"random")) octo_instruction(p, 0xC0|r, octo_value_8bit(p));
      else if (octo_match(p,"key"   )) octo_instruction(p, 0xF0|r, 0x0A);
      else if (octo_match(p,"delay" )) octo_instruction(p, 0xF0|r, 0x07);
      else                             octo_instruction(p, 0x60|r, octo_value_8bit(p));
    }
    else if(octo_match(p,"+=")){
      if(octo_peek_is_register(p))     octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x4);
      else                             octo_instruction(p, 0x70|r, octo_value_8bit(p));
    }
    else if(octo_match(p,"-=")){
      if(octo_peek_is_register(p))     octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x5);
      else                             octo_instruction(p, 0x70|r, 1+~octo_value_8bit(p));
    }
    else if(octo_match(p,"|=" ))       octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x1);
    else if(octo_match(p,"&=" ))       octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x2);
    else if(octo_match(p,"^=" ))       octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x3);
    else if(octo_match(p,"=-" ))       octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x7);
    else if(octo_match(p,">>="))       octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0x6);
    else if(octo_match(p,"<<="))       octo_instruction(p, 0x80|r, (octo_register(p)<<4)|0xE);
    else{
      octo_tok*t=octo_next(p); char d[256];
      if(!p->is_error) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Unrecognized operator %s.",octo_tok_value(t,d));
      octo_free_tok(t);
    }
    return;
  }
  if(octo_match(p,":"))octo_resolve_label(p,0);
  else if(octo_match(p,":next"))octo_resolve_label(p,1);
  else if(octo_match(p,":unpack")){
    int a=0;
    if(octo_match(p,"long")){a=octo_value_16bit(p,1,0);}
    else{int v=octo_value_4bit(p);a=(v<<12)|octo_value_12bit(p);}
    octo_reg*rh=octo_map_get(&p->aliases,octo_intern(p,"unpack-hi"));
    octo_reg*rl=octo_map_get(&p->aliases,octo_intern(p,"unpack-lo"));
    octo_instruction(p, 0x60|rh->value, a>>8);
    octo_instruction(p, 0x60|rl->value, a);
  }
  else if(octo_match(p,":breakpoint")) p->breakpoints[p->here]=octo_string(p);
  else if(octo_match(p,":monitor")) {
    char n[256]; octo_mon*m=octo_make_mon();
    octo_tok_value(octo_peek(p),n);
    if(octo_peek_is_register(p)){
      m->type=0; // register monitor
      m->base=octo_register(p);
      if(octo_peek(p)->type==OCTO_TOK_NUM) m->len=octo_value_4bit(p),m->format=NULL;
      else                                 m->len=-1,m->format=octo_string(p);
    }
    else{
      m->type=1; // memory monitor
      m->base=octo_value_16bit(p,0,0);
      if(octo_peek(p)->type==OCTO_TOK_NUM) m->len=octo_value_16bit(p,0,0),m->format=NULL;
      else                                 m->len=-1,m->format=octo_string(p);
    }
    if(n[strlen(n)-1]=='\'')n[strlen(n)-1]='\0';
    char* nn=octo_intern(p,n[0]=='\''?n+1:n);
    octo_map_set(&p->monitors,nn,m);
  }
  else if(octo_match(p,":assert")) {
    char*message=octo_peek_match(p,"{",0)?NULL:octo_string(p);
    if(!octo_calculated(p,"assertion")){
      p->is_error=1;
      if (message!=NULL) snprintf(p->error,OCTO_ERR_MAX,"Assertion failed: %s",message);
      else               snprintf(p->error,OCTO_ERR_MAX,"Assertion failed.");
    }
  }
  else if(octo_match(p,":proto"))octo_free_tok(octo_next(p));//deprecated
  else if(octo_match(p,":alias")){
    char*n=octo_identifier(p,"alias");
    if(octo_map_get(&p->constants,n)!=NULL){p->is_error=1,snprintf(p->error,OCTO_ERR_MAX,"The name '%s' is already used by a constant.",n);return;}
    int v=octo_peek_match(p,"{",0)?octo_calculated(p,"ANONYMOUS"):octo_register(p);
    if(v<0||v>15){p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Register index must be in the range [0,F].");return;}
    octo_reg*prev=octo_map_set(&p->aliases,n,octo_make_reg(v));
    if(prev!=NULL)octo_free_reg(prev);
  }
  else if(octo_match(p,":byte")){
    octo_append(p, octo_peek_match(p,"{",0)?(int)octo_calculated(p,"ANONYMOUS"):octo_value_8bit(p));
  }
  else if(octo_match(p,":pointer")){
    int a=octo_peek_match(p,"{",0)?octo_calculated(p,"ANONYMOUS"):octo_value_16bit(p,1,0);
    octo_instruction(p,a>>8,a);
  }
  else if(octo_match(p,":org")){
    p->here=(octo_peek_match(p,"{",0)?0xFFFF&(int)octo_calculated(p,"ANONYMOUS"):octo_value_16bit(p,0,0));
  }
  else if(octo_match(p,":call")){
    octo_immediate(p,0x20,octo_peek_match(p,"{",0)?0xFFF&(int)octo_calculated(p,"ANONYMOUS"):octo_value_12bit(p));
  }
  else if(octo_match(p,":const")){
    char*n=octo_identifier(p,"constant");
    if(octo_map_get(&p->constants,n)!=NULL){p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"The name '%s' has already been defined.",n);return;}
    octo_map_set(&p->constants,n,octo_value_constant(p));
  }
  else if(octo_match(p,":calc")){
    char*n=octo_identifier(p,"calculated constant");
    octo_const*prev=octo_map_get(&p->constants,n);
    if(prev!=NULL&&!prev->is_mutable){p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Cannot redefine the name '%s' with :calc.",n);return;}
    octo_map_set(&p->constants,n,octo_make_const(octo_calculated(p,n),1));
    if(prev!=NULL)octo_free_const(prev);
  }
  else if(octo_match(p,";")||octo_match(p,"return")) octo_instruction(p, 0x00, 0xEE);
  else if(octo_match(p,"clear"))        octo_instruction(p, 0x00, 0xE0);
  else if(octo_match(p,"bcd"))          octo_instruction(p, 0xF0|octo_register(p), 0x33);
  else if(octo_match(p,"delay"))        octo_expect(p,":="),octo_instruction(p, 0xF0|octo_register(p), 0x15);
  else if(octo_match(p,"buzzer"))       octo_expect(p,":="),octo_instruction(p, 0xF0|octo_register(p), 0x18);
  else if(octo_match(p,"pitch"))        octo_expect(p,":="),octo_instruction(p, 0xF0|octo_register(p), 0x3A);
  else if(octo_match(p,"jump0"))        octo_immediate(p, 0xB0, octo_value_12bit(p));
  else if(octo_match(p,"jump"))         octo_immediate(p, 0x10, octo_value_12bit(p));
  else if(octo_match(p,"native"))       octo_immediate(p, 0x00, octo_value_12bit(p));
  else if(octo_match(p,"audio"))        octo_instruction(p, 0xF0, 0x02);
  else if(octo_match(p,"scroll-down"))  octo_instruction(p, 0x00, 0xC0|octo_value_4bit(p));
  else if(octo_match(p,"scroll-up"))    octo_instruction(p, 0x00, 0xD0|octo_value_4bit(p));
  else if(octo_match(p,"scroll-right")) octo_instruction(p, 0x00, 0xFB);
  else if(octo_match(p,"scroll-left"))  octo_instruction(p, 0x00, 0xFC);
  else if(octo_match(p,"exit"))         octo_instruction(p, 0x00, 0xFD);
  else if(octo_match(p,"lores"))        octo_instruction(p, 0x00, 0xFE);
  else if(octo_match(p,"hires"))        octo_instruction(p, 0x00, 0xFF);
  else if(octo_match(p,"sprite")){
    int x=octo_register(p),y=octo_register(p);octo_instruction(p,0xD0|x,(y<<4)|octo_value_4bit(p));
  }
  else if(octo_match(p,"plane")){
    int n=octo_value_4bit(p);
    if(n>3) p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"The plane bitmask must be [0,3], was %d.",n);
    octo_instruction(p, 0xF0|n, 0x01);
  }
  else if(octo_match(p,"saveflags"))octo_instruction(p, 0xF0|octo_register(p), 0x75);
  else if(octo_match(p,"loadflags"))octo_instruction(p, 0xF0|octo_register(p), 0x85);
  else if(octo_match(p,"save")){
    int r=octo_register(p);
    if(octo_match(p,"-")) octo_instruction(p, 0x50|r, (octo_register(p)<<4)|0x02);
    else                  octo_instruction(p, 0xF0|r, 0x55);
  }
  else if(octo_match(p,"load")){
    int r=octo_register(p);
    if(octo_match(p,"-")) octo_instruction(p, 0x50|r, (octo_register(p)<<4)|0x03);
    else                  octo_instruction(p, 0xF0|r, 0x65);
  }
  else if(octo_match(p,"i")){
    if(octo_match(p,":=")){
      if(octo_match(p,"long")){
        int a=octo_value_16bit(p,1,2);
        octo_instruction(p,0xF0,0x00);
        octo_instruction(p,(a>>8),a);
      }
      else if(octo_match(p,"hex"))    octo_instruction(p, 0xF0|octo_register(p), 0x29);
      else if(octo_match(p,"bighex")) octo_instruction(p, 0xF0|octo_register(p), 0x30);
      else                            octo_immediate(p, 0xA0, octo_value_12bit(p));
    }
    else if(octo_match(p,"+=")) octo_instruction(p, 0xF0|octo_register(p), 0x1E);
    else{
      octo_tok*t=octo_next(p); char d[256];
      p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"%s is not an operator that can target the i register.",octo_tok_value(t,d));
      octo_free_tok(t);
    }
  }
  else if(octo_match(p,"if")){
    int index=(octo_peek_match(p,"key",1)||octo_peek_match(p,"-key",1))?2: 3;
    if(octo_peek_match(p,"then",index)){
      octo_conditional(p,0), octo_expect(p,"then");
    }
    else if (octo_peek_match(p,"begin",index)){
      octo_conditional(p,1), octo_expect(p,"begin");
      octo_stack_push(&p->branches,octo_make_flow(p->here,p->source_line,p->source_pos,"begin"));
      octo_instruction(p, 0x00, 0x00);
    }
    else{
      for(int z=0;z<=index;z++) if(!octo_is_end(p)) octo_free_tok(octo_next(p));
      p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Expected 'then' or 'begin'.");
    }
  }
  else if(octo_match(p,"else")){
    if(octo_stack_is_empty(&p->branches)){
      p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"This 'else' does not have a matching 'begin'.");
      return;
    }
    octo_flow*f=octo_stack_pop(&p->branches);
    octo_jump(p,f->addr,p->here+2); octo_free_flow(f);
    octo_stack_push(&p->branches,octo_make_flow(p->here,peek_line,peek_pos,"else"));
    octo_instruction(p, 0x00, 0x00);
  }
  else if(octo_match(p,"end")){
    if(octo_stack_is_empty(&p->branches)){
      p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"This 'end' does not have a matching 'begin'.");
      return;
    }
    octo_flow*f=octo_stack_pop(&p->branches);
    octo_jump(p,f->addr,p->here); octo_free_flow(f);
  }
  else if(octo_match(p,"loop")){
    octo_stack_push(&p->loops,octo_make_flow(p->here,peek_line,peek_pos,"loop"));
    octo_stack_push(&p->whiles,octo_make_flow(-1,peek_line,peek_pos,"loop"));
  }
  else if(octo_match(p,"while")){
    if(octo_stack_is_empty(&p->loops)){
      p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"This 'while' is not within a loop.");
      return;
    }
    octo_conditional(p,1);
    octo_stack_push(&p->whiles,octo_make_flow(p->here,peek_line,peek_pos,"while"));
    octo_immediate(p, 0x10, 0); // forward jump
  }
  else if(octo_match(p,"again")){
    if(octo_stack_is_empty(&p->loops)){
      p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"This 'again' does not have a matching 'loop'.");
      return;
    }
    octo_flow*f=octo_stack_pop(&p->loops);
    octo_immediate(p,0x10,f->addr);
    octo_free_flow(f);
    while(1){
      octo_flow*f=octo_stack_pop(&p->whiles);
      int a=f->addr;octo_free_flow(f);
      if(a==-1)break;
      octo_jump(p,a,p->here);
    }
  }
  else if(octo_match(p,":macro")){
    char*n=octo_identifier(p,"macro");
    if(octo_map_get(&p->macros,n)){
      p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"The name '%s' has already been defined.",n);
      return;
    }
    octo_macro*m=octo_make_macro();
    octo_map_set(&p->macros,n,m);
    while(!octo_is_end(p) && !octo_peek_match(p,"{",0)) octo_list_append(&m->args,octo_identifier(p,"macro argument"));
    octo_macro_body(p,"macro",n,m);
  }
  else if(octo_match(p,":stringmode")){
    char*n=octo_identifier(p,"stringmode");
    if(octo_map_get(&p->stringmodes,n)==NULL)octo_map_set(&p->stringmodes,n,octo_make_smode());
    octo_smode*s=octo_map_get(&p->stringmodes,n);
    int alpha_base=p->source_pos, alpha_quote=octo_peek_char(p)=='"';
    char*alphabet=octo_string(p);
    octo_macro*m=octo_make_macro(); // every stringmode needs its own copy of this
    octo_macro_body(p,"string mode",n,m);
    for(int z=0;z<octo_interned_len(alphabet);z++){
      int c=0xFF&alphabet[z];
      if(s->modes[c]!=0){
        p->error_pos=alpha_base+z+(alpha_quote?1:0);
        p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"String mode '%s' is already defined for the character '%c'.",n,c);
        break;
      }
      s->values[c]=z;
      s->modes [c]=octo_make_macro();
      octo_tok_list_insert(&s->modes[c]->body,&m->body,0);
    }
    octo_free_macro(m);
  }
  else{
    octo_tok*t=octo_peek(p);
    if(p->is_error)return;
    if(t->type==OCTO_TOK_NUM){
      int n=t->num_value; octo_free_tok(octo_next(p));
      if(n<-128||n>255){
        p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Literal value '%d' does not fit in a byte- must be in range [-128,255].",n);
      }
      octo_append(p,n);
      return;
    }
    char*n=t->type==OCTO_TOK_STR?t->str_value:"";
    if(octo_map_get(&p->macros,n)!=NULL){
      octo_free_tok(octo_next(p));
      octo_macro*m=octo_map_get(&p->macros,n);
      octo_map bindings; // name -> tok
      octo_map_init(&bindings);
      octo_map_set(&bindings,octo_intern(p,"CALLS"),octo_make_tok_num(m->calls++));
      for(int z=0;z<m->args.count;z++){
        if(octo_is_end(p)){
          p->error_line=p->source_line, p->error_pos=p->source_pos;
          p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"Not enough arguments for expansion of macro '%s'.",n);
          break;
        }
        octo_map_set(&bindings,octo_list_get(&m->args,z),octo_next(p));
      }
      int splice_index=0;
      for(int z=0;z<m->body.count;z++){
        octo_tok*t=octo_list_get(&m->body,z);
        octo_tok*r=(t->type==OCTO_TOK_STR)?octo_map_get(&bindings,t->str_value):NULL;
        octo_list_insert(&p->tokens,octo_tok_copy(r==NULL?t:r),splice_index++);
      }
      octo_map_destroy(&bindings,OCTO_DESTRUCTOR(octo_free_tok));
    }
    else if (octo_map_get(&p->stringmodes,n)!=NULL){
      octo_free_tok(octo_next(p));
      octo_smode*s=octo_map_get(&p->stringmodes,n);
      int text_base=p->source_pos, text_quote=octo_peek_char(p)=='"';
      char*text=octo_string(p);
      int splice_index=0;
      for(int tz=0;tz<octo_interned_len(text);tz++){
        int c=0xFF&text[tz];
        if (s->modes[c]==0){
          p->error_pos=text_base+tz+(text_quote?1:0);
          p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"String mode '%s' is not defined for the character '%c'.",n,c);
          break;
        }
        octo_map bindings; // name -> tok
        octo_map_init(&bindings);
        octo_map_set(&bindings,octo_intern(p,"CALLS"),octo_make_tok_num(s->calls++));   // expansion count
        octo_map_set(&bindings,octo_intern(p,"CHAR" ),octo_make_tok_num(c));            // ascii value of current char
        octo_map_set(&bindings,octo_intern(p,"INDEX"),octo_make_tok_num((int)tz));      // index of char in input string
        octo_map_set(&bindings,octo_intern(p,"VALUE"),octo_make_tok_num(s->values[c])); // index of char in class alphabet
        octo_macro*m=s->modes[c];
        for(int z=0;z<m->body.count;z++){
          octo_tok*t=octo_list_get(&m->body,z);
          octo_tok*r=(t->type==OCTO_TOK_STR)?octo_map_get(&bindings,t->str_value):NULL;
          octo_list_insert(&p->tokens,octo_tok_copy(r==NULL?t:r),splice_index++);
        }
        octo_map_destroy(&bindings,OCTO_DESTRUCTOR(octo_free_tok));
      }
    }
    else octo_immediate(p, 0x20, octo_value_12bit(p));
  }
}

octo_program* octo_program_init(char* text){
  octo_program* p=malloc(sizeof(octo_program));
  p->strings_used=0;
  memset(p->strings,'\0',OCTO_INTERN_MAX);
  p->source=text;
  p->source_root=text;
  p->source_line=0;
  p->source_pos=0;
  octo_list_init(&p->tokens);
  p->has_main=1;
  p->here=0x200;
  p->length=OCTO_RAM_MAX;
  memset(p->rom, 0,OCTO_RAM_MAX);
  memset(p->used,0,OCTO_RAM_MAX);
  octo_map_init(&p->constants);
  octo_map_init(&p->aliases);
  octo_map_init(&p->protos);
  octo_map_init(&p->macros);
  octo_map_init(&p->stringmodes);
  octo_stack_init(&p->loops);
  octo_stack_init(&p->branches);
  octo_stack_init(&p->whiles);
  memset(p->breakpoints,0,sizeof(char*)*OCTO_RAM_MAX);
  octo_map_init(&p->monitors);
  p->is_error=0;
  p->error[0]='\0';
  p->error_line=0;
  p->error_pos=0;
  if((unsigned char)p->source[0]==0xEF&&(unsigned char)p->source[1]==0xBB&&(unsigned char)p->source[2]==0xBF)p->source+=3; // UTF-8 BOM
  octo_skip_whitespace(p);

  #define octo_kc(l,n) (octo_map_set(&p->constants,octo_intern(p,("OCTO_KEY_"l)),octo_make_const(n,0)))
  octo_kc("1",0x1), octo_kc("2",0x2), octo_kc("3",0x3), octo_kc("4",0xC),
  octo_kc("Q",0x4), octo_kc("W",0x5), octo_kc("E",0x6), octo_kc("R",0xD),
  octo_kc("A",0x7), octo_kc("S",0x8), octo_kc("D",0x9), octo_kc("F",0xE),
  octo_kc("Z",0xA), octo_kc("X",0x0), octo_kc("C",0xB), octo_kc("V",0xF);

  octo_map_set(&p->aliases,octo_intern(p,"unpack-hi"),octo_make_reg(0));
  octo_map_set(&p->aliases,octo_intern(p,"unpack-lo"),octo_make_reg(1));
  return p;
}

octo_program* octo_compile_str(char* text) {
  octo_program* p=octo_program_init(text);
  octo_instruction(p, 0x00, 0x00); // reserve a jump slot for main
  while(!octo_is_end(p) && !p->is_error){
    p->error_line=p->source_line;
    p->error_pos =p->source_pos;
    octo_compile_statement(p);
  }
  if(p->is_error)return p;
  while(p->length>0x200&&!p->used[p->length-1])p->length--;
  p->error_line=p->source_line, p->error_pos=p->source_pos;

  if(p->has_main){
    octo_const*c=octo_map_get(&p->constants,octo_intern(p,"main"));
    if(c==NULL)return p->is_error=1, snprintf(p->error,OCTO_ERR_MAX,"This program is missing a 'main' label."), p;
    octo_jump(p, 0x200, c->value);
  }
  if(p->protos.keys.count>0){
    octo_proto*pr=octo_list_get(&p->protos.values,0);
    p->error_line=pr->line, p->error_pos=pr->pos;
    p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"Undefined forward reference: %s",(char*)octo_list_get(&p->protos.keys,0));
    return p;
  }
  if(!octo_stack_is_empty(&p->loops)){
    octo_flow*f=octo_stack_pop(&p->loops);
    p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"This 'loop' does not have a matching 'again'.");
    p->error_line=f->line, p->error_pos=f->pos;
    octo_free_flow(f);
    return p;
  }
  if(!octo_stack_is_empty(&p->branches)){
    octo_flow*f=octo_stack_pop(&p->branches);
    p->is_error=1;snprintf(p->error,OCTO_ERR_MAX,"This '%s' does not have a matching 'end'.",f->type);
    p->error_line=f->line, p->error_pos=f->pos;
    octo_free_flow(f);
    return p;
  }
  return p;
}
