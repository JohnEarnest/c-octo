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

int main(int argc,char** argv) {
  if(argc<2){
    printf("octo-cli v1.0\n");
    printf("usage: %s <source> [<destination>]\n",argv[0]);
    return 0;
  }

  // read input { .8o, .gif }
  octo_options o;
  octo_default_options(&o);
  char*source=NULL, *source_filename=argv[1];
  if(strcmp(".gif",source_filename+(strlen(source_filename)-4))==0){
    source=octo_cart_load(argv[1],&o);
    if(source==NULL){
      fprintf(stderr,"%s: Unable to load octocart\n",source_filename);
      return 1;
    }
  }
  else {
    struct stat st;
    if(stat(source_filename,&st)!=0){
      fprintf(stderr,"%s: No such file or directory\n",source_filename);
      return 1;
    }
    size_t source_size=st.st_size;
    source=malloc(source_size+1);
    FILE*source_file=fopen(source_filename,"rb");
    fread(source,sizeof(char),source_size,source_file);
    source[source_size]='\0';
    fclose(source_file);
  }

  // compile
  octo_program*p=octo_compile_str(source);
  if(p->is_error){
    fprintf(stderr,"(%d:%d) %s\n",p->error_line+1,p->error_pos+1,p->error);
    return 1;
  }

  // write output { .ch8, .8o, .gif }
  if(argc>2){
    char*dest_filename=argv[2];
    FILE*dest_file=fopen(dest_filename,"wb");
    if(dest_file==NULL){
      fprintf(stderr,"%s: Unable to open file for writing\n",dest_filename);
      return 1;
    }
    if     (strcmp(".gif",dest_filename+(strlen(dest_filename)-4))==0){octo_cart_save(dest_file,source,&o,NULL,dest_filename);}
    else if(strcmp(".8o", dest_filename+(strlen(dest_filename)-3))==0){fwrite(source,sizeof(char),strlen(source),dest_file);}
    else                                                              {fwrite(p->rom+0x200,sizeof(char),p->length-0x200,dest_file);}
    fclose(dest_file);
  }
  else {
    fwrite(p->rom+0x200,sizeof(char),p->length-0x200,stdout);
  }
}
