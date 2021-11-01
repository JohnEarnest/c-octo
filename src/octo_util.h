/**
*
*  octo_util.h
*
*  shared user interface and helper bits
*  used by octo-run and octo-de.
*
**/
#include <time.h>  // time()
#include <stdlib.h> // srand()

#define MAX(a,b)          (a>b?a:b)
#define BLACK             0xFF000000
#define WHITE             0xFFFFFFFF
#define POPCOLOR          0xFFFF00BB
#define ERRCOLOR          0xFF8B0000
#define SYNTAX_BACKGROUND 0xFF272822
#define SYNTAX_COMMENT    0xFF75715E
#define SYNTAX_STRING     0xFFE6DB74
#define SYNTAX_ESCAPE     0xFFAE81FF
#define SYNTAX_KEYWORD    0xFFF92672
#define SYNTAX_SELECTED   0xFF49483E

#define PIX(x,y)     target[(x)+((y)*stride)]

typedef struct {
  int windowed;
  int software_render;
  int win_width;
  int win_height;
  int win_scale;
  int volume;
  int show_monitors;
} octo_ui_config;
octo_ui_config ui;

typedef struct {
  int x, y, w, h;
} rect;

int in_box(rect* r, int x, int y) {
  return x>=r->x && y>=r->y && x<r->x+r->w && y<r->y+r->h;
}
void inset(rect*r,int pixels){
  r->x+=pixels, r->y+=pixels, r->w-=2*pixels, r->h-=2*pixels;
}

typedef struct {
  int  height;
  int  maxwidth;
  char glyphs[95][10];
} font;

font octo_mono_font={
  9, 6,
  {
    {5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {5, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x20, 0x00},
    {5, 0x00, 0x50, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00},
    {5, 0x00, 0x50, 0xF8, 0x50, 0xF8, 0x50, 0x00, 0x00, 0x00},
    {5, 0x20, 0x70, 0xA8, 0xA0, 0x70, 0x28, 0xA8, 0x70, 0x20},
    {5, 0x00, 0x48, 0xA8, 0x50, 0x20, 0x50, 0xA8, 0x90, 0x00},
    {5, 0x00, 0x60, 0x90, 0xA0, 0x40, 0xA8, 0x90, 0x68, 0x00},
    {5, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {5, 0x10, 0x20, 0x20, 0x40, 0x40, 0x40, 0x20, 0x20, 0x10},
    {5, 0x40, 0x20, 0x20, 0x10, 0x10, 0x10, 0x20, 0x20, 0x40},
    {5, 0x00, 0x20, 0xA8, 0x70, 0xA8, 0x20, 0x00, 0x00, 0x00},
    {5, 0x00, 0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00, 0x00},
    {5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x20, 0x40},
    {5, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00},
    {5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00},
    {5, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x80, 0x00},
    {5, 0x00, 0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70, 0x00},
    {5, 0x00, 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xF8, 0x00},
    {5, 0x00, 0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70, 0x00},
    {5, 0x00, 0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10, 0x00},
    {5, 0x00, 0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70, 0x00},
    {5, 0x00, 0x70, 0x80, 0xF0, 0x88, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0xF8, 0x08, 0x08, 0x10, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0x70, 0x88, 0x88, 0x88, 0x78, 0x08, 0x70, 0x00},
    {5, 0x00, 0x00, 0x60, 0x60, 0x00, 0x60, 0x60, 0x00, 0x00},
    {5, 0x00, 0x00, 0x60, 0x60, 0x00, 0x60, 0x60, 0x20, 0x40},
    {5, 0x00, 0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00},
    {5, 0x00, 0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00},
    {5, 0x00, 0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x40, 0x00},
    {5, 0x00, 0x70, 0x88, 0x08, 0x10, 0x20, 0x00, 0x20, 0x00},
    {5, 0x70, 0x88, 0x88, 0xA8, 0xE8, 0xB0, 0x80, 0x88, 0x70},
    {5, 0x00, 0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0, 0x00},
    {5, 0x00, 0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70, 0x00},
    {5, 0x00, 0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0x00},
    {5, 0x00, 0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0x00},
    {5, 0x00, 0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80, 0x00},
    {5, 0x00, 0x70, 0x88, 0x80, 0x98, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0x08, 0x08, 0x08, 0x08, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88, 0x00},
    {5, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8, 0x00},
    {5, 0x00, 0x88, 0xD8, 0xA8, 0x88, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80, 0x00},
    {5, 0x00, 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x08},
    {5, 0x00, 0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70, 0x00},
    {5, 0x00, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0x88, 0x88, 0x88, 0x50, 0x50, 0x20, 0x20, 0x00},
    {5, 0x00, 0x88, 0x88, 0x88, 0x88, 0xA8, 0xD8, 0x88, 0x00},
    {5, 0x00, 0x88, 0x50, 0x20, 0x20, 0x20, 0x50, 0x88, 0x00},
    {5, 0x00, 0x88, 0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8, 0x00},
    {5, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30},
    {5, 0x80, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x00},
    {5, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x30},
    {5, 0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00},
    {5, 0x40, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {5, 0x00, 0x00, 0x00, 0x78, 0x88, 0x88, 0x98, 0x68, 0x00},
    {5, 0x00, 0x80, 0x80, 0xF0, 0x88, 0x88, 0x88, 0xF0, 0x00},
    {5, 0x00, 0x00, 0x00, 0x70, 0x88, 0x80, 0x80, 0x78, 0x00},
    {5, 0x00, 0x08, 0x08, 0x78, 0x88, 0x88, 0x88, 0x78, 0x00},
    {5, 0x00, 0x00, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x78, 0x00},
    {5, 0x00, 0x18, 0x20, 0x70, 0x20, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0x00, 0x00, 0x78, 0x88, 0x88, 0x78, 0x08, 0x70},
    {5, 0x00, 0x80, 0x80, 0xF0, 0x88, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00},
    {5, 0x00, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0xC0},
    {5, 0x00, 0x80, 0x80, 0x90, 0xA0, 0xE0, 0x90, 0x88, 0x00},
    {5, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x30, 0x00},
    {5, 0x00, 0x00, 0x00, 0xF0, 0xA8, 0xA8, 0xA8, 0xA8, 0x00},
    {5, 0x00, 0x00, 0x00, 0xB0, 0xC8, 0x88, 0x88, 0x88, 0x00},
    {5, 0x00, 0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00},
    {5, 0x00, 0x00, 0x00, 0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80},
    {5, 0x00, 0x00, 0x00, 0x78, 0x88, 0x88, 0x78, 0x08, 0x08},
    {5, 0x00, 0x00, 0x00, 0xB0, 0xC8, 0x80, 0x80, 0x80, 0x00},
    {5, 0x00, 0x00, 0x00, 0x78, 0x80, 0x70, 0x08, 0xF0, 0x00},
    {5, 0x00, 0x20, 0x20, 0x78, 0x20, 0x20, 0x20, 0x18, 0x00},
    {5, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x98, 0x68, 0x00},
    {5, 0x00, 0x00, 0x00, 0x88, 0x88, 0x50, 0x50, 0x20, 0x00},
    {5, 0x00, 0x00, 0x00, 0xA8, 0xA8, 0xA8, 0xA8, 0x50, 0x00},
    {5, 0x00, 0x00, 0x00, 0x88, 0x50, 0x20, 0x50, 0x88, 0x00},
    {5, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x78, 0x08, 0x70},
    {5, 0x00, 0x00, 0x00, 0xF8, 0x10, 0x20, 0x40, 0xF8, 0x00},
    {5, 0x18, 0x20, 0x20, 0x20, 0xC0, 0x20, 0x20, 0x20, 0x18},
    {5, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20},
    {5, 0xC0, 0x20, 0x20, 0x20, 0x18, 0x20, 0x20, 0x20, 0xC0},
    {5, 0x00, 0x68, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  }
};

char ICON_PLAY[]={
  0x00,0x00,0x00,0x00,0x18,0x00,0x1E,0x00,0x1F,0x80,0x1F,0xE0,0x1F,0xF8,0x1F,0xFC,
  0x1F,0xF8,0x1F,0xE0,0x1F,0x80,0x1E,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_PAUSE[]={
  0x00,0x00,0x00,0x00,0x00,0x00,0x1E,0x78,0x1E,0x78,0x1E,0x78,0x1E,0x78,0x1E,0x78,
  0x1E,0x78,0x1E,0x78,0x1E,0x78,0x1E,0x78,0x1E,0x78,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_CANCEL[]={
  0x00,0x00,0x00,0x00,0x18,0x18,0x3C,0x3C,0x3E,0x7C,0x1F,0xF8,0x0F,0xF0,0x07,0xE0,
  0x0F,0xF0,0x1F,0xF8,0x3E,0x7C,0x3C,0x3C,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_CONFIRM[]={
  0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x0C,0x00,0x18,0x00,0x38,0x00,0x70,0x18,0xF0,
  0x3D,0xE0,0x3F,0xE0,0x1F,0xC0,0x0F,0xC0,0x07,0x80,0x03,0x00,0x00,0x00,0x00,0x00,
};
char ICON_PIXEL_EDITOR[]={
  0x00,0x00,0x1F,0xF8,0x10,0x08,0x11,0x88,0x11,0x88,0x17,0xE8,0x17,0xE8,0x11,0x88,
  0x11,0x88,0x17,0xE8,0x17,0xE8,0x16,0x68,0x16,0x68,0x10,0x08,0x1F,0xF8,0x00,0x00,
};
char ICON_PALETTE_EDITOR[]={
  0x00,0x00,0x00,0x00,0x1F,0xF8,0x1E,0x48,0x1E,0x48,0x1E,0x48,0x1E,0x48,0x1F,0xF8,
  0x12,0x48,0x12,0x48,0x12,0x48,0x12,0x48,0x1F,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_SAVE[]={
  0x00,0x00,0x00,0x00,0x7F,0xFC,0x40,0x02,0x5F,0xFA,0x50,0x0A,0x50,0x0A,0x50,0x0A,
  0x50,0x0A,0x5F,0xFA,0x40,0x02,0x40,0x02,0x41,0x82,0x7F,0xFE,0x00,0x00,0x00,0x00,
};
char ICON_OPEN[]={
  0x00,0x00,0x00,0x00,0x1E,0x00,0x21,0xF0,0x40,0x08,0x47,0xFE,0x48,0x02,0x48,0x02,
  0x50,0x04,0x50,0x04,0x60,0x08,0x7F,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_NEW[]={
  0x00,0x00,0x1F,0x80,0x10,0xC0,0x10,0xA0,0x10,0x90,0x10,0xF8,0x10,0x08,0x10,0x08,
  0x10,0x08,0x10,0x08,0x10,0x08,0x10,0x08,0x10,0x08,0x1F,0xF8,0x00,0x00,0x00,0x00,
};
char ICON_SPRITE_BIG[]={
  0x00,0x00,0x7F,0xFE,0x40,0x82,0x40,0x02,0x40,0x82,0x40,0x02,0x40,0x82,0x40,0x02,
  0x40,0x82,0x40,0x02,0x40,0x82,0x40,0x02,0x6A,0x82,0x40,0x02,0x7F,0xFE,0x00,0x00,
};
char ICON_SPRITE_SMALL[]={
  0x00,0x00,0x7F,0xAA,0x40,0x80,0x40,0x82,0x40,0x80,0x40,0x82,0x40,0x80,0x40,0x82,
  0x40,0x80,0x40,0x82,0x40,0x80,0x40,0x82,0x7F,0x80,0x00,0x02,0x55,0x54,0x00,0x00,
};
char ICON_SPRITE_MONO[]={
  0x00,0x00,0x00,0x00,0x3F,0xC0,0x3F,0xC0,0x3F,0xC0,0x3F,0xE8,0x3F,0xC4,0x3F,0xC0,
  0x3F,0xC4,0x3F,0xC0,0x04,0x04,0x00,0x00,0x04,0x04,0x02,0xA8,0x00,0x00,0x00,0x00,
};
char ICON_SPRITE_COLOR[]={
  0x00,0x00,0x00,0x00,0x3F,0xC0,0x3F,0xC0,0x3F,0xC0,0x3F,0xFC,0x3F,0xC4,0x3F,0xC4,
  0x3F,0xC4,0x3F,0xC4,0x04,0x04,0x04,0x04,0x04,0x04,0x07,0xFC,0x00,0x00,0x00,0x00,
};
char ICON_QUIET[]={
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x01,0x80,0x03,0x80,0x1F,0x80,0x1F,0x80,
  0x1F,0x80,0x1F,0x80,0x03,0x80,0x01,0x80,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_SOUND[]={
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90,0x01,0x88,0x03,0xA4,0x1F,0x94,0x1F,0xD4,
  0x1F,0xD4,0x1F,0x94,0x03,0xA4,0x01,0x88,0x00,0x90,0x00,0x00,0x00,0x00,0x00,0x00,
};
char ICON_GEAR[]={
  0x00,0x00,0x01,0x80,0x1A,0x58,0x26,0x64,0x20,0x04,0x10,0x08,0x31,0x8C,0x42,0x42,
  0x42,0x42,0x31,0x8C,0x10,0x08,0x20,0x04,0x26,0x64,0x1A,0x58,0x01,0x80,0x00,0x00,
};
char ICON_TRASH[]={
  0x00,0x00,0x07,0xF0,0x1C,0x1C,0x25,0xD2,0x20,0x02,0x3F,0xFE,0x15,0x54,0x15,0x54,
  0x15,0x54,0x15,0x54,0x15,0x54,0x15,0x54,0x15,0x54,0x15,0x54,0x0F,0xF8,0x00,0x00,
};
char ICON_UPLEVEL[]={
  0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x70,0x00,0xF8,0x01,0xFC,0x03,0xFE,0x00,0x70,
  0x00,0x70,0x00,0x70,0x7F,0xF0,0x7F,0xF0,0x7F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,
};
char SLIDER_THUMB[]={ // 16x16
  0x01,0xFF,0x03,0xFF,0x07,0xFF,0x0F,0xFF,0x1F,0xFF,0x3F,0xFF,0x7F,0xFF,0xFF,0xFF,
  0x7F,0xFF,0x3F,0xFF,0x1F,0xFF,0x0F,0xFF,0x07,0xFF,0x03,0xFF,0x01,0xFF,0x00,0x00,
};
char ICON_DIR  []={0x70,0x8F,0x81,0x81,0x81,0x81,0xFF,0x00}; // 8x8
char ICON_CART []={0x00,0xFE,0x82,0xBA,0x82,0xFE,0x7C,0x00};
char ICON_8O   []={0x00,0x77,0x55,0x75,0x55,0x77,0x00,0x00};
char ICON_CH8  []={0x00,0x77,0x45,0x47,0x45,0x77,0x00,0x00};
char MORE_RIGHT[]={0x80,0x80,0x40,0x40,0x20,0x40,0x40,0x80,0x80}; // 3x9
char MORE_LEFT []={0x20,0x20,0x40,0x40,0x80,0x40,0x40,0x20,0x20};

/**
*
*  Drawing
*
**/

int  colors[OCTO_PALETTE_SIZE+3];
int* target;
int  stride;
int  tw=0, th=0, tscale=1;
int  mon_vw=0;

void octo_ui_begin(octo_options* options, int* t, int s, int w, int h, int scale){
  memcpy(colors,options->colors,sizeof(int)*OCTO_PALETTE_SIZE);
  if (tw!=w||th!=h)mon_vw=50;
  target=t, stride=s, tw=w, th=h, tscale=scale;
}

int draw_char(char c,int x,int y,int color){
  if(c<' '||c>'~')c='@';
  int gw=octo_mono_font.glyphs[c-' '][0];
  for(int a=0;a<octo_mono_font.height;a++)for(int b=0;b<gw;b++)
    if(x+b>0&&y+a>0&&x+b<tw&&y+a<th&&(octo_mono_font.glyphs[c-' '][1+a]>>(7-b))&1)PIX(x+b,y+a)=color;
  return gw;
}
void draw_text(char* string,int x, int y,int color){
  int c, sx=x;
  while((c=(int)*string++)){
    if(c=='\n')x=sx,y+=octo_mono_font.height+1;
    else x+=draw_char(c,x,y,color)+1;
  }
}
void draw_stext(char* string, int x, int y, rect* bounds){
  bounds->x=x, bounds->y=y, bounds->h=octo_mono_font.height;
  int c, cx=x;
  while((c=(int)*string)){
    if(c=='\n') bounds->h+=octo_mono_font.height+1, cx=x,y+=octo_mono_font.height+1;
    else{
      if(c<' '||c>'~')c='@';
      int gw=octo_mono_font.glyphs[c-' '][0];
      draw_char(c,cx+1,y+1,BLACK);
      draw_char(c,cx+1,y+0,BLACK);
      draw_char(c,cx+0,y+1,BLACK);
      draw_char(c,cx  ,y  ,WHITE);
      cx+=gw+1, bounds->w=MAX(bounds->w,cx-x);
    }
    string++;
  }
}
void size_text(char* string,int x, int y,rect*r){
  int c; r->x=x,r->y=y,r->w=0;
  while((c=(int)*string++)){
    if(c=='\n')r->w=MAX(r->w,x-r->x),y+=octo_mono_font.height+1,x=r->x;
    else x+=octo_mono_font.maxwidth;
  }
  r->w=MAX(r->w,x-r->x),r->h=y-r->y+octo_mono_font.height;
}
void draw_fill(rect*r,int color){
  int x1=CLAMP(0,r->x,tw), y1=CLAMP(0,r->y,th), x2=CLAMP(0,r->x+r->w,tw), y2=CLAMP(0,r->y+r->h,th);
  for(int a=y1;a<y2;a++)for(int b=x1;b<x2;b++)PIX(b,a)=color;
}
void draw_fillslash(rect*r,int color){
  int x1=CLAMP(0,r->x,tw), y1=CLAMP(0,r->y,th), x2=CLAMP(0,r->x+r->w,tw), y2=CLAMP(0,r->y+r->h,th);
  for(int a=y1;a<y2;a++)for(int b=x1;b<x2;b++)if((a+b)/4%2)PIX(b,a)=color;
}
void draw_icon(rect*r,char*data,int color){
  for(int a=0;a<r->h;a++)for(int b=0;b<r->w;b++){
    int i=(a*r->w)+b, c=(data[i/8]>>(7-(i%8)))&1;
    int x=r->x+b, y=r->y+a;
    if(c&&x>=0&&y>=0&&x<tw&&y<th)PIX(x,y)=color;
  }
}
void draw_hdash(int x1,int x2,int y,int color){
  x1=CLAMP(0,x1,tw-1), x2=CLAMP(0,x2,tw-1), y=CLAMP(0,y,th-1);
  for(int z=x1;z<=x2;z++)if((z/2)%2)PIX(z,y)=color;
}
void draw_hline(int x1,int x2,int y,int color){
  x1=CLAMP(0,x1,tw-1), x2=CLAMP(0,x2,tw-1), y=CLAMP(0,y,th-1);
  for(int z=x1;z<=x2;z++)PIX(z,y)=color;
}
void draw_vline(int x,int y1,int y2,int color){
  x=CLAMP(0,x,tw-1), y1=CLAMP(0,y1,th-1), y2=CLAMP(0,y2,th-1);
  for(int z=y1;z<=y2;z++)PIX(x,z)=color;
}
void draw_shline(int x1, int x2, int y){
  draw_hline(x1+1,x2+1,y+1,BLACK);
  draw_hline(x1,x2,y,WHITE);
}
void draw_svline(int x, int y1, int y2){
  draw_vline(x+1,y1+1,y2+1,BLACK);
  draw_vline(x,y1,y2,WHITE);
}
void draw_rect(rect*r,int color){
  draw_hline(r->x,r->x+r->w-1,r->y       ,color);
  draw_hline(r->x,r->x+r->w-1,r->y+r->h-1,color);
  draw_vline(r->x       ,r->y,r->y+r->h-1,color);
  draw_vline(r->x+r->w-1,r->y,r->y+r->h-1,color);
}
void draw_rrect(rect*r,int color){
  draw_hline(r->x+1,r->x+r->w-2,r->y,       color);
  draw_hline(r->x+1,r->x+r->w-2,r->y+r->h-1,color);
  draw_vline(r->x,       r->y+1,r->y+r->h-2,color);
  draw_vline(r->x+r->w-1,r->y+1,r->y+r->h-2,color);
}

/**
*
* Strings
*
**/

void string_cap_left(char*dest,char*src,int max){
  int len=strlen(src);
  if(len<max)snprintf(dest,max,"%s",src);
  else {snprintf(dest,max-4,"%s",src);snprintf(dest+max-5,4,"...");} // foo...
}
void string_cap_right(char*dest,char*src,int max){
  int len=strlen(src);
  if(len<max)snprintf(dest,max,"%s",src);
  else snprintf(dest,max,"...%s",src+(len-(max-5))); // ...foo
}
char* stralloc(char*source){
  size_t len=strlen(source);
  char* ret=malloc(len+1);
  snprintf(ret,len+1,"%s",source);
  return ret;
}

/**
*
* Input
*
**/

#define EVENT_NONE             0
#define EVENT_ESCAPE           1
#define EVENT_ENTER            2
#define EVENT_FIND             3
#define EVENT_RUN              4
#define EVENT_NEW              5
#define EVENT_OPEN             6
#define EVENT_SAVE             7
#define EVENT_SPRITE           8
#define EVENT_PALETTE          9
#define EVENT_COPY            10
#define EVENT_CUT             11
#define EVENT_PASTE           12
#define EVENT_TOGGLE_COMMENT  13
#define EVENT_MOUSEDOWN       14
#define EVENT_MOUSEUP         15
#define EVENT_UP              16
#define EVENT_DOWN            17
#define EVENT_LEFT            18
#define EVENT_RIGHT           19
#define EVENT_NEXT            20
#define EVENT_PREV            21
#define EVENT_UNDO            22
#define EVENT_REDO            23
#define EVENT_PAGEUP          24
#define EVENT_PAGEDOWN        25
#define EVENT_SCROLLUP        26
#define EVENT_SCROLLDOWN      27
#define EVENT_SCROLLLEFT      28
#define EVENT_SCROLLRIGHT     29
#define EVENT_SPACE           30
#define EVENT_1               31
#define EVENT_2               32
#define EVENT_3               33
#define EVENT_4               34
#define EVENT_5               35
#define EVENT_6               36
#define EVENT_HOME            37
#define EVENT_END             38
#define EVENT_FULLSCREEN      39
#define EVENT_INTERRUPT       40
#define EVENT_STEP            41
#define EVENT_TOGGLE_MONITORS 42
#define EVENT_DOUBLECLICK     43
#define EVENT_SELECT_ALL      44
#define EVENT_BACK            45

#define EVENT_MAX (1+EVENT_BACK)

typedef struct {
  int is_down, down_x, down_y, right_button;
  int mouse_x, mouse_y;
  int is_shifted;
  int events[EVENT_MAX];
  int double_click_timeout;
} input_events;
input_events input={0,-1,-1,0,-1,-1,0,{0},0};

void events_queue(SDL_Event*e){
  if(e->type==SDL_KEYDOWN){
    int code=e->key.keysym.sym;
    if(code==SDLK_LSHIFT)input.is_shifted|=1;
    if(code==SDLK_RSHIFT)input.is_shifted|=2;
  }
  if(e->type==SDL_KEYUP){
    int code =e->key.keysym.sym;
    int cmd  =e->key.keysym.mod&(KMOD_LCTRL|KMOD_RCTRL|KMOD_LGUI|KMOD_RGUI);
    int shift=e->key.keysym.mod&(KMOD_LSHIFT|KMOD_RSHIFT);
    if(code==SDLK_ESCAPE)input.events[EVENT_ESCAPE   ]=1;
    if(code==SDLK_RETURN)input.events[EVENT_ENTER    ]=1;
    if(code==SDLK_f&&cmd)input.events[EVENT_FIND     ]=1;
    if(code==SDLK_r&&cmd)input.events[EVENT_RUN      ]=1;
    if(code==SDLK_n&&cmd)input.events[EVENT_NEW      ]=1;
    if(code==SDLK_o&&cmd)input.events[EVENT_OPEN     ]=1;
    if(code==SDLK_s&&cmd)input.events[EVENT_SAVE     ]=1;
    if(code==SDLK_e&&cmd)input.events[EVENT_SPRITE   ]=1;
    if(code==SDLK_p&&cmd)input.events[EVENT_PALETTE  ]=1;
    if(code==SDLK_c&&cmd)input.events[EVENT_COPY     ]=1;
    if(code==SDLK_x&&cmd)input.events[EVENT_CUT      ]=1;
    if(code==SDLK_v&&cmd)input.events[EVENT_PASTE    ]=1;
    if(code==SDLK_SLASH&&cmd)input.events[EVENT_TOGGLE_COMMENT]=1;
    if(code==SDLK_UP   )input.events[EVENT_UP   ]=1;
    if(code==SDLK_DOWN )input.events[EVENT_DOWN ]=1;
    if(code==SDLK_LEFT )input.events[EVENT_LEFT ]=1;
    if(code==SDLK_RIGHT)input.events[EVENT_RIGHT]=1;
    if(code==SDLK_TAB&&!shift)input.events[EVENT_NEXT]=1;
    if(code==SDLK_TAB&& shift)input.events[EVENT_PREV]=1;
    if(code==SDLK_z&&(!shift)&&cmd)input.events[EVENT_UNDO]=1;
    if(code==SDLK_z&&( shift)&&cmd)input.events[EVENT_REDO]=1;
    if(code==SDLK_PAGEUP)  input.events[EVENT_PAGEUP  ]=1;
    if(code==SDLK_PAGEDOWN)input.events[EVENT_PAGEDOWN]=1;
    if(code==SDLK_SPACE)   input.events[EVENT_SPACE   ]=1;
    if(code==SDLK_1)input.events[EVENT_1]=1;
    if(code==SDLK_2)input.events[EVENT_2]=1;
    if(code==SDLK_3)input.events[EVENT_3]=1;
    if(code==SDLK_4)input.events[EVENT_4]=1;
    if(code==SDLK_5)input.events[EVENT_5]=1;
    if(code==SDLK_6)input.events[EVENT_6]=1;
    if(code==SDLK_HOME)input.events[EVENT_HOME]=1;
    if(code==SDLK_END )input.events[EVENT_END ]=1;
    if(code==SDLK_b&&cmd)input.events[EVENT_FULLSCREEN]=1;
    if(code==SDLK_LSHIFT)input.is_shifted&=~1;
    if(code==SDLK_RSHIFT)input.is_shifted&=~2;
    if(code==SDLK_i)input.events[EVENT_INTERRUPT]=1;
    if(code==SDLK_o)input.events[EVENT_STEP]=1;
    if(code==SDLK_m)input.events[EVENT_TOGGLE_MONITORS]=1;
    if(code==SDLK_a&&cmd)input.events[EVENT_SELECT_ALL]=1;
    if(code==SDLK_BACKQUOTE)input.events[EVENT_BACK]=1;
  }
  if(e->type==SDL_MOUSEMOTION){
    input.mouse_x=(e->motion.x/tscale);
    input.mouse_y=(e->motion.y/tscale);
  }
  if(e->type==SDL_MOUSEBUTTONDOWN){
    input.mouse_x=input.down_x=(e->button.x/tscale);
    input.mouse_y=input.down_y=(e->button.y/tscale);
    input.is_down=1;
    input.events[EVENT_MOUSEDOWN]=1;
    input.right_button=e->button.button!=SDL_BUTTON_LEFT;
  }
  if(e->type==SDL_MOUSEBUTTONUP){
    input.mouse_x=(e->button.x/tscale);
    input.mouse_y=(e->button.y/tscale);
    input.is_down=0;
    input.events[EVENT_MOUSEUP]=1;
    input.right_button=0;
    if(input.double_click_timeout>0)input.events[EVENT_DOUBLECLICK]=1,input.double_click_timeout=0;
    else input.double_click_timeout=20;
  }
  if(e->type==SDL_MOUSEWHEEL){
    if     (e->wheel.y>0)input.events[EVENT_SCROLLUP   ]=1;
    else if(e->wheel.y<0)input.events[EVENT_SCROLLDOWN ]=1;
    else if(e->wheel.x>0)input.events[EVENT_SCROLLRIGHT]=1;
    else if(e->wheel.x<0)input.events[EVENT_SCROLLLEFT ]=1;
  }
}
void events_clear(){
  for(int z=0;z<EVENT_MAX;z++)input.events[z]=0;
  if(input.double_click_timeout>0)input.double_click_timeout--;
}
int mouse_in(rect*r){
  return (input.is_down||input.events[EVENT_MOUSEUP])&&in_box(r,input.mouse_x,input.mouse_y);
}

typedef struct {SDL_Keycode k;int v;} key_mapping;
key_mapping keys[]={
  {SDLK_x,0},
  {SDLK_1,1},{SDLK_KP_9,1}/*xo-circle*/,
  {SDLK_2,2},{SDLK_KP_7,2}/*xo-square*/,
  {SDLK_3,3},
  {SDLK_q,4},{SDLK_F12,4},{SDLK_KP_3,4}/*xo-cross*/,
  {SDLK_w,5},{SDLK_UP,5},{SDLK_KP_6,5},
  {SDLK_e,6},{SDLK_SPACE,6},{SDLK_BACKSPACE,6},{SDLK_KP_1,6}/*xo-check*/,
  {SDLK_a,7},{SDLK_LEFT,7},{SDLK_KP_4,7},
  {SDLK_s,8},{SDLK_DOWN,8},{SDLK_KP_2,8},
  {SDLK_d,9},{SDLK_RIGHT,9},{SDLK_KP_6,9},
  {SDLK_z,10},
  {SDLK_c,11},
  {SDLK_4,12},
  {SDLK_r,13},
  {SDLK_f,14},
  {SDLK_v,15},
};

void events_emulator(octo_emulator*emu,SDL_Event*e){
  if(e->type==SDL_KEYDOWN){
    int code=e->key.keysym.sym;
    for(int z=0;z<(int)(sizeof(keys)/sizeof(key_mapping));z++)if(keys[z].k==code)emu->keys[keys[z].v]=1;
  }
  if(e->type==SDL_KEYUP){
    int code=e->key.keysym.sym;
    for(int z=0;z<(int)(sizeof(keys)/sizeof(key_mapping));z++)if(keys[z].k==code){
      emu->keys[keys[z].v]=0;
      if(emu->wait){emu->v[(int)emu->wait_reg]=keys[z].v;emu->wait=0;}
    }
  }
}

void events_joystick(octo_emulator*emu, SDL_Joystick**joy, SDL_Event*e){
  if(e->type==SDL_JOYDEVICEADDED&&e->jdevice.which==0&&(*joy)==NULL){(*joy)=SDL_JoystickOpen(0);printf("found gamepad\n");}
  if(e->type==SDL_JOYDEVICEREMOVED&&e->jdevice.which==0&&(*joy)!=NULL){SDL_JoystickClose(*joy);(*joy)=NULL;printf("lost gamepad\n");}
  if(e->type==SDL_JOYAXISMOTION&&(*joy)!=NULL){
    #define DEAD_ZONE ((int)(32768*0.2))
    int xaxis=SDL_JoystickGetAxis(*joy,0);
    int yaxis=SDL_JoystickGetAxis(*joy,1);
    emu->keys[7]=xaxis<-DEAD_ZONE?1:0; // LEFT
    emu->keys[9]=xaxis> DEAD_ZONE?1:0; // RIGHT
    emu->keys[5]=yaxis<-DEAD_ZONE?1:0; // UP
    emu->keys[8]=yaxis> DEAD_ZONE?1:0; // DOWN
    if(emu->wait){
      if(abs(yaxis)<DEAD_ZONE&&abs(xaxis)<DEAD_ZONE&&emu->pending!=-1)emu->v[(int)emu->wait_reg]=emu->pending,emu->pending=-1,emu->wait=0;
      else if(abs(yaxis)>DEAD_ZONE&&abs(yaxis)>abs(xaxis)&&emu->pending==-1)emu->pending=yaxis<0?5:8;
      else if(abs(xaxis)>DEAD_ZONE&&abs(xaxis)>abs(yaxis)&&emu->pending==-1)emu->pending=xaxis<0?7:9;
    }
  }
  if(e->type==SDL_JOYBUTTONDOWN){
    int b=e->jbutton.button%2?6:4;emu->keys[b]=1;
  }
  if(e->type==SDL_JOYBUTTONUP){
    int b=e->jbutton.button%2?6:4;emu->keys[b]=0;
    if(emu->wait)emu->v[(int)emu->wait_reg]=b, emu->wait=0;
  }
}

/**
*
*  Widgets
*
**/

#define MENU_WIDTH 32

int widget_menubutton(rect*pos,char*label,char*icon,int event){
  if(pos->y+2*pos->h>th)pos->h=th-pos->y; // last button in a column
  int active=mouse_in(pos)&&in_box(pos,input.down_x,input.down_y);
  draw_fill(pos,active?POPCOLOR:BLACK);
  if(pos->y!=0) draw_hline(pos->x,pos->x+pos->w-1,pos->y,WHITE);
  if(label){
    rect tb;
    size_text(label,0,0,&tb);
    draw_text(label,pos->x+(MENU_WIDTH-tb.w)/2,pos->y+(pos->h-tb.h)/2,WHITE);
  }
  if(icon){
    rect ib={pos->x+(MENU_WIDTH-16)/2,pos->y+(pos->h-16)/2,16,16};
    draw_icon(&ib,icon,WHITE);
  }
  pos->y+=pos->h;
  return input.events[event]||(active&&input.events[EVENT_MOUSEUP]);
}
void widget_menuspacer(rect*pos){
  draw_fillslash(pos,BLACK);
  if(pos->y!=0) draw_hline(pos->x,pos->x+pos->w,pos->y,WHITE);
  pos->y+=pos->h;
}
int widget_menucolor(rect*pos,int color,int*selected){
  if(pos->y+2*pos->h>th)pos->h=th-pos->y; // last button in a column
  char* labels[]={"0","1","2","3",NULL,NULL};
  char* icons []={NULL,NULL,NULL,NULL,ICON_QUIET,ICON_SOUND};
  int   events[]={EVENT_1,EVENT_2,EVENT_3,EVENT_4,EVENT_5,EVENT_6};
  int active=mouse_in(pos)||input.events[events[color]];
  if(active)*selected=color;
  pos->h-=1;
  if(pos->y!=0)draw_hline(pos->x,pos->x+pos->w-1,pos->y-1,BLACK);
  if(*selected==color){
    rect i={pos->x,pos->y,pos->w,pos->h};
    draw_fill(&i,active?POPCOLOR:WHITE);
    inset(&i,2);
    draw_rect(&i,BLACK);
    inset(&i,1);
    draw_fill(&i,colors[color]);
  }
  else{
    draw_fill(pos,colors[color]);
  }
  pos->h+=1;
  if(labels[color]){
    rect tb;
    size_text(labels[color],0,0,&tb);
    int px=pos->x+(MENU_WIDTH-tb.w)/2, py=pos->y+(pos->h-tb.h)/2;
    draw_text(labels[color],px+1,py  ,BLACK);
    draw_text(labels[color],px+1,py+1,BLACK);
    draw_text(labels[color],px  ,py+1,BLACK);
    draw_text(labels[color],px,py,WHITE);
  }
  if(icons[color]){
    rect ib={pos->x+(MENU_WIDTH-16)/2,pos->y+(pos->h-16)/2,16,16};
    ib.x++;draw_icon(&ib,icons[color],BLACK);
    ib.y++;draw_icon(&ib,icons[color],BLACK);
    ib.x--;draw_icon(&ib,icons[color],BLACK);
    ib.y--;draw_icon(&ib,icons[color],WHITE);
  }
  pos->y+=pos->h;
  return input.events[events[color]]||(active&&input.events[EVENT_MOUSEUP]);
}
int widget_optionbar(rect*pos,char*label,char**options,int len,int*selected){
  rect t;
  size_text(label,0,0,&t);
  draw_text(label,pos->x,pos->y,WHITE);
  rect b={pos->x,pos->y+t.h+2,pos->w,4+octo_mono_font.height+6};
  int bw=pos->w/len;
  int change=-1, event=0;
  for(int z=0;z<len;z++){
    rect bb={b.x+(bw*z)+1,b.y+1,bw-1,b.h-2};
    if(mouse_in(&bb))change=z;
    if(change!=-1&&input.events[EVENT_MOUSEUP])event=1;
  }
  for(int z=0;z<len;z++){
    rect bb={b.x+(bw*z)+1,b.y+1,bw-1,b.h-2};
    draw_fill(&bb,(*selected==z&&change==-1)||change==z?POPCOLOR:BLACK);
    rect bl;
    size_text(options[z],0,0,&bl);
    draw_text(options[z],bb.x+(bb.w-bl.w)/2,bb.y+4,WHITE);
    if(z!=0)draw_vline(bb.x-1,bb.y,bb.y+bb.h,WHITE);
  }
  draw_rrect(&b,WHITE);
  pos->h=(b.y+b.h)-pos->y+6;
  if(event)(*selected)=change;
  return event;
}

void addr_name(octo_program*prog,char* dest,int len,uint16_t addr){
  if(prog==NULL)return;
  char*best=NULL; uint16_t best_offset=0xFFFF;
  for(int z=0;z<prog->constants.keys.count;z++){
    char*n=octo_list_get(&prog->constants.keys,z);
    octo_const*c=octo_list_get(&prog->constants.values,z);
    if(c->value==addr){best=n,best_offset=0;break;}
    if(c->value<addr&&addr-c->value<best_offset)best=n,best_offset=addr-c->value;
  }
  if     (best==NULL)    snprintf(dest,len," (unknown)");
  else if(best_offset==0)snprintf(dest,len," (%s)",best);
  else                   snprintf(dest,len," (%s + %d)",best,best_offset);
}

void octo_ui_registers(octo_emulator*emu,octo_program*prog){
  #define print draw_stext(line,10,y,&tb),y+=tb.h;
  rect tb; char line[1024]; int len=0, y=10;
  snprintf(line,1024,"%ld: %s",emu->ticks,emu->halt_message),print;
  y+=2;
  draw_shline(10,10+200,y-1);
  y+=2;
  len=snprintf(line,1024,"pc := 0x%04X",emu->pc),addr_name(prog,line+len,1024-len,emu->pc),print;
  len=snprintf(line,1024,"i  := 0x%04X",emu->i ),addr_name(prog,line+len,1024-len,emu->i ),print;
  for(int z=0;z<16;z++){
    int rlen=snprintf(line,1024,"v%X := 0x%02X",z,emu->v[z]), found=0;
    if(prog!=NULL)for(int a=0;a<prog->aliases.keys.count;a++){
      octo_reg*r=octo_list_get(&prog->aliases.values,a);
      if(r->value==z)rlen+=snprintf(line+rlen,1024-rlen,"%s%s",(found++==0)?" ":", ",(char*)octo_list_get(&prog->aliases.keys,a));
    }
    print;
  }
  y+=4;
  snprintf(line,1024,"Stack"),print;y+=4;
  draw_shline(10,10+200,y-2);
  for(int z=0;z<emu->rp;z++)len=snprintf(line,1024,"0x%04X",emu->ret[z]),addr_name(prog,line+len,1024-len,emu->ret[z]),print;
}

void octo_ui_monitors(octo_emulator*emu,octo_program*prog){
  #define print_k draw_stext(line,tw-(kw+20+mon_vw+10),y,&tb)
  #define print_v draw_stext(line,tw-(      mon_vw+10),y,&tb)
  rect tb; char line[1024]; int y=10, kw=7;
  for(int z=0;z<prog->monitors.keys.count;z++){
    octo_mon*m=octo_list_get(&prog->monitors.values,z);
    octo_monitor_format(emu,!m->type,m->base,m->len,m->format, line,1024);
    kw=MAX(kw,(int)strlen(octo_list_get(&prog->monitors.keys,z)));
    for(int z=0,t=0;z<(int)strlen(line);z++){if(line[z]=='\n')t=0;t++;mon_vw=MAX(mon_vw,t * octo_mono_font.maxwidth);}
  }
  kw*=octo_mono_font.maxwidth;
  if(kw>0){
    snprintf(line,1024,"Monitor"),print_k;
    snprintf(line,1024,"Data"   ),print_v,y+=tb.h+2;
    draw_shline(tw-(kw+20+mon_vw+10),tw-10,y-1),y+=1;
    for(int z=0;z<prog->monitors.keys.count;z++){
      snprintf(line,1024,"%s",(char*)octo_list_get(&prog->monitors.keys,z)),print_k;
      octo_mon*m=octo_list_get(&prog->monitors.values,z);
      octo_monitor_format(emu,!m->type,m->base,m->len,m->format, line,1024),print_v,y+=tb.h+2;
    }
    draw_svline(tw-(10+mon_vw+10),10,y);
  }
}

void octo_ui_init(SDL_Window*win,SDL_Renderer**ren,SDL_Texture**screen){
  if(*screen)SDL_DestroyTexture(*screen);
  if(*ren)SDL_DestroyRenderer(*ren);
  *ren=SDL_CreateRenderer(win,-1, ui.software_render?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED);
  *screen=SDL_CreateTexture(*ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,128,128); // oversized for rotation
}

void octo_ui_invalidate(octo_emulator*emu){emu->ppx[0]^=0xFF;}
void octo_ui_run(octo_emulator*emu,octo_program*prog,octo_ui_config*ui,SDL_Window*win,SDL_Renderer*ren,SDL_Texture*screen,SDL_Texture*overlay){
  // drop repaints if the display hasn't changed
  int dirty=memcmp(emu->px,emu->ppx,sizeof(emu->px))!=0, debug=emu->halt||ui->show_monitors;
  if(!dirty&&!debug)return;
  memcpy(emu->ppx,emu->px,sizeof(emu->ppx));

  // render chip8 display
  int *p, pitch, w=emu->hires?128:64, h=emu->hires?64:32;
  SDL_LockTexture(screen,NULL,(void**)&p,&pitch);
  int stride=pitch/sizeof(int);
  if(emu->options.rotation==  0) for(int y=0;y<h;y++)for(int x=0;x<w;x++)p[x+      (y*stride)      ]=emu->options.colors[emu->px[x+(y*w)]];
  if(emu->options.rotation== 90) for(int y=0;y<h;y++)for(int x=0;x<w;x++)p[(h-1-y)+(x*stride)      ]=emu->options.colors[emu->px[x+(y*w)]];
  if(emu->options.rotation==180) for(int y=0;y<h;y++)for(int x=0;x<w;x++)p[(w-1-x)+((h-1-y)*stride)]=emu->options.colors[emu->px[x+(y*w)]];
  if(emu->options.rotation==270) for(int y=0;y<h;y++)for(int x=0;x<w;x++)p[y+      ((w-1-x)*stride)]=emu->options.colors[emu->px[x+(y*w)]];
  if(emu->options.rotation==90||emu->options.rotation==270){int t=w;w=h,h=t;}
  SDL_UnlockTexture(screen);
  int dw, dh, border=5;
  SDL_GetWindowSize(win,&dw,&dh);
  int sx=(dw-border)/w,sy=(dh-border)/h, scale=sx<sy?sx:sy;
  SDL_Rect s_src={0,0,w,h};
  SDL_Rect s_dst={(dw-scale*w)/2,(dh-scale*h)/2,scale*w,scale*h};
  SDL_RenderCopy(ren,screen,&s_src,&s_dst);
  //render ui overlays
  if(debug){
    SDL_LockTexture(overlay,NULL,(void**)&p,&pitch);
    stride=pitch/sizeof(int);
    octo_ui_begin(&emu->options,p,stride,(dw/ui->win_scale),(dh/ui->win_scale),ui->win_scale);
    memset(p,0,sizeof(int)*stride*th);
    if(emu->halt)octo_ui_registers(emu,prog);
    if(ui->show_monitors)octo_ui_monitors(emu,prog);
    SDL_UnlockTexture(overlay);
    SDL_SetTextureBlendMode(overlay,SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(ren,overlay,NULL,NULL);
  }
  SDL_RenderPresent(ren);
}

Uint32 tick(Uint32 interval,void*param){
  SDL_Event e={SDL_USEREVENT};
  e.user.data1=param;
  SDL_PushEvent(&e);
  return interval;
}

/**
*
*  Path Manipulation/Enumeration
*
**/

#define OCTO_PATH_MAX 4096
#define OCTO_NAME_MAX 4096

#define OCTO_FILE_TYPE_DIRECTORY 0
#define OCTO_FILE_TYPE_CH8       1
#define OCTO_FILE_TYPE_8O        2
#define OCTO_FILE_TYPE_CARTRIDGE 3

char* octo_file_type_names[]={
  "Directory",
  "CHIP-8 Binary",
  "Octo Source",
  "Octo Cartridge",
};

typedef struct {
  int type;
  char name[OCTO_NAME_MAX];
} octo_path_entry;

#ifdef _WIN32
#include <windows.h>
#define SEPARATOR '\\'
#define HOME      "USERPROFILE"
#else
#include <dirent.h>
#define SEPARATOR '/'
#define HOME      "HOME"
#endif

void octo_path_home(char*path){
  snprintf(path,OCTO_PATH_MAX,"%s",getenv(HOME));
}
void octo_path_append(char*path,char*child){
  size_t len=strlen(path);
  if(len&&path[len-1]!=SEPARATOR)path[len++]= SEPARATOR;
  snprintf(path+len,OCTO_PATH_MAX-len,"%s",child);
}
void octo_path_parent(char*path){
  int len=strlen(path);
  while(len&&path[len]!=SEPARATOR)path[len--]='\0';
  if(path[len]==SEPARATOR)path[len--]='\0';
  if(len<1)path[0]=SEPARATOR,path[1]='\0';
}
void octo_name_set_extension(char*name,char*extension){
  int len=strlen(name), ext=len;
  while(ext>0&&name[ext]!= SEPARATOR &&name[ext]!='.')ext--;
  if(ext==0||name[ext]==SEPARATOR)snprintf(name+len,OCTO_NAME_MAX-len,".%s",extension);// append
  else                            snprintf(name+ext,OCTO_NAME_MAX-ext,".%s",extension);// replace
}
char* octo_name_get_extension(char*name){
  int len=strlen(name), ext=len;
  while(ext>0&&name[ext]!=SEPARATOR&&name[ext]!='.')ext--;
  return (ext==0||name[ext]==SEPARATOR)?name+len:name+ext; // "" or ".suffix"
}
int octo_path_sort(const void*a,const void*b){
  const octo_path_entry*ia=*((octo_path_entry**)a);
  const octo_path_entry*ib=*((octo_path_entry**)b);
  // directories come before files:
  if(ia->type==OCTO_FILE_TYPE_DIRECTORY&&ib->type!=OCTO_FILE_TYPE_DIRECTORY)return -1;
  if(ia->type!=OCTO_FILE_TYPE_DIRECTORY&&ib->type==OCTO_FILE_TYPE_DIRECTORY)return  1;
  // sort alphabetically by name:
  return strcmp(ia->name,ib->name);
}
void octo_path_record(octo_list* results, char* name, int is_dir) {
  char*ext=octo_name_get_extension(name);
  int type=is_dir               ?OCTO_FILE_TYPE_DIRECTORY:
           strcmp(ext,".8o" )==0?OCTO_FILE_TYPE_8O:
           strcmp(ext,".ch8")==0?OCTO_FILE_TYPE_CH8:
           strcmp(ext,".gif")==0?OCTO_FILE_TYPE_CARTRIDGE:-1;
  if(type==-1)return;
  octo_path_entry* x=malloc(sizeof(octo_path_entry));
  octo_list_append(results,x);
  x->type=type,snprintf(x->name,OCTO_NAME_MAX,"%s",name);
}

void octo_path_list(octo_list* results, char* path) {
    while (results->count)free(octo_list_remove(results, 0));
#ifdef _WIN32
    char wildcard[OCTO_PATH_MAX];
    wildcard[0]='\0';
    octo_path_append(wildcard,path);
    octo_path_append(wildcard,"*");
    WIN32_FIND_DATAA find;
    HANDLE d=FindFirstFileA(wildcard,&find);
    if(d==INVALID_HANDLE_VALUE)return;
    do{
      if(find.cFileName[0]=='.')continue;
      if(find.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)continue;
      octo_path_record(results,find.cFileName,find.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY?1:0);
    }while(FindNextFileA(d,&find));
    FindClose(d);
#else
    DIR*d=opendir(path);
    if(d==NULL)return;
    struct dirent*find;
    char work_path[OCTO_PATH_MAX];
    while((find=readdir(d))){
        if(find->d_name[0]=='.')continue; // skip invisible files and ./..
        work_path[0]='\0';
        octo_path_append(work_path,path);
        octo_path_append(work_path,find->d_name); // note: names could be UTF-8. need to handle this eventually!
        DIR*test=opendir(work_path);
        octo_path_record(results,find->d_name,test!=NULL);
        if(test!=NULL)closedir(test);
    }
    closedir(d);
#endif
    qsort(results->data, results->count, sizeof(void*), octo_path_sort);
}

/**
*
*  Configuration File
*
**/

void octo_load_config(octo_ui_config*ui,octo_options*o,const char*config_path){
  FILE*conf=fopen(config_path,"rb");
  if(conf==NULL){printf("unable to open %s\n",config_path);return;}
  char line[256];
  while(fgets(line,sizeof(line),conf)){
    if(strlen(line)<=1)continue; // skip empty lines
    if(line[0]=='#')continue;    // skip comments
    char key[256],value[256]; int ki=0,vi=0,si=0;
    while(ki<255&&line[si]&&line[si]!='=')key[ki++]=line[si++];
    key[ki]='\0';
    if(line[si]!='=')continue;
    si++;
    while(vi<255&&line[si]&&line[si]!='\n'&&line[si]!='\r')value[vi++]=line[si++];
    value[vi]='\0';
    if(vi==0)continue;

    // at this point we have a well-formed key and value:
    if(strcmp(key,"ui.windowed"       )==0)ui->windowed=atoi(value)!=0;
    if(strcmp(key,"ui.software_render")==0)ui->software_render=atoi(value)!=0;
    if(strcmp(key,"ui.win_scale"      )==0)ui->win_scale=CLAMP(0,atoi(value),4096);
    if(strcmp(key,"ui.win_width"      )==0)ui->win_width=CLAMP(0,atoi(value),4096);
    if(strcmp(key,"ui.win_height"     )==0)ui->win_height=CLAMP(0,atoi(value),4096);
    if(strcmp(key,"ui.volume"         )==0)ui->volume=CLAMP(0,atoi(value),127);

    if(strcmp(key,"core.tickrate")==0)o->tickrate=CLAMP(1,atoi(value),50000);
    if(strcmp(key,"core.max_rom" )==0){
      o->max_rom=atoi(value);
      if(o->max_rom!=3216&&o->max_rom!=3583&&o->max_rom!=3584)o->max_rom=65024;
    }
    if(strcmp(key,"core.rotation")==0){
      o->rotation=atoi(value);
      if(o->rotation!=90&&o->rotation!=180&&o->rotation!=270)o->rotation=0;
    }
    if(strcmp(key,"core.font")==0){
      o->font=strcmp(value,"vip"       )==0?OCTO_FONT_VIP:
              strcmp(value,"dream_6800")==0?OCTO_FONT_DREAM_6800:
              strcmp(value,"eti_660"   )==0?OCTO_FONT_ETI_660:
              strcmp(value,"schip"     )==0?OCTO_FONT_SCHIP:
              strcmp(value,"fish"      )==0?OCTO_FONT_FISH: OCTO_FONT_OCTO;
    }
    if(strcmp(key,"core.touch_mode")==0){
      o->touch_mode=strcmp(value,"swipe"     )==0?OCTO_TOUCH_SWIPE:
                    strcmp(value,"seg16"     )==0?OCTO_TOUCH_SEG16:
                    strcmp(value,"seg16_fill")==0?OCTO_TOUCH_SEG16_FILL:
                    strcmp(value,"gamepad"   )==0?OCTO_TOUCH_GAMEPAD:
                    strcmp(value,"vip"       )==0?OCTO_TOUCH_VIP: OCTO_TOUCH_NONE;
    }
    if(strcmp(key,"quirks.shift"    )==0)o->q_shift    =atoi(value)!=0;
    if(strcmp(key,"quirks.loadstore")==0)o->q_loadstore=atoi(value)!=0;
    if(strcmp(key,"quirks.jump0"    )==0)o->q_jump0    =atoi(value)!=0;
    if(strcmp(key,"quirks.logic"    )==0)o->q_logic    =atoi(value)!=0;
    if(strcmp(key,"quirks.clip"     )==0)o->q_clip     =atoi(value)!=0;
    if(strcmp(key,"quirks.vblank"   )==0)o->q_vblank   =atoi(value)!=0;
    char* colors[OCTO_PALETTE_SIZE]={"color.plane0","color.plane1","color.plane2","color.plane3","color.background","color.sound"};
    for(int z=0;z<OCTO_PALETTE_SIZE;z++){
      if(strcmp(key,colors[z])==0){
        int c=0;
        sscanf(value,"%x",&c);
        o->colors[z]=0xFF000000|c;
      }
    }
  }
  fclose(conf);
}

void octo_load_config_default(octo_ui_config*ui,octo_options*o){
  ui->windowed=1, ui->software_render=0, ui->win_width=480, ui->win_height=272, ui->win_scale=2, ui->volume=20;
  ui->show_monitors=0;
  char config_path[OCTO_PATH_MAX];
  octo_path_home(config_path);
  octo_path_append(config_path,".octo.rc");
  octo_default_options(o);
  octo_load_config(ui,o,config_path);
}

/**
*
*  Audio
*
**/

#define AUDIO_FRAG_SIZE 1024
#define AUDIO_SAMPLE_RATE (4096*8)

SDL_AudioSpec audio;

void audio_pump(void*user,Uint8*stream,int len){
  octo_emulator*emu=user;
  double freq=4000*pow(2,(emu->pitch-64)/48.0);
  for(int z=0;z<len;z++){
    int ip=emu->osc;
    stream[z]=!emu->had_sound?audio.silence: (emu->pattern[ip>>3]>>((ip&7)^7))&1?ui.volume: audio.silence;
    emu->osc=fmod(emu->osc+(freq/AUDIO_SAMPLE_RATE),128.0);
  }
  emu->had_sound=0;
}

void audio_init(octo_emulator*emu){
  SDL_memset(&audio,0,sizeof(audio));
  audio.freq=AUDIO_SAMPLE_RATE;
  audio.format=AUDIO_S8;
  audio.channels=1;
  audio.samples=AUDIO_FRAG_SIZE;
  audio.callback=audio_pump;
  audio.userdata=emu;
  if(ui.volume){
    if(SDL_OpenAudio(&audio,NULL)){printf("failed to initialize audio: %s\n",SDL_GetError());}
    else{SDL_PauseAudio(0);}
  }
}

/**
*
*  Emulation
*
**/

void emu_step(octo_emulator*emu,octo_program*prog){
  if(emu->halt)return;
  for(int z=0;z<emu->options.tickrate&&!emu->halt;z++){
    if(emu->options.q_vblank&&(emu->ram[emu->pc]&0xF0)==0xD0)z=emu->options.tickrate;
    octo_emulator_instruction(emu);
    if(prog!=NULL&&prog->breakpoints[emu->pc]) emu->halt=1,snprintf(emu->halt_message,OCTO_HALT_MAX,"%s",prog->breakpoints[emu->pc]);
  }
  if(emu->dt>0)emu->dt--;
  if(emu->st>0)emu->st--,emu->had_sound=1;
}

void random_init() {
  srand(time(NULL));
}

/**
*
*  Source Files
*
**/

void octo_load_program(octo_ui_config*ui,octo_emulator*emu,octo_program**prog,const char* filename,const char* options){
  octo_options defaults;
  octo_load_config_default(ui,&defaults);
  if(options)octo_load_config(ui,&defaults,options);
  char*source;
  struct stat st;
  if(stat(filename,&st)!=0){
    fprintf(stderr,"%s: No such file or directory\n",filename);
    exit(1);
  }
  size_t source_size=st.st_size;

  if(strcmp(".ch8",filename+(strlen(filename)-4))==0){
    source=malloc(source_size+1);
    FILE*source_file=fopen(filename,"rb");
    fread(source,sizeof(char),source_size,source_file);
    fclose(source_file);
    octo_emulator_init(emu, source, source_size, &defaults, NULL);
  }
  else if(strcmp(".8o",filename+(strlen(filename)-3))==0){
    source=malloc(source_size+1);
    FILE*source_file=fopen(filename,"rb");
    fread(source,sizeof(char),source_size,source_file);
    source[source_size]='\0';
    fclose(source_file);
    octo_program*p=(*prog)=octo_compile_str(source);
    if(p->is_error){
      fprintf(stderr,"(%d:%d) %s\n",p->error_line+1,p->error_pos+1,p->error);
      octo_free_program(p),exit(1);
    }
    octo_emulator_init(emu,p->rom+0x200,p->length-0x200,&defaults,NULL);
  }
  else if(strcmp(".gif",filename+(strlen(filename)-4))==0){
    char* source=octo_cart_load(filename,&defaults);
    if(source==NULL){
      fprintf(stderr,"%s: Unable to load octocart\n",filename);
      exit(1);
    }
    octo_program*p=(*prog)=octo_compile_str(source);
    if(p->is_error){
      fprintf(stderr,"(%d:%d) %s\n",p->error_line+1,p->error_pos+1,p->error);
      octo_free_program(p),exit(1);
    }
    octo_emulator_init(emu,p->rom+0x200,p->length-0x200,&defaults,NULL);
  }
  else {
    fprintf(stderr,"source file must be a .ch8 or .8o\n");
    exit(1);
  }
}
