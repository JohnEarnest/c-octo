/**
*
*  Octo CLI
*
*  A simple command-line frontend for the c-octo
*  compiler and related tools.
*
**/

#include "octo_compiler.h"
#include "octo_emulator.h"
#include "octo_cartridge.h"

char* escape(char*dest,char*src){
  int n=strlen(src), e=0;
  for(int z=0;z<n;z++)if(strchr("\n,\"",src[z])){e=1;break;}
  if(!e){snprintf(dest,4096,"%s",src);return dest;}
  int d=0;dest[d++]='"';
  for(int z=0;z<n&&d+3<4096;z++){if(src[z]=='"'){dest[d++]='"';}dest[d++]=src[z];}
  dest[d++]='"';return dest;
}

void compile(char*source,FILE*dest_file,FILE*sym_file){
  octo_program*p=octo_compile_str(source);
  if(p->is_error){
    fprintf(stderr,"(%d:%d) %s\n",p->error_line+1,p->error_pos+1,p->error);
    exit(1);
  }
  fwrite(p->rom+0x200,sizeof(char),p->length-0x200,dest_file);
  if(!sym_file)return;
  fprintf(sym_file,"type,name,value\n");
  char ek[4096], ev[4096];
  for(int z=0;z<OCTO_RAM_MAX;z++){
    if(!p->breakpoints[z])continue;
    fprintf(sym_file,"breakpoint,%s,%d\n",escape(ek,p->breakpoints[z]),z);
  }
  for(int z=0;z<p->constants.keys.count;z++){
    char*k=octo_list_get(&p->constants.keys,z);if(!strncmp("OCTO_",k,5))continue;
    octo_const*c=octo_list_get(&p->constants.values,z);
    fprintf(sym_file,"constant,%s,%d\n",escape(ek,k),(int)c->value);
  }
  for(int z=0;z<p->aliases.keys.count;z++){
    char*k=octo_list_get(&p->aliases.keys,z);
    octo_reg*r=octo_list_get(&p->aliases.values,z);
    fprintf(sym_file,"alias,%s,%d\n",escape(ek,k),r->value);
  }
  for(int z=0;z<p->monitors.keys.count;z++){
    char*k=escape(ek,octo_list_get(&p->monitors.keys,z));
    octo_mon*m=octo_list_get(&p->monitors.values,z);
    if(m->format){fprintf(sym_file,"monitor,%s,%s\n",k,escape(ev,m->format));}else{fprintf(sym_file,"monitor,%s,%d\n",k,m->len);}
  }
  fclose(sym_file);
}

int main(int argc,char** argv) {
  if(argc<2){
    printf("octo-cli v%s\n",VERSION);
    printf("usage: %s <source> [<destination>] [-s <symfile>]\n",argv[0]);
    return 0;
  }

  char*source_filename=NULL;
  char*dest_filename=NULL;
  char*sym_filename=NULL;
  for(int z=1;z<argc;z++){
    if(!strcmp(argv[z],"-s")){
      if(z+1>=argc){fprintf(stderr,"no symbol file path specified for -s.\n");return 1;}
      sym_filename=argv[++z];
    }
    if(source_filename==NULL){source_filename=argv[z];}
    else{dest_filename=argv[z];}
  }

  // read input { .8o, .gif }
  octo_options o;
  octo_default_options(&o);
  char*source=NULL;
  if(strcmp(".gif",source_filename+(strlen(source_filename)-4))==0){
    source=octo_cart_load(argv[1],&o);
    if(source==NULL){fprintf(stderr,"%s: Unable to load octocart\n",source_filename);return 1;}
  }
  else {
    struct stat st;
    if(stat(source_filename,&st)!=0){fprintf(stderr,"%s: No such file or directory\n",source_filename);return 1;}
    size_t source_size=st.st_size;
    source=malloc(source_size+1);
    FILE*source_file=fopen(source_filename,"rb");
    fread(source,sizeof(char),source_size,source_file);
    source[source_size]='\0';
    fclose(source_file);
  }

  // write output { .ch8, .8o, .gif }
  FILE*sym_file=NULL;
  if(sym_filename!=NULL){
    sym_file=fopen(sym_filename,"w");
    if(sym_file==NULL){fprintf(stderr,"%s: Unable to open symbol file for writing\n",sym_filename);return 1;}
  }

  if(dest_filename==NULL){compile(source,stdout,sym_file);return 0;}
  FILE*dest_file=fopen(dest_filename,"wb");
  if(dest_file==NULL){fprintf(stderr,"%s: Unable to open file for writing\n",dest_filename);return 1;}
  if     (strcmp(".gif",dest_filename+(strlen(dest_filename)-4))==0){octo_cart_save(dest_file,source,&o,NULL,dest_filename);}
  else if(strcmp(".8o", dest_filename+(strlen(dest_filename)-3))==0){fwrite(source,sizeof(char),strlen(source),dest_file);}
  else                                                              {compile(source,dest_file,sym_file);}
  fclose(dest_file);
}
