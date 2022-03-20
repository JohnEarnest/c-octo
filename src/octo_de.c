/**
*
*  OctoDE
*
*  a self-contained IDE for CHIP-8 programs,
*  requiring only SDL2 and the C standard library.
*
**/

#include "octo_emulator.h"
#include "octo_compiler.h"
#include "octo_cartridge.h"
#include <SDL.h>
#include "octo_util.h"

#define MODE_TEXT_EDITOR     0
#define MODE_HEX_DUMP        1
#define MODE_PIXEL_EDITOR    2
#define MODE_PALETTE_EDITOR  3
#define MODE_CONFIG          4
#define MODE_RUN             5
#define MODE_OPEN            6
#define MODE_SAVE            7
#define MODE_NEW_UNSAVED     8
#define MODE_QUIT_UNSAVED    9
#define MODE_SAVE_OVERWRITE 10

#define LEGEND_SPACE 16

#ifndef VERSION
#define VERSION "1.2w"
#endif

char*default_program=
  "# Welcome to OctoDE v"VERSION"!\n\n"
  ": main\n"
  " plane 3\n"
  " i := ball\n"
  " loop\n"
  "  sprite v0 v1 8\n"
  "  v0 += 3\n"
  "  v1 += 5\n"
  " again\n\n"
  ": ball\n"
  " 0x20 0x48 0x84 0xC0 0xE2 0x7C 0x38 0x00\n"
  " 0x38 0x74 0xFA 0xFE 0xFE 0x7C 0x38 0x00"
;

typedef struct {
  char* root;
  char* cat_root;
  int count;
  int space;
} text_line;
typedef struct {
  int row;
  int col;
} text_pos;
typedef struct {
  text_pos start;
  text_pos end;
} text_span;
typedef struct {
  int       keep_sel;
  text_span old_span;
  text_span new_span;
  char*     old_data;
  char*     new_data;
} text_edit;

typedef struct {
  // shared
  int       running;
  int       mode;
  int       dirty;
  // text editor
  text_span text_cursor;
  text_pos  text_scroll;
  octo_list text_lines;
  char      text_status[256];
  int       text_err;
  int       text_timer;
  int       text_find;
  char      text_find_str[256];
  int       text_find_changed;
  int       text_find_index;
  octo_list text_find_results;
  octo_list text_hist;
  int       text_hist_index;
  // palette editor
  int       pal_selswatch;
  int       pal_selcolor;
  // hex dump
  int       hex_scroll;
  int       hex_sel;
  // sprite editor
  char*     sprite_source;
  int       sprite_big;
  int       sprite_color;
  int       sprite_height;
  int       sprite_selcolor;
  int       sprite_paint;
  char      sprite_data[64];
  octo_list sprite_hist;
  int       sprite_hist_index;
  // open and save
  char      open_path[OCTO_PATH_MAX];
  octo_list open_items;
  int       open_scroll;
  int       open_sel;
  char      open_name[OCTO_NAME_MAX];
  int       open_drag;
  int       open_drag_offset;

} ide_state; ide_state state;

octo_options defaults;
octo_emulator emu;
octo_program*prog=NULL;

/**
*
* Text Editor (Main View)
*
**/

#define TOKEN_UNKNOWN  0
#define TOKEN_NORMAL   1
#define TOKEN_COMMENT  2
#define TOKEN_STRING   3
#define TOKEN_STRINGE  4
#define TOKEN_ESCAPE   5
#define TOKEN_ESCAPEE  6
#define TOKEN_KEYWORD  7

// horrible forward declarations:
void import_text_to_pixel_editor(char*text);
void text_setcursor(int x, int y);
void save_file();
void new_file();

#define T_METRICS int ch=octo_mono_font.height+1,cw=octo_mono_font.maxwidth,cy=(th-4-(1+ch+1))/ch,cx=((tw-MENU_WIDTH-1)/cw)-2;

#define TEXT_LINE_CHUNK 32
text_line* line_create(){
  text_line*r=malloc(sizeof(text_line));
  return r->space=TEXT_LINE_CHUNK,r->root=malloc(r->space),r->cat_root=malloc(r->space),r->count=0,r;
}
void line_destroy(text_line*x){free(x->root),free(x->cat_root),free(x);}
char line_get(text_line*x,int index){return x->root[index];}
char line_get_cat(text_line*x,int index){return x->cat_root[index];}
void line_set_cat(text_line*x,int index,char type){x->cat_root[index]=type;}
int  line_at(text_line*x,int index,char*string){while(*string)if(x->root[index++]!=*string++)return 0;return 1;}
void line_insert(text_line*x,int index,char c){
  if(x->count>=x->space){
    x->root=realloc(x->root,x->space+=TEXT_LINE_CHUNK);
    x->cat_root=realloc(x->cat_root,x->space);
  }
  for(int z=x->count;z>index;z--)x->root[z]=x->root[z-1], x->cat_root[z]=x->cat_root[z-1];
  x->root[index]=c, x->cat_root[index]=TOKEN_UNKNOWN, x->count++;
}
void line_remove(text_line*x,int index){
  for(int z=index;z<x->count;z++)x->root[z]=x->root[z+1], x->cat_root[z]=x->cat_root[z+1];
  x->count--;
}

int cat_color(int c){
  return c==TOKEN_COMMENT?SYNTAX_COMMENT:
         c==TOKEN_KEYWORD?SYNTAX_KEYWORD:
         c==TOKEN_STRING ?SYNTAX_STRING:
         c==TOKEN_STRINGE?SYNTAX_STRING:
         c==TOKEN_ESCAPE ?SYNTAX_ESCAPE:
         c==TOKEN_ESCAPEE?SYNTAX_ESCAPE:
         WHITE;
}
void text_categorize(int row){
  int prev=TOKEN_NORMAL, col=0, prevrow=row;
  while(prevrow>0){
    // we may need to search across multiple blank lines to find the
    // preceding character, particularly for string literals:
    text_line*l=octo_list_get(&state.text_lines,prevrow-1);
    if(l->count<1){prevrow--;continue;}
    prev=line_get_cat(l,l->count-1);
    if(prev==TOKEN_COMMENT)prev=TOKEN_NORMAL;
    break;
  }
  if(prev==TOKEN_STRINGE)prev=TOKEN_NORMAL;
  if(prev==TOKEN_KEYWORD)prev=TOKEN_NORMAL;
  if(prev==TOKEN_UNKNOWN)prev=TOKEN_NORMAL;
  while(row<state.text_lines.count){
    process_row:;
    text_line*line=octo_list_get(&state.text_lines,row);
    if(col>=line->count){
      if(prev==TOKEN_COMMENT)prev=TOKEN_NORMAL;
      if(prev==TOKEN_STRINGE)prev=TOKEN_NORMAL;
      row++,col=0; continue;
    }
    char c=line_get(line,col); int n=prev;
    if(prev==TOKEN_NORMAL){
      if(col==0||isspace(line_get(line,col-1))){
        for(size_t z=0;z<sizeof(octo_reserved_words)/sizeof(char*);z++)if(line_at(line,col,octo_reserved_words[z])){
          int len=strlen(octo_reserved_words[z]);
          if(col+len>=line->count||isspace(line_get(line,col+len))){
            for(int kc=0;kc<len;kc++)line_set_cat(line,col++,TOKEN_KEYWORD);
            goto process_row;
          }
        }
      }
      if(c=='"' )n=TOKEN_STRING;
      if(c=='#' )n=TOKEN_COMMENT;
    }
    else if(prev==TOKEN_STRING){
      if(c=='\\')n=TOKEN_ESCAPE;
      if(c=='"' )n=TOKEN_STRINGE;
    }
    else if(prev==TOKEN_ESCAPE)n=TOKEN_ESCAPEE;
    else if(prev==TOKEN_ESCAPEE)n=TOKEN_STRING;
    else if(prev==TOKEN_STRINGE)n=TOKEN_NORMAL;
    char t=line_get_cat(line,col);
    if(n==t&&n!=TOKEN_KEYWORD)break; // we are now aligned with previous calculations; everything after this point will match!
    line_set_cat(line,col,n);
    prev=n; col++;
  }
}
char* text_export_span(text_span*span){
  octo_str r;
  octo_str_init(&r);
  text_pos*head=&span->end;
  text_pos*tail=&span->start;
  text_pos*min=head->row<tail->row?head: head->row>tail->row?tail: head->col<tail->col?head: tail;
  text_pos*max=min==head?tail:head;
  if(min->row==max->row){
    text_line*line=octo_list_get(&state.text_lines,min->row);
    for(int col=min->col;col<max->col;col++)octo_str_append(&r,line_get(line,col));
  }
  else{
    text_line*first=octo_list_get(&state.text_lines,min->row);
    for(int col=min->col;col<first->count;col++)octo_str_append(&r,line_get(first,col));
    octo_str_append(&r,'\n');
    for(int row=min->row+1;row<max->row;row++){
      text_line*line=octo_list_get(&state.text_lines,row);
      for(int col=0;col<line->count;col++)octo_str_append(&r,line_get(line,col));
      octo_str_append(&r,'\n');
    }
    text_line*last=octo_list_get(&state.text_lines,max->row);
    for(int col=0;col<max->col;col++)octo_str_append(&r,line_get(last,col));
  }
  octo_str_append(&r,'\0');
  return r.root;
}
char* text_export_selection(){
  return text_export_span(&state.text_cursor);
}
char* text_export(){
  octo_str r;
  octo_str_init(&r);
  for(int row=0;row<state.text_lines.count;row++){
    text_line*line=octo_list_get(&state.text_lines,row);
    for(int col=0;col<line->count;col++)octo_str_append(&r,line_get(line,col));
    if(row<state.text_lines.count-1)octo_str_append(&r,'\n');
  }
  octo_str_append(&r,'\0');
  return r.root;
}

void clear_text_hist(int limit){
  state.text_hist_index=limit;
  while(state.text_hist.count>limit){
    text_edit*edit=octo_list_remove(&state.text_hist,state.text_hist.count-1);
    free(edit->old_data),free(edit->new_data),free(edit);
  }
}
void text_apply_edit(text_span*from,text_span*to,char*text,int keep_sel){
  // delete contents of "from" span:
  if(from->start.row==from->end.row){
    text_line*line=octo_list_get(&state.text_lines,from->start.row);
    for(int z=0;z<(from->end.col-from->start.col);z++)line_remove(line,from->start.col);
  }
  else{
    text_line*first=octo_list_get(&state.text_lines,from->start.row);
    text_line*last =octo_list_get(&state.text_lines,from->end  .row);
    text_line*fused=line_create();
    for(int z=0              ;z<from->start.col;z++)line_insert(fused,fused->count,line_get(first,z));
    for(int z=from->end.col  ;z<last->count    ;z++)line_insert(fused,fused->count,line_get(last ,z));
    for(int z=from->start.row;z<=from->end.row ;z++)line_destroy(octo_list_remove(&state.text_lines,from->start.row));
    octo_list_insert(&state.text_lines,fused,from->start.row);
  }
  // insert our text at from.start, calculating an end position for "to":
  int row=from->start.row;
  int col=from->start.col;
  int len=strlen(text);
  for(int z=0;z<len;z++){
    char c=text[z];
    text_line*line=octo_list_get(&state.text_lines,row);
    if(c=='\n'){
      text_line*rest=line_create();
      while(line->count>col)line_insert(rest,rest->count,line_get(line,col)),line_remove(line,col);
      octo_list_insert(&state.text_lines,rest,row+1);
      row++; col=0;
    }
    else if(c=='\r'){}
    else{line_insert(line,col++,c=='\t'?' ': c<' '?'@': c>'~'?'@': c);}
  }
  to->start.row=from->start.row, to->start.col=from->start.col, to->end.row=row, to->end.col=col;
  // we must recompute character classes from the beginning of the line because
  // keywords are effectively determined by "look-behind";
  // for the same reason, when we removed the old text we did not
  // bother to preserve the existing category information for our fused line.
  text_line*start=octo_list_get(&state.text_lines,row);
  for(int z=0;z<start->count;z++)line_set_cat(start,z,TOKEN_UNKNOWN);
  text_categorize(from->start.row);
  text_categorize(row);
  if(keep_sel){
    state.text_cursor.start.row=from->start.row;
    state.text_cursor.start.col=from->start.col;
    state.text_cursor.end.row=row;
    state.text_cursor.end.col=col;
    // this hack again:
    state.text_find=1;
    text_setcursor(col,row);
    state.text_find=0;
  }
  else{
    state.text_cursor.start.row=state.text_cursor.end.row=row;
    state.text_cursor.start.col=state.text_cursor.end.col=col;
    text_setcursor(col,row);
  }
  state.dirty=1;
}
void text_undo(){
  if(state.text_hist_index<=0)return;
  state.text_hist_index--;
  text_edit*edit=octo_list_get(&state.text_hist,state.text_hist_index);
  text_apply_edit(&edit->new_span,&edit->old_span,edit->old_data,edit->keep_sel);
}
void text_redo(){
  if(state.text_hist_index>=state.text_hist.count)return;
  text_edit*edit=octo_list_get(&state.text_hist,state.text_hist_index);
  state.text_hist_index++;
  text_apply_edit(&edit->old_span,&edit->new_span,edit->new_data,edit->keep_sel);
}
void text_new_edit(text_span*span,char* insert,int keep_sel){
  // spans in history MUST be ordered: start<=end.
  // todo: attempt to coalesce sequential edits together?
  clear_text_hist(state.text_hist_index);
  text_edit*edit=malloc(sizeof(text_edit));
  octo_list_append(&state.text_hist,edit);
  state.text_hist_index++;
  edit->keep_sel=keep_sel;
  edit->old_span=*span;
  edit->old_data=text_export_span(span);
  edit->new_data=insert;
  text_apply_edit(&edit->old_span,&edit->new_span,insert,keep_sel);
}
void text_import(char*text){
  clear_text_hist(0);
  state.text_cursor.start.row=0;
  state.text_cursor.start.col=0;
  state.text_cursor.end  .row=0;
  state.text_cursor.end  .col=0;
  state.text_scroll.row=0;
  state.text_scroll.col=0;
  state.text_timer=0;
  while(state.text_lines.count)line_destroy((text_line*)octo_list_remove(&state.text_lines,0));
  text_line* l=line_create();
  octo_list_append(&state.text_lines,l);
  char c;
  if((unsigned char)text[0]==0xEF&&(unsigned char)text[1]==0xBB&&(unsigned char)text[2]==0xBF)text+=3; // UTF-8 BOM
  while((c=*text++)){
    if     (c=='\n'){octo_list_append(&state.text_lines,(l=line_create()));}
    else if(c=='\r'){}
    else if(c=='\t'){line_insert(l,l->count,' ');}
    else            {line_insert(l,l->count,c<' '?'@': c>'~'?'@': c);}
  }
  text_categorize(0);
  state.dirty=0;
}

void text_setstart(int x, int y){
  text_pos*tail=&state.text_cursor.start;
  tail->row=MAX(0,MIN(state.text_lines.count-1,y));
  text_line*line=octo_list_get(&state.text_lines,tail->row);
  tail->col=MAX(0,MIN(line->count,x));
}
void text_setcursor(int x, int y){
  T_METRICS
  // move/normalize cursor and selection
  state.text_timer=0;
  text_pos*head=&state.text_cursor.end;
  text_pos*tail=&state.text_cursor.start;
  int drag=input.is_down||input.is_shifted||state.text_find||input.events[EVENT_SELECT_ALL];
  if(!drag&&(head->col!=tail->col||head->row!=tail->row))*tail=*head, x=head->col,y=head->row; // only collapse selection
  head->row=MAX(0,MIN(state.text_lines.count-1,y));
  text_line*line=octo_list_get(&state.text_lines,head->row);
  head->col=MAX(0,MIN(line->count,x));
  if(!drag)*tail=*head; // move and collapse selection
  // update status bar
  if(head->row==tail->row&&head->col!=tail->col){
    snprintf(state.text_status,sizeof(state.text_status),"%d characters selected",abs(tail->col-head->col));state.text_err=0;
  }
  else if(head->row!=tail->row){
    text_pos min=head->row<tail->row?*head:*tail;
    text_pos max=head->row>tail->row?*head:*tail;
    text_line* first=octo_list_get(&state.text_lines,min.row);
    int chars=(first->count-min.col+1)+(max.col); // suffix+newline, prefix
    for(int z=min.row+1;z<max.row;z++)chars+=((text_line*)octo_list_get(&state.text_lines,z))->count + 1; // (+1 for newline)
    snprintf(state.text_status,sizeof(state.text_status),"%d lines, %d characters selected",abs(tail->row-head->row)+1,chars);state.text_err=0;
  }
  else{
    snprintf(state.text_status,sizeof(state.text_status),"Line %d, Column %d",head->row+1,head->col+1);state.text_err=0;
  }
  // scroll to show cursor
  state.text_scroll.row=MAX(head->row-cy+1,MIN(head->row,state.text_scroll.row));
  state.text_scroll.col=MAX(head->col-cx+1,MIN(head->col,state.text_scroll.col));
}
void text_movecursor(int dx, int dy){
  text_pos*head=&state.text_cursor.end;
  text_setcursor(head->col+dx, head->row+dy);
}
void text_end_find(){
  state.text_find=0;
  text_movecursor(0,0);
}
int text_peek(text_pos*pos){
  if(pos->row>=0&&pos->row<state.text_lines.count){
    text_line*line=octo_list_get(&state.text_lines,pos->row);
    if(pos->col>=0&&pos->col<line->count) return line_get(line,pos->col);
    else if(pos->col==line->count)        return '\n';
  }
  return -1;
}
int text_prevpos(text_pos*pos){
  if(pos->row==0&&pos->col==0)return 0;
  if(!pos->col--)--pos->row,pos->col=((text_line*)octo_list_get(&state.text_lines,pos->row))->count;
  return 1;
}
int text_nextpos(text_pos*pos){
  text_line*line=octo_list_get(&state.text_lines,pos->row);
  if(pos->row==state.text_lines.count-1&&pos->col==line->count)return 0;
  if(pos->col++==line->count)++pos->row,pos->col=0;
  return 1;
}
text_pos text_scanw(text_pos from,int dir){
  text_pos pos=from;
  if(dir<0){
    if(text_prevpos(&pos)){
      from=pos;
      int wantspace=isspace(text_peek(&pos));
      while(text_prevpos(&pos)&&!(wantspace^isspace(text_peek(&pos))))from=pos;
    }
  }
  else{
    int wantspace=isspace(text_peek(&pos));
    while(text_nextpos(&pos)&&!(wantspace^isspace(text_peek(&pos))))from=pos;
    from=pos;
  }
  return from;
}
void find_events(SDL_Event*e){
  size_t len=strlen(state.text_find_str);
  if(e->type==SDL_TEXTINPUT){
    char c=e->text.text[0];
    if((c>=' ')&&(c<='~')&&len<sizeof(state.text_find_str)-1)state.text_find_str[len]=c,state.text_find_str[len+1]='\0',state.text_find_changed=1;
    state.text_timer=0;
  }
  if(e->type==SDL_KEYUP&&e->key.keysym.sym==SDLK_BACKSPACE){
    if(len>0)state.text_find_str[len-1]='\0',state.text_find_changed=1;
    state.text_timer=0;
  }
}
text_span get_ordered_cursor(){
  text_pos*head=&state.text_cursor.end;
  text_pos*tail=&state.text_cursor.start;
  text_pos*min=head->row<tail->row?head: head->row>tail->row?tail: head->col<tail->col?head: tail;
  text_pos*max=min==head?tail:head;
  text_span span; span.start=*min; span.end=*max;
  return span;
}
text_span get_ordered_block(){
  text_span span=get_ordered_cursor();
  span.start.col=0;
  text_line*line=octo_list_get(&state.text_lines,span.end.row);
  span.end.col=MAX(line->count,0);
  return span;
}

void edit_events(SDL_Event*e){
  text_span span=get_ordered_cursor();
  if(e->type==SDL_TEXTINPUT){
    text_new_edit(&span,stralloc(e->text.text),0);
  }
  if(e->type==SDL_KEYDOWN){
    int code=e->key.keysym.sym;
    int mod=e->key.keysym.mod&(KMOD_LCTRL|KMOD_RCTRL|KMOD_LGUI|KMOD_RGUI);
    if(code==SDLK_RETURN){
      text_new_edit(&span,stralloc("\n"),0);
    }
    if(code==SDLK_BACKSPACE){
      if(span.start.row==span.end.row&&span.start.col==span.end.col){
        if(span.start.col>0){span.start.col--;}
        else if(span.start.row==0){return;}
        else{text_line*line=octo_list_get(&state.text_lines,span.start.row-1); span.start.col=line->count, span.start.row--;}
      }
      text_new_edit(&span,stralloc(""),0);
    }
    if(code==SDLK_DELETE){
      if(span.start.row==span.end.row&&span.start.col==span.end.col){
        text_line*line=octo_list_get(&state.text_lines,span.end.row);
        if(span.end.col<line->count){span.end.col++;}
        else if(span.end.row==state.text_lines.count){return;}
        else{span.end.row++,span.end.col=0;}
      }
      text_new_edit(&span,stralloc(""),0);
    }

    text_pos*head=&state.text_cursor.end;
    text_line*headline=octo_list_get(&state.text_lines,head->row);
    text_line*prevline=octo_list_get(&state.text_lines,MAX(0,head->row-1));
    if(code==SDLK_UP){
      if(head->row==0){text_setcursor(0,0);}
      else{text_movecursor(0,-1);}
    }
    if(code==SDLK_DOWN){
      if(head->row==state.text_lines.count-1){text_setcursor(headline->count,head->row);}
      else{text_movecursor(0,1);}
    }
    if(code==SDLK_LEFT){
      if(mod){
        text_pos new_pos=text_scanw(*head,-1);
        text_setcursor(new_pos.col, new_pos.row);
      }
      else if(head->col==0&&head->row>0){text_setcursor(prevline->count,head->row-1);}
      else{text_movecursor(-1,0);}
    }
    if(code==SDLK_RIGHT){
      if(mod){
        text_pos new_pos=text_scanw(*head,1);
        text_setcursor(new_pos.col, new_pos.row);
      }
      else if(head->col==headline->count&&head->row<state.text_lines.count-1){text_setcursor(0,head->row+1);}
      else{text_movecursor(1,0);}
    }
  }
}
void render_text_editor(){
  T_METRICS
  rect mb={tw-MENU_WIDTH,0,MENU_WIDTH,th/8};
  draw_vline(mb.x-1,0,th,WHITE);
  if(widget_menubutton(&mb,NULL,ICON_PLAY,EVENT_RUN)){
    if(prog!=NULL)octo_free_program(prog);
    prog=octo_compile_str(text_export());
    if(prog->is_error){
      text_setcursor(prog->error_pos,prog->error_line);
      text_line*line=octo_list_get(&state.text_lines,state.text_cursor.end.row);
      int c=state.text_cursor.end.col;
      if(line_get_cat(line,c)!=TOKEN_STRING){
        while(c<=line->count&&!isspace(line_get(line,c)))c++;
        state.text_find=1; // hack: force forming a selection
        text_setcursor(c,prog->error_line);
        state.text_find=0;
      }
      snprintf(state.text_status,sizeof(state.text_status),"(%d:%d) %s",prog->error_line+1,prog->error_pos+1,prog->error);state.text_err=1;
      octo_free_program(prog);prog=NULL;
    }
    else{
      int bytes=prog->length-0x200;
      octo_emulator_init(&emu,prog->rom+0x200,bytes,&defaults,NULL);
      snprintf(state.text_status,sizeof(state.text_status),"%d bytes, %d free.",bytes,emu.options.max_rom-bytes);state.text_err=0;
      state.mode=MODE_RUN;
    }
  }
  if(widget_menubutton(&mb,NULL,ICON_OPEN,EVENT_OPEN))state.mode=MODE_OPEN;
  if(widget_menubutton(&mb,NULL,ICON_SAVE,EVENT_NONE))state.mode=MODE_SAVE;
  if(input.events[EVENT_SAVE]){
    if(input.is_shifted||strlen(state.open_name)<1)state.mode=MODE_SAVE;
    else save_file();
  }
  if(widget_menubutton(&mb,NULL,ICON_NEW,EVENT_NEW)){
    if(state.dirty)state.mode=MODE_NEW_UNSAVED;
    else new_file();
  }
  if(widget_menubutton(&mb,NULL,ICON_PIXEL_EDITOR, EVENT_SPRITE)){
    import_text_to_pixel_editor(text_export_selection());
    state.mode=MODE_PIXEL_EDITOR;
  }
  if(widget_menubutton(&mb,NULL,ICON_PALETTE_EDITOR,EVENT_PALETTE))state.mode=MODE_PALETTE_EDITOR;
  if(widget_menubutton(&mb,NULL,ICON_GEAR,          EVENT_NONE   ))state.mode=MODE_CONFIG;
  if(widget_menubutton(&mb,NULL,ICON_CANCEL,EVENT_ESCAPE)){
    if(state.text_find)text_end_find();
    else if (state.dirty)state.mode=MODE_QUIT_UNSAVED;
    else state.running=0;
  }
  // status bar
  rect sb={0,th-(1+ch+1),tw-MENU_WIDTH-1,1+ch+1};
  draw_fill(&sb,state.text_err?ERRCOLOR: state.text_find?POPCOLOR: BLACK);
  for(int z=0;z<MIN(cx,255)&&state.text_status[z];z++)draw_char(state.text_status[z],cw+z*cw,sb.y+1,WHITE);
  if(input.events[EVENT_MOUSEUP]&&mouse_in(&sb)&&in_box(&sb,input.down_x,input.down_y)&&prog!=NULL){
    state.mode=MODE_HEX_DUMP, state.hex_scroll=0, state.hex_sel=-1;
  }
  rect tb={cw,4,cx*cw,cy*ch};
  // cursor/selection
  text_pos*head=&state.text_cursor.end;
  text_pos*tail=&state.text_cursor.start;
  text_line*headline=octo_list_get(&state.text_lines,head->row);
  if(state.text_find==0){
    if(input.events[EVENT_FIND]){
      state.text_find=1;
      state.text_find_changed=1;
      char* sel=text_export_selection();
      snprintf(state.text_find_str,sizeof(state.text_find_str),"%s",sel);
      free(sel);
    }
    if(input.events[EVENT_HOME    ])text_movecursor(-headline->count,0);
    if(input.events[EVENT_END     ])text_movecursor( headline->count,0);
    if(input.events[EVENT_PAGEUP  ])text_movecursor(0,-cy);
    if(input.events[EVENT_PAGEDOWN])text_movecursor(0, cy);
    if(input.events[EVENT_UNDO    ])text_undo();
    if(input.events[EVENT_REDO    ])text_redo();
    if(input.events[EVENT_SELECT_ALL]){
      text_setstart(0,0);
      int lastrow=state.text_lines.count-1;
      text_line*last=octo_list_get(&state.text_lines,lastrow);
      text_setcursor(last->count,lastrow);
    }
    if(input.events[EVENT_CUT]){
      char*text=text_export_selection(); int len=strlen(text);
      if(len>1){
        SDL_SetClipboardText(text);
        text_span span=get_ordered_cursor();
        text_new_edit(&span,stralloc(""),0);
        snprintf(state.text_status,sizeof(state.text_status),"Cut %d character%s.",len,len==1?"":"s");state.text_err=0;
      }
      free(text);
    }
    if(input.events[EVENT_COPY]){
      char*text=text_export_selection(); int len=strlen(text);
      if(len>1){
        SDL_SetClipboardText(text);
        snprintf(state.text_status,sizeof(state.text_status),"Copied %d character%s.",len,len==1?"":"s");state.text_err=0;
      }
      free(text);
    }
    if(input.events[EVENT_PASTE]){
      char*text=SDL_GetClipboardText();
      if(text!=NULL&&*text){
        int len=strlen(text);
        text_span span=get_ordered_cursor();
        text_new_edit(&span,stralloc(text),1);
        snprintf(state.text_status,sizeof(state.text_status),"Pasted %d character%s.",len,len==1?"":"s");state.text_err=0;
      }
      if(text!=NULL)SDL_free(text);
    }
    int block_sel=(head->row!=tail->row)||(head->col!=tail->col), first=MIN(head->row,tail->row), last=MAX(head->row,tail->row), len=last-first+1;
    if(input.events[EVENT_TOGGLE_COMMENT]){
      // determine whether this block is already commented:
      int all_comments=1;
      for(int z=first;z<=last;z++){
        text_line*line=octo_list_get(&state.text_lines,z);
        int i=0;
        while(i<line->count&&isspace(line_get(line,i)))i++;
        if(i>=line->count||line_get(line,i)!='#')all_comments=0;
      }
      // prepare a replacement block
      octo_str r;
      octo_str_init(&r);
      for(int z=first;z<=last;z++){
        if(z!=first)octo_str_append(&r,'\n');
        text_line*line=octo_list_get(&state.text_lines,z);
        if(all_comments){// strip first comment
          int found_first=0;
          for(int col=0;col<line->count;col++)if(!found_first&&line_get(line,col)=='#'){found_first=1;}else{octo_str_append(&r,line_get(line,col));}
        }
        else{// insert leading comment
          octo_str_append(&r,'#');
          for(int col=0;col<line->count;col++)octo_str_append(&r,line_get(line,col));
        }
      }
      octo_str_append(&r,'\0');
      text_span span=get_ordered_block();
      text_new_edit(&span,stralloc(r.root),1);
      octo_str_destroy(&r);
      snprintf(state.text_status,sizeof(state.text_status),"%sommented %d line%s.",all_comments?"Un-c":"C",len,len==1?"":"s");state.text_err=0;
    }
    if((input.events[EVENT_NEXT]||input.events[EVENT_PREV])&&block_sel){
      int indent=input.events[EVENT_NEXT];
      octo_str r;
      octo_str_init(&r);
      for(int z=first;z<=last;z++){
        if(z!=first)octo_str_append(&r,'\n');
        text_line*line=octo_list_get(&state.text_lines,z);
        int col=0;
        if(indent){octo_str_append(&r,' ');}
        else{if(col<line->count&&line_get(line,col)==' ')col++;}
        for(;col<line->count;col++)octo_str_append(&r,line_get(line,col));
      }
      octo_str_append(&r,'\0');
      text_span span=get_ordered_block();
      text_new_edit(&span,stralloc(r.root),1);
      octo_str_destroy(&r);
      snprintf(state.text_status,sizeof(state.text_status),"%sndented %d line%s.",indent?"I":"Uni",len,len==1?"":"s");state.text_err=0;
    }
  }
  else{
    if(state.text_find_changed==1){
      while(state.text_find_results.count)free(octo_list_remove(&state.text_find_results,0));
      int len=strlen(state.text_find_str);
      if(len){
        for(int row=0;row<state.text_lines.count;row++){
          text_line*line=octo_list_get(&state.text_lines,row);
          for(int col=0;col<line->count;col++)if(line_at(line,col,state.text_find_str)){
            text_span*span=malloc(sizeof(text_span));
            span->start.row=row;
            span->start.col=col;
            span->end  .row=row;
            span->end  .col=col+len;
            octo_list_append(&state.text_find_results,span);
          }
        }
      }
      state.text_find_index=0;
      state.text_find_changed=0;
    }
    if(input.events[EVENT_UP  ]||input.events[EVENT_LEFT ])state.text_find_index--;
    if(input.events[EVENT_DOWN]||input.events[EVENT_RIGHT])state.text_find_index++;
    if(state.text_find_index<0)state.text_find_index=state.text_find_results.count-1;
    if(state.text_find_index>=state.text_find_results.count)state.text_find_index=0;
    if(state.text_find_results.count){
      text_span* span=octo_list_get(&state.text_find_results,state.text_find_index);
      text_setstart(span->start.col,span->start.row);
      text_setcursor(span->end.col,span->end.row);
    }
    char trimmed[sizeof(state.text_find_str)];
    string_cap_right(trimmed,state.text_find_str,cx-6);
    snprintf(state.text_status,sizeof(state.text_status),"Find: %s",trimmed); state.text_err=0;
    if(input.events[EVENT_ENTER])text_end_find();
  }
  if(input.events[EVENT_SCROLLUP   ])state.text_scroll.row=MAX(0,state.text_scroll.row-1);
  if(input.events[EVENT_SCROLLDOWN ])state.text_scroll.row=MIN(state.text_lines.count-(cy/2),state.text_scroll.row+1);
  if(input.events[EVENT_SCROLLLEFT ])state.text_scroll.col=MAX(0,state.text_scroll.col-1);
  if(input.events[EVENT_SCROLLRIGHT])state.text_scroll.col=MIN(headline->count,state.text_scroll.col+1);
  if(input.events[EVENT_DOUBLECLICK]&&in_box(&tb,input.down_x,input.down_y)){
    int mx=(input.down_x-tb.x)/cw + state.text_scroll.col;
    int my=(input.down_y-tb.y)/ch + state.text_scroll.row;
    if(my>=0&&my<state.text_lines.count){
      text_line*line=octo_list_get(&state.text_lines,my);
      if(mx>=0&&mx<=line->count){
        int a=mx, b=mx;
        while(a             &&!isspace(line_get(line,a)))a--; // left word boundary
        while(b<=line->count&&!isspace(line_get(line,b)))b++; // right word boundary
        if(isspace(line_get(line,a)))a++;
        state.text_find=1;
        text_setstart(a,my);
        text_setcursor(b,my);
        state.text_find=0;
      }
    }
  }
  else if(input.is_down&&in_box(&tb,input.down_x,input.down_y)){
    state.text_find=0;
    int mx=(input.mouse_x-tb.x)/cw + state.text_scroll.col;
    int my=(input.mouse_y-tb.y)/ch + state.text_scroll.row;
    if(input.events[EVENT_MOUSEDOWN])text_setstart(mx,my);
    text_setcursor(mx,my);
  }
  // text view
  text_pos*min=head->row<tail->row?head: head->row>tail->row?tail: head->col<tail->col?head: tail;
  text_pos*max=min==head?tail:head;
  for(int r=0;r<cy;r++){
    int row=r+state.text_scroll.row;
    if(row<0)continue;
    if(row>=state.text_lines.count)break;
    text_line*l=octo_list_get(&state.text_lines,row);
    for(int c=0;c<cx;c++){
      int col=c+state.text_scroll.col;
      if(col<0)continue;
      if(col>=l->count)break;
      rect cp={tb.x+(c*cw),tb.y+(r*ch),cw,ch};
      int in_selection=(min->row==max->row)?(row==min->row&&col>=min->col&&col<max->col):
                       (row>min->row&&row<max->row)||(row==min->row&&col>=min->col)||(row==max->row&&col<max->col);
      if(in_selection)draw_fill(&cp,SYNTAX_SELECTED);
      draw_char(line_get(l,col),cp.x,cp.y,cat_color(line_get_cat(l,col)));
    }
    if(l->count-state.text_scroll.col>cx){rect m={tb.x+tb.w+2,tb.y+(r*ch),8,9};draw_icon(&m,MORE_RIGHT,POPCOLOR);}
    if(l->count&&state.text_scroll.col>0){rect m={tb.x-5     ,tb.y+(r*ch),8,9};draw_icon(&m,MORE_LEFT, POPCOLOR);}
  }
  // draw cursor after text
  if(state.text_find==0){
    int cursor_x=tb.x+(state.text_cursor.end.col-state.text_scroll.col)*cw;
    int cursor_y=tb.y+(state.text_cursor.end.row-state.text_scroll.row)*ch;
    rect cb={tb.x,tb.y,tb.w+1,tb.h};
    if(in_box(&cb,cursor_x,cursor_y)&&(state.text_timer/30)%2==0)draw_vline(cursor_x,cursor_y,cursor_y+ch-1,WHITE);
  }
  else{
    int cursor_x=sb.x+(strlen(state.text_status)+1)*cw;
    int cursor_y=sb.y+1;
    if(in_box(&sb,cursor_x,cursor_y)&&(state.text_timer/30)%2==0)draw_vline(cursor_x,cursor_y,cursor_y+ch-1,WHITE);
  }
  state.text_timer++;
}

/**
*
* Alert Dialogs
*
**/

void alert_cancel(){
  if     (state.mode==MODE_SAVE_OVERWRITE)state.mode=MODE_SAVE;
  else if(state.mode==MODE_NEW_UNSAVED   )state.mode=MODE_TEXT_EDITOR;
  else if(state.mode==MODE_QUIT_UNSAVED  )state.mode=MODE_TEXT_EDITOR;
}
void alert_confirm(){
  if     (state.mode==MODE_SAVE_OVERWRITE)save_file();
  else if(state.mode==MODE_NEW_UNSAVED   )new_file();
  else if(state.mode==MODE_QUIT_UNSAVED  )state.running=0;
}
void render_alert(){
  // MODE_NEW_UNSAVED | MODE_QUIT_UNSAVED | MODE_SAVE_OVERWRITE
  rect mb={0,0,MENU_WIDTH,th/4};
  draw_vline(mb.x+mb.w,0,th,WHITE);
  if(widget_menubutton(&mb,NULL,ICON_CANCEL,EVENT_ESCAPE))alert_cancel();
  widget_menuspacer(&mb);
  widget_menuspacer(&mb);
  if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER))alert_confirm();
  char*message=state.mode==MODE_SAVE_OVERWRITE?"Saving will replace the previous document.":"The current document has not been saved. Discard it?";
  char*verb   =state.mode==MODE_SAVE_OVERWRITE?"Overwrite":"Discard";
  rect m; size_text(message,0,0,&m);
  draw_text(message,(tw-m.w)/2,(th-m.h)/2,WHITE);
  rect b={(tw-150)/2,th-16-32,150,32};
  draw_rrect(&b,WHITE);
  int active=mouse_in(&b);
  if(active){
    rect i={b.x,b.y,b.w,b.h};
    inset(&i,1);
    draw_fill(&i,POPCOLOR);
    if(input.events[EVENT_MOUSEUP])alert_confirm();
  }
  rect v; size_text(verb,0,0,&v);
  draw_text(verb,b.x+(b.w-v.w)/2,b.y+(b.h-v.h)/2,WHITE);
}

/**
*
* Configuration Panel
*
**/

void draw_preview_font_char(char*font,int xoff,int yoff,int w,int h,int c){
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
    if((font[c*h+y]>>(7-x)&1)==0)continue;
    int px=xoff+2*(9*c+x), py=yoff+2*y;
    PIX(px,py)=PIX(px+1,py)=PIX(px,py+1)=PIX(px+1,py+1)=colors[1];
  }
}
void render_config(){
  rect mb={0,0,MENU_WIDTH,th/4};
  draw_vline(mb.x+mb.w,0,th,WHITE);
  widget_menuspacer(&mb);
  widget_menuspacer(&mb);
  widget_menuspacer(&mb);
  if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER)||input.events[EVENT_ESCAPE])state.mode=MODE_TEXT_EDITOR;
  #define CONFIG_MARGIN 16
  rect o={MENU_WIDTH+CONFIG_MARGIN,CONFIG_MARGIN,tw-MENU_WIDTH-(2*CONFIG_MARGIN),0};
  char* speeds[]={"7","15","20","30","100","200","500","1000"};
  int   speed_vals[]={7,15,20,30,100,200,500,1000};
  int s=7; while(s&&speed_vals[s]>defaults.tickrate)s--;
  if(widget_optionbar(&o,"Cycles Per Frame",speeds,sizeof(speeds)/sizeof(char*),&s)){state.dirty=1;defaults.tickrate=speed_vals[s];}
  o.y+=o.h;
  int p=3, settings[][7]={
    {0,0,1,0,1,1, 3215}, // vip
    {1,1,1,1,0,0, 3583}, // schip
    {0,0,0,0,0,0,65024}, // xo-chip
  };
  for(int z=0;z<3;z++){
    if(((1&defaults.q_shift    )==settings[z][0])&&
       ((1&defaults.q_loadstore)==settings[z][1])&&
       ((1&defaults.q_clip     )==settings[z][2])&&
       ((1&defaults.q_jump0    )==settings[z][3])&&
       ((1&defaults.q_logic    )==settings[z][4])&&
       ((1&defaults.q_vblank   )==settings[z][5])&&
       (defaults.max_rom        ==settings[z][6]))p=z;
  }
  char* profiles[]={"VIP","SCHIP","XO-CHIP","Custom"};
  char* descs[]={
    "Compatible with CHIP-8 on the COSMAC VIP. 3215b of RAM.",
    "Compatible with SCHIP on the HP-48. 3583b of RAM.",
    "Compatible with Octo programs. 65024b of RAM.",
    "This cartridge has a custom configuration.",
  };
  if(widget_optionbar(&o,"Compatibility Profile",profiles,sizeof(profiles)/sizeof(char*),&p)){
    if(p!=3){
      defaults.q_shift    =settings[p][0];
      defaults.q_loadstore=settings[p][1];
      defaults.q_clip     =settings[p][2];
      defaults.q_jump0    =settings[p][3];
      defaults.q_logic    =settings[p][4];
      defaults.q_vblank   =settings[p][5];
      defaults.max_rom    =settings[p][6];
    }
    state.dirty=1;
  }
  o.y+=o.h;
  draw_text(descs[p],o.x,o.y,WHITE);
  o.y+=32;
  char* rotations[]={"0","90","180","270"}; int r=defaults.rotation/90;
  if(widget_optionbar(&o,"Screen Rotation",rotations,sizeof(rotations)/sizeof(char*),&r)){defaults.rotation=90*r,state.dirty=1;}
  o.y+=o.h;
  char* fonts[]={"Octo","VIP","Dream","ETI","SCHIP","Fish"};
  if(widget_optionbar(&o,"Font",fonts,sizeof(fonts)/sizeof(char*),&defaults.font))state.dirty=1;
  rect fp={o.x+(o.w-146*2)/2,o.y+o.h+8,146*2,23*2};
  draw_fill(&fp,colors[0]);
  for(int z=0;z<16;z++){
    draw_preview_font_char(octo_font_sets[defaults.font][0],fp.x+1+2,fp.y+1+ 2,5, 5,z);
    draw_preview_font_char(octo_font_sets[defaults.font][1],fp.x+1+2,fp.y+1+22,8,10,z);
  }
  draw_rect(&fp,WHITE);
}

/**
*
* Palette Editor
*
**/

int color_options[]={
  // see https://lospec.com/palette-list/journey
  0xff7a7d,0xff417d,0xd61a88,0x94007a,0x42004e,0x220029,0x100726,0x25082c,
  0xf0c297,0xcf968c,0x8f5765,0x52294b,0x0f022e,0x35003b,0x64004c,0x9b0e3e,
  0xffcf8e,0xf5a15d,0xd46453,0x9c3247,0x691749,0x3b063a,0x110524,0x050914,
  0xd41e3c,0xed4c40,0xff9757,0xd4662f,0x9c341a,0x691b22,0x450c28,0x2d002e,
  0x3d1132,0x73263d,0xbd4035,0xed7b39,0xffb84a,0xfff540,0xc6d831,0x77b02a,
  0x24142c,0x0e0f2c,0x132243,0x1a466b,0x10908e,0x28c074,0x3dff6e,0xf8ffb8,
  0x144491,0x032769,0x0c0b42,0x0e0421,0x052137,0x153c4a,0x2c645e,0x429058,
  0x488bd4,0x78d7ff,0xb0fff1,0xfaffff,0xc7d4e1,0x928fb8,0x5b537d,0x392946,
};

void render_palette_editor(){
  #define PAL_EDITOR_MARGIN 16
  rect mb={0,0,MENU_WIDTH,th/4};
  draw_vline(mb.x+mb.w,0,th,WHITE);
  widget_menuspacer(&mb);
  widget_menuspacer(&mb);
  widget_menuspacer(&mb);
  if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER)||input.events[EVENT_ESCAPE])state.mode=MODE_TEXT_EDITOR;
  int ss=(th-(2*PAL_EDITOR_MARGIN))/8;
  int sx=(tw-(8*ss))/2;
  int sy=(th-(8*ss))/2;
  for(int z=0;z<64;z++){
    rect swatch={sx+(z%8)*ss,sy+(z/8)*ss,ss-1,ss-1};
    rect ol=swatch;
    inset(&ol,-1);
    draw_rect(&ol,BLACK);
    if(z==state.pal_selswatch){
      rect i={swatch.x,swatch.y,swatch.w,swatch.h};
      draw_fill(&i,WHITE);
      inset(&i,2);
      draw_rect(&i,BLACK);
      inset(&i,1);
      draw_fill(&i,color_options[z]);
    }
    else{draw_fill(&swatch,color_options[z]);}
    if(input.events[EVENT_MOUSEDOWN]&&in_box(&swatch,input.mouse_x,input.mouse_y)){
      state.pal_selswatch=z;
      defaults.colors[state.pal_selcolor]=(0xFF000000|color_options[state.pal_selswatch]);
      state.dirty=1;
    }
  }
  rect pb={tw-MENU_WIDTH,0,MENU_WIDTH,th/6};
  for(int z=0;z<6;z++){
    if(widget_menucolor(&pb,z,&state.pal_selcolor)){
      for(int s=0;s<64;s++)if(color_options[s]==(0xFFFFFF&defaults.colors[z]))state.pal_selswatch=s;
    }
  }
  draw_vline(pb.x-1,0,th,BLACK);
  if(input.events[EVENT_UP   ])state.pal_selswatch-=8;
  if(input.events[EVENT_DOWN ])state.pal_selswatch+=8;
  if(input.events[EVENT_LEFT ])state.pal_selswatch--;
  if(input.events[EVENT_RIGHT])state.pal_selswatch++;
  if(state.pal_selswatch< 0)state.pal_selswatch+=64;
  if(state.pal_selswatch>63)state.pal_selswatch-=64;
  if(input.events[EVENT_UP]||input.events[EVENT_DOWN]||input.events[EVENT_LEFT]||input.events[EVENT_RIGHT]){
    defaults.colors[state.pal_selcolor]=(0xFF000000|color_options[state.pal_selswatch]);
    state.dirty=1;
  }
  if(input.events[EVENT_NEXT])state.pal_selcolor++;
  if(input.events[EVENT_PREV])state.pal_selcolor--;
  if(state.pal_selcolor>=6)state.pal_selcolor=0;
  if(state.pal_selcolor< 0)state.pal_selcolor=5;
  char legend[64];rect lpos;
  char* color_names[]={"Plane 0","Plane 1","Plane 2","Plane 3","Background","Buzzer"};
  snprintf(legend,sizeof(legend),"%s: #%06X",color_names[state.pal_selcolor],0xFFFFFF&defaults.colors[state.pal_selcolor]);
  size_text(legend,0,0,&lpos);
  draw_text(legend,(tw-lpos.w)/2,th-LEGEND_SPACE+(lpos.h)/2,WHITE);
}

/**
*
* Hex Dump
*
**/

void render_hex_dump(){
  #define HEX_DUMP_MARGIN 16
  if(prog==NULL){state.mode=MODE_TEXT_EDITOR;return;}//sanity check
  int ch=octo_mono_font.height, cw=octo_mono_font.maxwidth;
  int rows=(th-(3*HEX_DUMP_MARGIN))/(ch+1);
  rect dump={
    MENU_WIDTH+HEX_DUMP_MARGIN,
    HEX_DUMP_MARGIN,
    tw-MENU_WIDTH-(2*HEX_DUMP_MARGIN),
    rows*(ch+1)
  };
  int cols=(dump.w-(6*cw))/(5*cw);
  int page_bytes=rows*cols*2;
  // menu
  rect mb={0,0,MENU_WIDTH,th/4};
  draw_vline(mb.x+mb.w,0,th,WHITE);
  int hex_length=prog->length-0x200;

  if(widget_menubutton(&mb,"PgUp",NULL,EVENT_PAGEUP  ))state.hex_scroll=MAX(0,MIN(state.hex_scroll-page_bytes,hex_length-page_bytes+1));
  if(widget_menubutton(&mb,"PgDn",NULL,EVENT_PAGEDOWN))state.hex_scroll=MAX(0,MIN(state.hex_scroll+page_bytes,hex_length-page_bytes+1));
  widget_menuspacer(&mb);
  if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER))state.mode=MODE_TEXT_EDITOR;
  // the dump
  for(int y=0;y<rows;y++){
    int base=state.hex_scroll+(y*cols*2);
    char baseaddr[64]; snprintf(baseaddr,sizeof(baseaddr),"%04X",base+0x200);
    int row_y=dump.y+(ch+1)*y;
    draw_text(baseaddr,dump.x,row_y,WHITE);
    for(int x=0;x<cols;x++){
      for(int b=0;b<2;b++){
        int addr=base+(2*x)+b;
        if(addr>=hex_length)goto dump_done;
        char byte[64]; rect bb;
        snprintf(byte,sizeof(byte),"%02X",0xFF&prog->rom[addr+0x200]);
        size_text(byte,dump.x+(6*cw)+(5*cw*x)+(2*cw*b),row_y,&bb);
        if(mouse_in(&bb))state.hex_sel=addr;
        if(state.hex_sel==addr)draw_rect(&bb,POPCOLOR);
        draw_text(byte,bb.x,bb.y,WHITE);
      }
    }
  }
  dump_done:
  draw_vline(dump.x+(5*cw),dump.y,dump.y+dump.h-1,WHITE);
  if(input.is_down&&in_box(&dump,input.down_x,input.down_y)){ // drag to edges to scroll...
    if(input.mouse_y<dump.y       )input.events[EVENT_UP  ]=1;
    if(input.mouse_y>dump.y+dump.h)input.events[EVENT_DOWN]=1;
  }
  if(state.hex_scroll>0){
    draw_text("...",dump.x+(dump.w/2)-(1.5*cw),dump.y-ch-4,WHITE);
    if(input.events[EVENT_UP]||input.events[EVENT_SCROLLUP])state.hex_scroll=MAX(0,MIN(state.hex_scroll-(cols*2),hex_length-page_bytes+1));
  }
  if(state.hex_scroll+page_bytes<hex_length){
    draw_text("...",dump.x+(dump.w/2)-(1.5*cw),dump.y+dump.h,WHITE);
    if(input.events[EVENT_DOWN]||input.events[EVENT_SCROLLDOWN])state.hex_scroll=MAX(0,MIN(state.hex_scroll+(cols*2),hex_length-page_bytes+1));
  }
  char legend[64]; rect lpos;
  if(state.hex_sel==-1)printf(legend,"%d byte%s",hex_length,hex_length==1?"":"s");
  else snprintf(legend,sizeof(legend),"0x%04X: %d of %d byte%s",state.hex_sel+0x200,state.hex_sel,hex_length,hex_length==1?"":"s");
  size_text(legend,0,0,&lpos);
  draw_text(legend,MENU_WIDTH+(tw-MENU_WIDTH-lpos.w)/2,th-LEGEND_SPACE+(lpos.h)/2,WHITE);
}

/**
*
* Pixel Editor
*
**/

void clear_pixel_editor_hist(){
  state.sprite_hist_index=0;
  while(state.sprite_hist.count)free(octo_list_remove(&state.sprite_hist,state.sprite_hist.count-1));
  char*snapshot=malloc(sizeof(state.sprite_data));
  memcpy(snapshot,state.sprite_data,sizeof(state.sprite_data));
  octo_list_append(&state.sprite_hist,snapshot);
}
void edit_pixel_editor(){
  state.sprite_hist_index++;
  while(state.sprite_hist.count>state.sprite_hist_index)free(octo_list_remove(&state.sprite_hist,state.sprite_hist.count-1));
  char*snapshot=malloc(sizeof(state.sprite_data));
  memcpy(snapshot,state.sprite_data,sizeof(state.sprite_data));
  octo_list_append(&state.sprite_hist,snapshot);
}
void undo_pixel_editor(){
  if(state.sprite_hist_index<=0)return;
  state.sprite_hist_index--;
  memcpy(state.sprite_data,octo_list_get(&state.sprite_hist,state.sprite_hist_index),sizeof(state.sprite_data));
}
void redo_pixel_editor(){
  if(state.sprite_hist_index>=state.sprite_hist.count-1)return;
  state.sprite_hist_index++;
  memcpy(state.sprite_data,octo_list_get(&state.sprite_hist,state.sprite_hist_index),sizeof(state.sprite_data));
}
void import_to_pixel_editor(char*source,int size){
  memset(state.sprite_data,0,sizeof(state.sprite_data));
  if(size==0){
    state.sprite_big=0, state.sprite_color=0, state.sprite_height=15;
  }
  else if(size<=15){
    state.sprite_big=0, state.sprite_color=0, state.sprite_height=size;
    for(int z=0;z<size;z++)state.sprite_data[2*z]=source[z];
  }
  else if(size==32){
    state.sprite_big=1, state.sprite_color=0, state.sprite_height=15;
    memcpy(state.sprite_data,source,32);
  }
  else if(size==64){
    state.sprite_big=1, state.sprite_color=1, state.sprite_height=15;
    memcpy(state.sprite_data,source,64);
  }
  else{
    int rows=size/2;
    state.sprite_big=0, state.sprite_color=1, state.sprite_height=rows;
    for(int z=0;z<rows;z++)state.sprite_data[2*z]=source[z],state.sprite_data[32+(2*z)]=source[rows+z];
  }
  clear_pixel_editor_hist();
}
int export_from_pixel_editor(char*dest){
  if(state.sprite_big==0&&state.sprite_color==0){
    for(int z=0;z<state.sprite_height;z++)dest[z]=state.sprite_data[2*z];
    return state.sprite_height;
  }
  else if(state.sprite_big==0&&state.sprite_color==1){
    for(int z=0;z<state.sprite_height;z++)dest[z]=state.sprite_data[2*z],dest[z+state.sprite_height]=state.sprite_data[32+(2*z)];
    return state.sprite_height*2;
  }
  else if(state.sprite_big==1&&state.sprite_color==0){
    memcpy(dest,state.sprite_data,32);return 32;
  }
  else {
    memcpy(dest,state.sprite_data,64);return 64;
  }
}
void import_text_to_pixel_editor(char*text){
  if(state.sprite_source!=NULL)free(state.sprite_source);
  state.sprite_source=text;
  octo_str data;
  octo_str_init(&data);
  octo_program*p=octo_program_init(stralloc(text));
  while(!octo_is_end(p)&&!p->is_error){
    octo_tok*t=octo_next(p);
    if(t->type!=OCTO_TOK_NUM){octo_free_tok(t);break;}
    octo_str_append(&data,0xFF&(int)(t->num_value));
    octo_free_tok(t);
  }
  octo_free_program(p);
  import_to_pixel_editor(data.root,data.pos);
  octo_str_destroy(&data);
}
int char_index(char*string,int line,int pos){
  for(int z=0,l=0,p=0;string[z];z++){
    if(l==line&&p==pos)return z;
    if(string[z]=='\n')p=0,l++;
    else p++;
  }
  return 0;
}
char* export_text_from_pixel_editor(){
  char pixels[64];
  int length=export_from_pixel_editor(pixels);
  octo_str text;       octo_str_init(&text);
  octo_str whitespace; octo_str_init(&whitespace);
  octo_program*p=octo_program_init(state.sprite_source);
  state.sprite_source=NULL;
  int byte_index=0;
  while(!octo_is_end(p)&&!p->is_error){
    octo_tok*t=octo_next(p);
    int tindex=char_index(p->source_root,t->line,t->pos);
    int a=tindex-1;
    while(a>=0&&(p->source_root[a]=='\t'||p->source_root[a]=='\n'||p->source_root[a]==' '))octo_str_append(&whitespace,p->source_root[a--]);
    while(whitespace.pos)octo_str_append(&text, whitespace.root[--whitespace.pos]);
    if(t->type!=OCTO_TOK_NUM){octo_free_tok(t);break;}
    char byte[64];
    if(p->source_root[tindex]=='0'&&p->source_root[tindex+1]=='b'){
      byte[0]='0',byte[1]='b',byte[9]='\0';
      octo_monitor_binary(byte+2,0xFF&pixels[byte_index++]);
    }
    else if(p->source_root[tindex]=='0'&&p->source_root[tindex+1]=='x'){snprintf(byte,sizeof(byte),"0x%02X",0xFF&pixels[byte_index++]);}
    else{snprintf(byte,sizeof(byte),"%d",0xFF&pixels[byte_index++]);}
    octo_str_join(&text,byte);
    octo_free_tok(t);
    if(byte_index>=length)break;
  }
  int b=strlen(p->source_root)-1;
  while(b>=0&&(p->source_root[b]=='\t'||p->source_root[b]=='\n'||p->source_root[b]==' '))octo_str_append(&whitespace,p->source_root[b--]);
  while(whitespace.pos)octo_str_append(&text, whitespace.root[--whitespace.pos]);
  // use default formatting for anything we don't already have whitespace for;
  // either we're making a new sprite from scratch or we're enlarging an existing one:
  while(byte_index<length){
    if(text.pos>0){
      char c=text.root[text.pos-1];
      if(c!=' '&&c!='\t'&&c!='\n')octo_str_append(&text,' ');
    }
    char byte[64];
    snprintf(byte,sizeof(byte),"0x%02X",0xFF&pixels[byte_index++]);
    octo_str_join(&text,byte);
  }
  octo_str_destroy(&whitespace);
  octo_free_program(p);
  octo_str_append(&text,'\0');
  return text.root;
}
int pixel_editor_get(int x,int y){
  int i=x+(y*16), b=(7-(i%8));
  int c1=(state.sprite_data[(i/8)   ]>>b)&1;
  int c2=(state.sprite_data[(i/8)+32]>>b)&1;
  return c1+(state.sprite_color?2*c2:0);
}
void pixel_editor_set(int x,int y,int c){
  int i=x+(y*16), m=1<<(7-(i%8));
  state.sprite_data[(i/8)   ]&=~m;
  state.sprite_data[(i/8)   ]|=(c&1)?m:0;
  state.sprite_data[(i/8)+32]&=~m;
  state.sprite_data[(i/8)+32]|=(c&2)?m:0;
}
void pixel_editor_shift(int dx, int dy){
  char copy[sizeof(state.sprite_data)];
  memcpy(&copy,state.sprite_data,sizeof(state.sprite_data));
  for(int x=0;x<16;x++)for(int y=0;y<16;y++){
    int sx=(x-dx+16)%16;
    int sy=(y-dy+16)%16;
    int i=sx+(sy*16), b=(7-(i%8));
    int c1=(copy[(i/8)   ]>>b)&1;
    int c2=(copy[(i/8)+32]>>b)&1;
    pixel_editor_set(x,y,c1+(state.sprite_color?2*c2:0));
  }
  edit_pixel_editor();
}
void render_pixel_editor(){
  // menu
  {
    rect mb={0,0,MENU_WIDTH,th/5};
    draw_vline(mb.x+mb.w,0,th,WHITE);
    if(widget_menubutton(&mb,NULL,ICON_CANCEL,EVENT_ESCAPE))state.mode=MODE_TEXT_EDITOR;
    if(widget_menubutton(&mb,NULL,ICON_TRASH,EVENT_NONE))memset(state.sprite_data,0,sizeof(state.sprite_data)),edit_pixel_editor();
    if(widget_menubutton(&mb,NULL,state.sprite_big  ?ICON_SPRITE_BIG:ICON_SPRITE_SMALL,EVENT_NONE))state.sprite_big=!state.sprite_big;
    if(widget_menubutton(&mb,NULL,state.sprite_color?ICON_SPRITE_COLOR:ICON_SPRITE_MONO,EVENT_NONE))state.sprite_color=!state.sprite_color;
    if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER)){
      state.mode=MODE_TEXT_EDITOR;
      text_span span=get_ordered_cursor();
      text_new_edit(&span,export_text_from_pixel_editor(),1);
    }
  }
  // palette
  if(state.sprite_color){
    if(input.events[EVENT_NEXT])state.sprite_selcolor++;
    if(input.events[EVENT_PREV])state.sprite_selcolor--;
    if(state.sprite_selcolor>=4)state.sprite_selcolor=0;
    if(state.sprite_selcolor< 0)state.sprite_selcolor=3;
    rect mb={tw-MENU_WIDTH,0,MENU_WIDTH,th/4};
    draw_vline(mb.x-1,0,th,BLACK);
    for(int z=0;z<4;z++)widget_menucolor(&mb,z,&state.sprite_selcolor);
  }
  // canvas layout
  int px=(th-2*LEGEND_SPACE)/16;
  int s_width =state.sprite_big?16:8;
  int s_height=state.sprite_big?16:state.sprite_height;
  int c_height=state.sprite_big?16:15;
  rect canvas={(tw-px*s_width)/2,(th-px*16)/2,px*s_width,px*c_height+1};
  // size slider
  if(!state.sprite_big){
    rect slider={canvas.x+canvas.w,canvas.y-(px/2),32,(c_height+1)*px};
    int slider_active=0;
    if(mouse_in(&slider)&&in_box(&slider,input.down_x,input.down_y))state.sprite_height=s_height=(input.mouse_y-slider.y)/px,slider_active=1;
    int sx=canvas.x+canvas.w+14;
    for(int z=0;z<16;z++)draw_hline(canvas.x+canvas.w,canvas.x+canvas.w+6,canvas.y+(z*px),WHITE);
    rect thumb={sx-8,canvas.y+s_height*px-7,16,16};
    draw_hdash(canvas.x,canvas.x+canvas.w,canvas.y+(s_height*px),slider_active?POPCOLOR:WHITE);
    draw_icon(&thumb,SLIDER_THUMB,slider_active?POPCOLOR:WHITE);
  }
  // canvas+editing
  if(input.events[EVENT_UP   ])pixel_editor_shift( 0,-1);
  if(input.events[EVENT_DOWN ])pixel_editor_shift( 0, 1);
  if(input.events[EVENT_LEFT ])pixel_editor_shift(-1, 0);
  if(input.events[EVENT_RIGHT])pixel_editor_shift( 1, 0);
  if(input.events[EVENT_UNDO])undo_pixel_editor();
  if(input.events[EVENT_REDO])redo_pixel_editor();
  if(in_box(&canvas,input.mouse_x,input.mouse_y)){
    int pixel_x=MIN(15,(input.mouse_x-canvas.x)/px);
    int pixel_y=MIN(15,(input.mouse_y-canvas.y)/px);
    if(input.events[EVENT_MOUSEDOWN]){
      // begin stroke
      state.sprite_paint=input.right_button?0:state.sprite_color?state.sprite_selcolor:!pixel_editor_get(pixel_x,pixel_y);
    }
    else if(input.is_down&&in_box(&canvas,input.down_x,input.down_y)){
      // paint
      pixel_editor_set(pixel_x,pixel_y,state.sprite_paint);
    }
  }
  if(input.events[EVENT_MOUSEUP]&&in_box(&canvas,input.down_x,input.down_y)){
    // end stroke
    edit_pixel_editor();
  }
  rect bg={canvas.x,canvas.y,canvas.w,px*s_height};
  draw_fill(&bg,colors[0]);
  for(int y=0;y<c_height;y++)for(int x=0;x<s_width;x++){
    rect pixel={canvas.x+px*x,canvas.y+px*y,px,px};
    int c=pixel_editor_get(x,y);
    if(c)draw_fill(&pixel,colors[c]);
  }
  draw_rect(&canvas,WHITE);
  // legend
  char legend[64]; rect lpos; int bytes=s_height*(s_width>8?2:1)*(state.sprite_color?2:1);
  snprintf(legend,sizeof(legend),"%d x %d pixels, %d byte%s",s_width,s_height,bytes,bytes==1?"":"s");
  size_text(legend,0,0,&lpos);
  draw_text(legend,(tw-lpos.w)/2,th-LEGEND_SPACE+(lpos.h)/2,WHITE);
}

/**
*
* Open and Save
*
**/

void open_choose(){
  input.events[EVENT_ENTER]=0;
  if(state.open_items.count<1)return;
  octo_path_entry*entry=octo_list_get(&state.open_items,state.open_sel);
  char filename[OCTO_PATH_MAX]="\0";
  octo_path_append(filename,state.open_path);
  octo_path_append(filename,entry->name);

  if(entry->type==OCTO_FILE_TYPE_DIRECTORY){
    octo_path_append(state.open_path,entry->name);
    octo_path_list(&state.open_items,state.open_path);
    state.open_sel=state.open_scroll=state.open_drag=0;
    return;
  }
  if(entry->type==OCTO_FILE_TYPE_CH8){
    state.mode=MODE_TEXT_EDITOR;
    struct stat st;
    if(stat(filename,&st)!=0){snprintf(state.text_status,sizeof(state.text_status),"Unable to open file '%s'",entry->name);state.text_err=1;return;}
    size_t source_size=st.st_size;
    char*source_bytes=malloc(source_size+1);
    FILE*source_file=fopen(filename,"rb");
    fread(source_bytes,sizeof(char),source_size,source_file);
    fclose(source_file);
    octo_str source;
    octo_str_init(&source);
    char header[OCTO_NAME_MAX+256];
    snprintf(header,sizeof(header),"# imported from '%s'\n: main",entry->name);
    octo_str_join(&source,header);
    for(int z=0;z<(int)source_size;z++){
      char byte[32];
      snprintf(byte,sizeof(byte),"%s0x%02X",(z%8==0)?"\n\t":" ",0xFF&source_bytes[z]);
      octo_str_join(&source,byte);
    }
    text_import(source.root);
    octo_str_destroy(&source);
    snprintf(state.open_name,sizeof(state.open_name),"%s",entry->name);
    octo_name_set_extension(state.open_name,"gif");
    snprintf(state.text_status,sizeof(state.text_status),"Imported CHIP-8 binary '%s'",entry->name);state.text_err=0;
  }
  if(entry->type==OCTO_FILE_TYPE_8O){
    state.mode=MODE_TEXT_EDITOR;
    struct stat st;
    if(stat(filename,&st)!=0){snprintf(state.text_status,sizeof(state.text_status),"Unable to open file '%s'",entry->name);state.text_err=1;return;}
    size_t source_size=st.st_size;
    char*source=malloc(source_size+1);
    FILE*source_file=fopen(filename,"r");
    fread(source,sizeof(char),source_size,source_file);
    source[source_size]='\0';
    fclose(source_file);
    text_import(source);
    free(source);
    snprintf(state.open_name,sizeof(state.open_name),"%s",entry->name);
    octo_name_set_extension(state.open_name,"gif");
    snprintf(state.text_status,sizeof(state.text_status),"Opened file '%s'",entry->name);state.text_err=0;
  }
  if(entry->type==OCTO_FILE_TYPE_CARTRIDGE){
    state.mode=MODE_TEXT_EDITOR;
    char*source=octo_cart_load(filename,&defaults);
    if(source==NULL||strlen(source)<1){
      snprintf(state.text_status,sizeof(state.text_status),"Unable to read octocart '%s'",entry->name);state.text_err=1;
      if(source!=NULL)free(source);
      return;
    }
    text_import(source);
    free(source);
    snprintf(state.open_name,sizeof(state.open_name),"%s",entry->name);
    octo_name_set_extension(state.open_name,"gif");
    snprintf(state.text_status,sizeof(state.text_status),"Opened octocart '%s'",entry->name);state.text_err=0;
  }
}
void save_choose(){
  if(state.open_items.count<1)return;
  octo_path_entry*entry=octo_list_get(&state.open_items,state.open_sel);
  if(entry->type==OCTO_FILE_TYPE_DIRECTORY){
    octo_path_append(state.open_path,entry->name);
    octo_path_list(&state.open_items,state.open_path);
    state.open_sel=state.open_scroll=state.open_drag=0;
  }
  else{
    snprintf(state.open_name,sizeof(state.open_name),"%s",entry->name);
  }
}
void new_file(){
  text_import(default_program);
  state.mode=MODE_TEXT_EDITOR;
  state.dirty=0;
  state.open_name[0]='\0';
  snprintf(state.text_status,sizeof(state.text_status),"Created a new project.");state.text_err=0;
}
void save_file(){
  if(strlen(state.open_name)<1)return; // sanity check
  octo_name_set_extension(state.open_name,"gif");
  char filename[OCTO_PATH_MAX]="\0";
  octo_path_append(filename,state.open_path);
  octo_path_append(filename,state.open_name);
  FILE*file=fopen(filename,"wb");
  if(file==NULL){
    snprintf(state.text_status,sizeof(state.text_status),"Unable to write octocart '%s'",state.open_name);state.text_err=1;
    state.mode=MODE_TEXT_EDITOR;
    return;
  }
  char*source=text_export();
  octo_cart_save(file,source,&defaults,NULL,state.open_name);
  free(source);
  fclose(file);
  snprintf(state.text_status,sizeof(state.text_status),"Saved octocart '%s'",state.open_name);state.text_err=0;
  state.mode=MODE_TEXT_EDITOR;
  state.dirty=0;
  octo_path_list(&state.open_items,state.open_path); // refresh list to include what we just saved
}
void save_events(SDL_Event*e){
  size_t len=strlen(state.open_name);
  if(e->type==SDL_TEXTINPUT){
    char c=e->text.text[0];
    if((c>=' ')&&(c<='~')&&len<sizeof(state.open_name)-1)state.open_name[len]=c,state.open_name[len+1]='\0';
    state.text_timer=0;
  }
  if(e->type==SDL_KEYUP&&e->key.keysym.sym==SDLK_BACKSPACE){
    if(len>0)state.open_name[len-1]='\0';
    state.text_timer=0;
  }
}
void render_open_save(int save){
  rect mb={0,0,MENU_WIDTH,th/4};
  draw_vline(mb.x+mb.w,0,th,WHITE);
  if(widget_menubutton(&mb,NULL,ICON_CANCEL,EVENT_ESCAPE))state.mode=MODE_TEXT_EDITOR;
  if(widget_menubutton(&mb,NULL,ICON_UPLEVEL,EVENT_BACK)){
    octo_path_parent(state.open_path);
    octo_path_list(&state.open_items,state.open_path);
    state.open_sel=state.open_scroll=0;
  }
  widget_menuspacer(&mb);
  if(save){
    if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER)){
      char filename[OCTO_PATH_MAX]="\0";
      octo_path_append(filename,state.open_path);
      octo_path_append(filename,state.open_name);
      octo_name_set_extension(filename,"gif");
      struct stat st;
      if(stat(filename,&st)==0)state.mode=MODE_SAVE_OVERWRITE;
      else save_file();
    }
  }
  else{if(widget_menubutton(&mb,NULL,ICON_CONFIRM,EVENT_ENTER))open_choose();}
  #define OPEN_MARGIN 16
  #define SCROLLWIDTH 32
  int ch=octo_mono_font.height+1;
  int cw=octo_mono_font.maxwidth;
  rect f_icon={MENU_WIDTH+OPEN_MARGIN,OPEN_MARGIN+1,8,8};
  draw_icon(&f_icon,ICON_DIR,WHITE);
  char trimmed[OCTO_NAME_MAX];
  string_cap_right(trimmed,state.open_path,(tw-MENU_WIDTH-2*OPEN_MARGIN)/cw);
  draw_text(trimmed,MENU_WIDTH+OPEN_MARGIN+16,OPEN_MARGIN,WHITE);
  char*legend=save?"Save as a .gif Octocart file.":"Open a .ch8, .8o, or .gif Octocart file.";
  draw_text(legend,MENU_WIDTH+OPEN_MARGIN,th-OPEN_MARGIN-ch,WHITE);
  int ih=(!save)?0: 4+2+ch+2+4;
  int lh=th-(2*OPEN_MARGIN)-(2*(ch+4))-2-ih;
  int lines=lh/ch;
  rect lb={MENU_WIDTH+OPEN_MARGIN,OPEN_MARGIN+ch+4,tw-MENU_WIDTH-2*OPEN_MARGIN,lines*ch+2};
  draw_rect(&lb,WHITE);
  if(save){
    rect ib={MENU_WIDTH+OPEN_MARGIN,lb.y+lb.h+4,lb.w,2+ch+2};
    draw_rect(&ib,WHITE);
    string_cap_right(trimmed,state.open_name,(tw-MENU_WIDTH-2*OPEN_MARGIN)/cw);
    draw_text(trimmed,ib.x+3,ib.y+2,WHITE);
    int cursor_x=ib.x+3+(cw*strlen(trimmed));
    if((state.text_timer/30)%2==0)draw_vline(cursor_x,ib.y+2,ib.y+ch+1,WHITE);
    state.text_timer++;
  }
  if(state.open_items.count<1){
    rect mb; char* msg="No files in this directory.";
    size_text(msg,0,0,&mb);
    draw_text(msg,lb.x+(lb.w-mb.w)/2,lb.y+(lb.h-mb.h)/2,WHITE);
  }
  else{
    rect sb={lb.x+lb.w-SCROLLWIDTH,lb.y,SCROLLWIDTH,lb.h};
    if(lines>=state.open_items.count){sb.w=1;}
    else{
      draw_fillslash(&sb,BLACK);
      draw_rect(&sb,WHITE);
      int max=state.open_items.count-lines;
      int thumb_height=MAX(32,sb.h/(1+max));
      int thumb_y=sb.y+(max==0?0:(sb.h-thumb_height)*((state.open_scroll*1.0)/max));
      if(!input.is_down)state.open_drag=0;
      if(state.open_drag){
        int capped=MAX(sb.y,MIN(sb.y+sb.h-thumb_height,input.mouse_y-state.open_drag_offset))-sb.y;
        thumb_y=capped+sb.y;
        if(sb.h-thumb_height!=0)state.open_scroll=MAX(0,MIN(max,((capped*1.0)/(sb.h-thumb_height))*max));
      }
      rect thumb={sb.x,thumb_y,sb.w,thumb_height};
      if(input.events[EVENT_MOUSEDOWN]&&in_box(&thumb,input.down_x,input.down_y)){
        state.open_drag=1,state.open_drag_offset=input.down_y-thumb_y;
      }
      draw_fill(&thumb,state.open_drag?POPCOLOR:SYNTAX_BACKGROUND);
      draw_rect(&thumb,WHITE);
    }
    int chars=(lb.w-2-sb.w-24)/cw;
    for(int z=0;z<lines;z++){
      int index=z+state.open_scroll;
      if(index<0||index>=state.open_items.count)break;
      rect line={lb.x+1,lb.y+1+(z*ch),lb.w-1-sb.w,ch};
      if(index==state.open_sel)draw_fill(&line,BLACK);
      if(mouse_in(&line)&&!state.open_drag){
        draw_fill(&line,POPCOLOR);
        if(input.events[EVENT_MOUSEUP])state.open_sel=index;
        if(input.events[EVENT_DOUBLECLICK]){
          input.events[EVENT_DOUBLECLICK]=0;
          if(save){save_choose();}else{open_choose();}
          break;
        }
      }
      octo_path_entry*entry=octo_list_get(&state.open_items,index);
      string_cap_left(trimmed,entry->name,chars);
      draw_text(trimmed,lb.x+24,lb.y+1+(z*ch),WHITE);
      char* icons[]={ICON_DIR,ICON_CH8,ICON_8O,ICON_CART};
      rect ib={lb.x+8,lb.y+2+(z*ch),8,8};
      draw_icon(&ib,icons[entry->type],WHITE);
    }
    if(input.events[EVENT_UP      ])state.open_sel--;
    if(input.events[EVENT_DOWN    ])state.open_sel++;
    if(input.events[EVENT_PAGEUP  ])state.open_sel-=lines;
    if(input.events[EVENT_PAGEDOWN])state.open_sel+=lines;
    if(state.open_sel<0)state.open_sel=0;
    if(state.open_sel>=state.open_items.count)state.open_sel=state.open_items.count-1;
    if(input.events[EVENT_UP]||input.events[EVENT_DOWN]||input.events[EVENT_PAGEUP]||input.events[EVENT_PAGEDOWN]){
      if(state.open_sel<state.open_scroll)state.open_scroll=state.open_sel;
      if(state.open_sel>state.open_scroll+lines-1)state.open_scroll=state.open_sel-lines+1;
    }
    if(input.events[EVENT_SCROLLUP  ])state.open_scroll--;
    if(input.events[EVENT_SCROLLDOWN])state.open_scroll++;
    if(state.open_scroll>state.open_items.count-lines)state.open_scroll=state.open_items.count-lines;
    if(state.open_scroll<0)state.open_scroll=0;
  }
}

/**
*
* Central Dogma
*
**/

void render(){
  if     (state.mode==MODE_TEXT_EDITOR   )render_text_editor   ();
  else if(state.mode==MODE_CONFIG        )render_config        ();
  else if(state.mode==MODE_PALETTE_EDITOR)render_palette_editor();
  else if(state.mode==MODE_HEX_DUMP      )render_hex_dump      ();
  else if(state.mode==MODE_PIXEL_EDITOR  )render_pixel_editor  ();
  else if(state.mode==MODE_NEW_UNSAVED   )render_alert         ();
  else if(state.mode==MODE_QUIT_UNSAVED  )render_alert         ();
  else if(state.mode==MODE_SAVE_OVERWRITE)render_alert         ();
  else if(state.mode==MODE_OPEN          )render_open_save     (0);
  else if(state.mode==MODE_SAVE          )render_open_save     (1);
}

int main(int argc,char*argv[]){
  (void)argc,(void)argv;
  octo_load_config_default(&ui,&defaults);

  octo_list_init(&state.text_lines);
  snprintf(state.text_status,sizeof(state.text_status),"Octode v"VERSION" Ready.");state.text_err=0;
  state.text_find=0;
  octo_list_init(&state.text_find_results);
  octo_list_init(&state.text_hist);
  state.text_hist_index=0;
  text_import(default_program);
  state.pal_selcolor=0;
  state.sprite_source=NULL;
  state.sprite_selcolor=1;
  octo_list_init(&state.sprite_hist);
  octo_path_home(state.open_path);
  octo_list_init(&state.open_items);
  octo_path_list(&state.open_items,state.open_path);
  state.open_scroll=0;
  state.open_sel=0;
  state.open_drag=0;
  snprintf(state.open_name,sizeof(state.open_name),"%s","");
  state.mode=MODE_TEXT_EDITOR;
  state.dirty=0;

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO);
  SDL_Window*win=SDL_CreateWindow("OctoDE",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,ui.win_width*ui.win_scale,ui.win_height*ui.win_scale,SDL_WINDOW_SHOWN);
  SDL_Renderer*ren=NULL;
  SDL_Texture*screen=NULL;
  octo_ui_init(win,&ren,&screen);
  SDL_Texture*overlay=NULL;
  SDL_SetWindowFullscreen(win,ui.windowed?0:SDL_WINDOW_FULLSCREEN_DESKTOP);
  SDL_AddTimer((1000/60),tick,NULL);
  SDL_JoystickEventState(SDL_ENABLE);
  SDL_Joystick*joy=NULL;
  audio_init(&emu);
  random_init();

  SDL_Event e; state.running=1;
  while(state.running&&SDL_WaitEvent(&e)){
    if(e.type==SDL_QUIT)state.running=0;
    if(e.type==SDL_RENDER_DEVICE_RESET||e.type==SDL_RENDER_TARGETS_RESET){
      SDL_DestroyTexture(overlay),overlay=NULL;
      octo_ui_init(win,&ren,&screen);
    }
    events_queue(&e);
    events_joystick(&emu,&joy,&e);
    if(state.mode==MODE_RUN)events_emulator(&emu,&e);
    if(state.mode==MODE_TEXT_EDITOR&& state.text_find)find_events(&e);
    if(state.mode==MODE_TEXT_EDITOR&&!state.text_find)edit_events(&e);
    if(state.mode==MODE_SAVE)save_events(&e);
    if(e.type==SDL_USEREVENT){
      SDL_FlushEvent(SDL_USEREVENT);

      if(input.events[EVENT_FULLSCREEN]){
        ui.windowed=!ui.windowed;
        if (SDL_SetWindowFullscreen(win, ui.windowed ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP)) {
          printf("toggling fullscreen failed: %s\n", SDL_GetError());
        }
      }
      int bgcolor=emu.options.colors[emu.st>0?OCTO_COLOR_SOUND: OCTO_COLOR_BACKGROUND];
      SDL_SetRenderDrawColor(ren,(bgcolor>>16)&0xFF,(bgcolor>>8)&0xFF,bgcolor&0xFF,0xFF);
      SDL_RenderClear(ren);

      int dw, dh, ow=0, oh=0;
      SDL_GetWindowSize(win,&dw,&dh);
      if(overlay!=NULL)SDL_QueryTexture(overlay,NULL,NULL,&ow,&oh);
      if(overlay==NULL||ow!=(dw/ui.win_scale)||oh!=(dh/ui.win_scale)){
        if(overlay!=NULL)SDL_DestroyTexture(overlay);
        overlay=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,(dw/ui.win_scale),(dh/ui.win_scale));
      }

      if(state.mode==MODE_RUN){
        emu_step(&emu,prog);
        if(input.events[EVENT_ESCAPE])state.mode=MODE_TEXT_EDITOR;
        if(input.events[EVENT_TOGGLE_MONITORS])ui.show_monitors=!ui.show_monitors,octo_ui_invalidate(&emu);
        if(emu.halt){
          if(input.events[EVENT_INTERRUPT])emu.halt=0,octo_ui_invalidate(&emu);
          if(input.events[EVENT_STEP]){
            emu.dt=emu.st=0;octo_emulator_instruction(&emu);snprintf(emu.halt_message,sizeof(emu.halt_message),"Single Stepping");
          }
        }
        else{
          if(input.events[EVENT_INTERRUPT]){
            emu.halt=1;snprintf(emu.halt_message,sizeof(emu.halt_message),"User Interrupt");
          }
        }
        octo_ui_run(&emu,prog,&ui,win,ren,screen,overlay);
        events_clear();
      }
      else{
        int *p, pitch, stride;
        SDL_LockTexture(overlay,NULL,(void**)&p,&pitch);
        stride=pitch/sizeof(int);
        octo_ui_begin(&defaults,p,stride,(dw/ui.win_scale),(dh/ui.win_scale),ui.win_scale);
        for(int z=0;z<stride*th;z++)target[z]=SYNTAX_BACKGROUND;
        render();
        SDL_UnlockTexture(overlay);
        SDL_SetTextureBlendMode(overlay,SDL_BLENDMODE_NONE);
        SDL_RenderCopy(ren,overlay,NULL,NULL);
        SDL_RenderPresent(ren);
        events_clear();
      }
    }
  }
  SDL_Quit();
  return 0;
}
