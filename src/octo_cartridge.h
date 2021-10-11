/**
*
*  octo_cartridge.h
*
*  encoder and decoder for octo cartridge files,
*  or 'octocarts'. a json payload with emulator
*  options and source code is steganographically
*  encoded into the frames of a .GIF animation.
*
*  the public interface is octo_cart_save(),
*  which saves a file, given source code and
*  emulator options, and octo_cart_load(),
*  which extracts source code and populates
*  options from an existing file.
*
**/

#define OCTO_CART_BLOCK 256
#define CLAMP(a,x,b) ((x)<(a)?(a): (x)>(b)?(b): (x))
#define MIN(a,b)     ((a)<(b)?(a):(b))

typedef struct {
  char* root;
  int   size;
  int   pos;
} octo_str;

void octo_str_init(octo_str*s){
  s->pos=0, s->size=OCTO_CART_BLOCK, s->root=malloc(s->size);
}
void octo_str_destroy(octo_str*s){
  free(s->root);
}
void octo_str_append(octo_str*s,char c){
  if(s->pos>=s->size)s->root=realloc(s->root,s->size+=OCTO_CART_BLOCK);
  s->root[s->pos++]=c;
}
void octo_str_join(octo_str*s,char*string){
  for(int z=0;string[z];z++)octo_str_append(s,string[z]);
}
void octo_str_short(octo_str*s,uint16_t n){
  octo_str_append(s,0xFF&n),octo_str_append(s,0xFF&(n>>8));
}
char octo_str_peek(octo_str*s){
  return s->root[s->pos];
}
uint8_t octo_str_next(octo_str*s){
  return s->root[s->pos++];
}
uint16_t octo_str_nextshort(octo_str*s){
  uint16_t a=octo_str_next(s), b=octo_str_next(s);
  return (b<<8)|a;
}
int octo_str_match(octo_str*s,char*str){
  for(int z=0;str[z];z++)if(s->root[s->pos+z]!=str[z])return 0;
  return (s->pos+=strlen(str)),1;
}

/**
*
* JSON formatting
*
**/

void octo_json_string(octo_str*s,char*str){
  octo_str_append(s,'"');
  for(int z=0;str[z];z++){
    char c=0xFF&str[z]; // ascii characters only(!), so no need to emit \uXXXX.
    #define octo_json_esc(char,esc) c==char?(octo_str_append(s,'\\'),octo_str_append(s,esc)):
    octo_json_esc('"','"')  octo_json_esc('\\','\\') octo_json_esc('/','/')  octo_json_esc('\b','b')
    octo_json_esc('\f','f') octo_json_esc('\n','n')  octo_json_esc('\r','r') octo_json_esc('\t','t')
    octo_str_append(s,c);
  }
  octo_str_append(s,'"');
}
void octo_json_map_str(octo_str*s,char*key,char*val){
  octo_json_string(s,key),octo_str_append(s,':'),octo_json_string(s,val);
}
void octo_json_map_num(octo_str*s,char*key,int num){
  octo_json_string(s,key),octo_str_append(s,':');
  char n[256]; snprintf(n,255,"%d",num);
  for(int z=0;n[z];z++)octo_str_append(s,n[z]);
}
void octo_json_map_bool(octo_str*s,char*key,int num){
  octo_json_string(s,key),octo_str_append(s,':');
  char* n=num?"true":"false";
  for(int z=0;n[z];z++)octo_str_append(s,n[z]);
}
void octo_json_map_color(octo_str*s,char*key,int num){
  octo_json_string(s,key),octo_str_append(s,':');
  char n[256]; snprintf(n,255,"#%06X",num&0xFFFFFF);
  octo_json_string(s,n);
}

char* octo_touch_modes[]={"none","swipe","seg16","seg16fill","gamepad","vip"};
char* octo_font_styles[]={"octo","vip","dream6800","eti660","schip","fish"};

void octo_cart_format_json(octo_str*s,char*program,octo_options*o){
  octo_str_append(s,'{');
  octo_json_map_str(s,"program",program);
  octo_str_append(s,',');
  octo_json_string(s,"options");
  octo_str_append(s,':');
  octo_str_append(s,'{');
  octo_json_map_num  (s,"tickrate",        o->tickrate);                     octo_str_append(s,',');
  octo_json_map_color(s,"fillColor",       o->colors[1]);                    octo_str_append(s,',');
  octo_json_map_color(s,"fillColor2",      o->colors[2]);                    octo_str_append(s,',');
  octo_json_map_color(s,"blendColor",      o->colors[3]);                    octo_str_append(s,',');
  octo_json_map_color(s,"backgroundColor", o->colors[0]);                    octo_str_append(s,',');
  octo_json_map_color(s,"buzzColor",       o->colors[5]);                    octo_str_append(s,',');
  octo_json_map_color(s,"quietColor",      o->colors[4]);                    octo_str_append(s,',');
  octo_json_map_bool (s,"shiftQuirks",     o->q_shift);                      octo_str_append(s,',');
  octo_json_map_bool (s,"loadStoreQuirks", o->q_loadstore);                  octo_str_append(s,',');
  octo_json_map_bool (s,"vfOrderQuirks",   0); /*deprecated*/                octo_str_append(s,',');
  octo_json_map_bool (s,"clipQuirks",      o->q_clip);                       octo_str_append(s,',');
  octo_json_map_bool (s,"vBlankQuirks",    o->q_vblank);                     octo_str_append(s,',');
  octo_json_map_bool (s,"jumpQuirks",      o->q_jump0);                      octo_str_append(s,',');
  octo_json_map_bool (s,"logicQuirks",     o->q_logic);                      octo_str_append(s,',');
  octo_json_map_num  (s,"screenRotation",  o->rotation);                     octo_str_append(s,',');
  octo_json_map_num  (s,"maxSize",         o->max_rom);                      octo_str_append(s,',');
  octo_json_map_str  (s,"touchInputMode",  octo_touch_modes[o->touch_mode]); octo_str_append(s,',');
  octo_json_map_str  (s,"fontStyle",       octo_font_styles[o->font]);
  octo_str_append(s,'}');
  octo_str_append(s,'}');
  octo_str_append(s,'\0');
}

/**
*
* JSON parsing
*
**/

char* octo_css_color_names[]={
  "alice blue","aliceblue","antique white","antiquewhite","antiquewhite1","antiquewhite2","antiquewhite3","antiquewhite4","aqua",
  "aquamarine","aquamarine1","aquamarine2","aquamarine3","aquamarine4","azure","azure1","azure2","azure3","azure4","beige","bisque",
  "bisque1","bisque2","bisque3","bisque4","black","blanched almond","blanchedalmond","blue","blue violet","blue1","blue2","blue3",
  "blue4","blueviolet","brown","brown1","brown2","brown3","brown4","burlywood","burlywood1","burlywood2","burlywood3","burlywood4",
  "cadet blue","cadetblue","cadetblue1","cadetblue2","cadetblue3","cadetblue4","chartreuse","chartreuse1","chartreuse2",
  "chartreuse3","chartreuse4","chocolate","chocolate1","chocolate2","chocolate3","chocolate4","coral","coral1","coral2","coral3",
  "coral4","cornflower blue","cornflowerblue","cornsilk","cornsilk1","cornsilk2","cornsilk3","cornsilk4","crimson","cyan","cyan1",
  "cyan2","cyan3","cyan4","dark blue","dark cyan","dark goldenrod","dark gray","dark green","dark grey","dark khaki","dark magenta",
  "dark olive green","dark orange","dark orchid","dark red","dark salmon","dark sea green","dark slate blue","dark slate gray",
  "dark slate grey","dark turquoise","dark violet","darkblue","darkcyan","darkgoldenrod","darkgoldenrod1","darkgoldenrod2",
  "darkgoldenrod3","darkgoldenrod4","darkgray","darkgreen","darkgrey","darkkhaki","darkmagenta","darkolivegreen","darkolivegreen1",
  "darkolivegreen2","darkolivegreen3","darkolivegreen4","darkorange","darkorange1","darkorange2","darkorange3","darkorange4",
  "darkorchid","darkorchid1","darkorchid2","darkorchid3","darkorchid4","darkred","darksalmon","darkseagreen","darkseagreen1",
  "darkseagreen2","darkseagreen3","darkseagreen4","darkslateblue","darkslategray","darkslategray1","darkslategray2",
  "darkslategray3","darkslategray4","darkslategrey","darkturquoise","darkviolet","deep pink","deep sky blue","deeppink","deeppink1",
  "deeppink2","deeppink3","deeppink4","deepskyblue","deepskyblue1","deepskyblue2","deepskyblue3","deepskyblue4","dim gray",
  "dim grey","dimgray","dimgrey","dodger blue","dodgerblue","dodgerblue1","dodgerblue2","dodgerblue3","dodgerblue4","firebrick",
  "firebrick1","firebrick2","firebrick3","firebrick4","floral white","floralwhite","forest green","forestgreen","fuchsia",
  "gainsboro","ghost white","ghostwhite","gold","gold1","gold2","gold3","gold4","goldenrod","goldenrod1","goldenrod2","goldenrod3",
  "goldenrod4","gray","gray0","gray1","gray10","gray100","gray11","gray12","gray13","gray14","gray15","gray16","gray17","gray18",
  "gray19","gray2","gray20","gray21","gray22","gray23","gray24","gray25","gray26","gray27","gray28","gray29","gray3","gray30",
  "gray31","gray32","gray33","gray34","gray35","gray36","gray37","gray38","gray39","gray4","gray40","gray41","gray42","gray43",
  "gray44","gray45","gray46","gray47","gray48","gray49","gray5","gray50","gray51","gray52","gray53","gray54","gray55","gray56",
  "gray57","gray58","gray59","gray6","gray60","gray61","gray62","gray63","gray64","gray65","gray66","gray67","gray68","gray69",
  "gray7","gray70","gray71","gray72","gray73","gray74","gray75","gray76","gray77","gray78","gray79","gray8","gray80","gray81",
  "gray82","gray83","gray84","gray85","gray86","gray87","gray88","gray89","gray9","gray90","gray91","gray92","gray93","gray94",
  "gray95","gray96","gray97","gray98","gray99","green","green yellow","green1","green2","green3","green4","greenyellow","grey",
  "grey0","grey1","grey10","grey100","grey11","grey12","grey13","grey14","grey15","grey16","grey17","grey18","grey19","grey2",
  "grey20","grey21","grey22","grey23","grey24","grey25","grey26","grey27","grey28","grey29","grey3","grey30","grey31","grey32",
  "grey33","grey34","grey35","grey36","grey37","grey38","grey39","grey4","grey40","grey41","grey42","grey43","grey44","grey45",
  "grey46","grey47","grey48","grey49","grey5","grey50","grey51","grey52","grey53","grey54","grey55","grey56","grey57","grey58",
  "grey59","grey6","grey60","grey61","grey62","grey63","grey64","grey65","grey66","grey67","grey68","grey69","grey7","grey70",
  "grey71","grey72","grey73","grey74","grey75","grey76","grey77","grey78","grey79","grey8","grey80","grey81","grey82","grey83",
  "grey84","grey85","grey86","grey87","grey88","grey89","grey9","grey90","grey91","grey92","grey93","grey94","grey95","grey96",
  "grey97","grey98","grey99","honeydew","honeydew1","honeydew2","honeydew3","honeydew4","hot pink","hotpink","hotpink1","hotpink2",
  "hotpink3","hotpink4","indian red","indianred","indianred1","indianred2","indianred3","indianred4","indigo","ivory","ivory1",
  "ivory2","ivory3","ivory4","khaki","khaki1","khaki2","khaki3","khaki4","lavender","lavender blush","lavenderblush",
  "lavenderblush1","lavenderblush2","lavenderblush3","lavenderblush4","lawn green","lawngreen","lemon chiffon","lemonchiffon",
  "lemonchiffon1","lemonchiffon2","lemonchiffon3","lemonchiffon4","light blue","light coral","light cyan","light goldenrod",
  "light goldenrod yellow","light gray","light green","light grey","light pink","light salmon","light sea green","light sky blue",
  "light slate blue","light slate gray","light slate grey","light steel blue","light yellow","lightblue","lightblue1","lightblue2",
  "lightblue3","lightblue4","lightcoral","lightcyan","lightcyan1","lightcyan2","lightcyan3","lightcyan4","lightgoldenrod",
  "lightgoldenrod1","lightgoldenrod2","lightgoldenrod3","lightgoldenrod4","lightgoldenrodyellow","lightgray","lightgreen",
  "lightgrey","lightpink","lightpink1","lightpink2","lightpink3","lightpink4","lightsalmon","lightsalmon1","lightsalmon2",
  "lightsalmon3","lightsalmon4","lightseagreen","lightskyblue","lightskyblue1","lightskyblue2","lightskyblue3","lightskyblue4",
  "lightslateblue","lightslategray","lightslategrey","lightsteelblue","lightsteelblue1","lightsteelblue2","lightsteelblue3",
  "lightsteelblue4","lightyellow","lightyellow1","lightyellow2","lightyellow3","lightyellow4","lime","lime green","limegreen",
  "linen","magenta","magenta1","magenta2","magenta3","magenta4","maroon","maroon1","maroon2","maroon3","maroon4",
  "medium aquamarine","medium blue","medium orchid","medium purple","medium sea green","medium slate blue","medium spring green",
  "medium turquoise","medium violet red","mediumaquamarine","mediumblue","mediumorchid","mediumorchid1","mediumorchid2",
  "mediumorchid3","mediumorchid4","mediumpurple","mediumpurple1","mediumpurple2","mediumpurple3","mediumpurple4","mediumseagreen",
  "mediumslateblue","mediumspringgreen","mediumturquoise","mediumvioletred","midnight blue","midnightblue","mint cream","mintcream",
  "misty rose","mistyrose","mistyrose1","mistyrose2","mistyrose3","mistyrose4","moccasin","navajo white","navajowhite",
  "navajowhite1","navajowhite2","navajowhite3","navajowhite4","navy","navy blue","navyblue","old lace","oldlace","olive",
  "olive drab","olivedrab","olivedrab1","olivedrab2","olivedrab3","olivedrab4","orange","orange red","orange1","orange2","orange3",
  "orange4","orangered","orangered1","orangered2","orangered3","orangered4","orchid","orchid1","orchid2","orchid3","orchid4",
  "pale goldenrod","pale green","pale turquoise","pale violet red","palegoldenrod","palegreen","palegreen1","palegreen2",
  "palegreen3","palegreen4","paleturquoise","paleturquoise1","paleturquoise2","paleturquoise3","paleturquoise4","palevioletred",
  "palevioletred1","palevioletred2","palevioletred3","palevioletred4","papaya whip","papayawhip","peach puff","peachpuff",
  "peachpuff1","peachpuff2","peachpuff3","peachpuff4","peru","pink","pink1","pink2","pink3","pink4","plum","plum1","plum2","plum3",
  "plum4","powder blue","powderblue","purple","purple1","purple2","purple3","purple4","rebecca purple","rebeccapurple","red","red1",
  "red2","red3","red4","rosy brown","rosybrown","rosybrown1","rosybrown2","rosybrown3","rosybrown4","royal blue","royalblue",
  "royalblue1","royalblue2","royalblue3","royalblue4","saddle brown","saddlebrown","salmon","salmon1","salmon2","salmon3","salmon4",
  "sandy brown","sandybrown","sea green","seagreen","seagreen1","seagreen2","seagreen3","seagreen4","seashell","seashell1",
  "seashell2","seashell3","seashell4","sienna","sienna1","sienna2","sienna3","sienna4","silver","sky blue","skyblue","skyblue1",
  "skyblue2","skyblue3","skyblue4","slate blue","slate gray","slate grey","slateblue","slateblue1","slateblue2","slateblue3",
  "slateblue4","slategray","slategray1","slategray2","slategray3","slategray4","slategrey","snow","snow1","snow2","snow3","snow4",
  "spring green","springgreen","springgreen1","springgreen2","springgreen3","springgreen4","steel blue","steelblue","steelblue1",
  "steelblue2","steelblue3","steelblue4","tan","tan1","tan2","tan3","tan4","teal","thistle","thistle1","thistle2","thistle3",
  "thistle4","tomato","tomato1","tomato2","tomato3","tomato4","turquoise","turquoise1","turquoise2","turquoise3","turquoise4",
  "violet","violet red","violetred","violetred1","violetred2","violetred3","violetred4","web gray","web green","web grey",
  "web maroon","web purple","webgray","webgreen","webgrey","webmaroon","webpurple","wheat","wheat1","wheat2","wheat3","wheat4",
  "white","white smoke","whitesmoke","x11 gray","x11 green","x11 grey","x11 maroon","x11 purple","x11gray","x11green","x11grey",
  "x11maroon","x11purple","yellow","yellow green","yellow1","yellow2","yellow3","yellow4","yellowgreen",
};
int octo_css_color_values[]={
  0xF0F8FF,0xF0F8FF,0xFAEBD7,0xFAEBD7,0xFFEFDB,0xEEDFCC,0xCDC0B0,0x8B8378,0x00FFFF,0x7FFFD4,0x7FFFD4,0x76EEC6,0x66CDAA,0x458B74,0xF0FFFF,0xF0FFFF,
  0xE0EEEE,0xC1CDCD,0x838B8B,0xF5F5DC,0xFFE4C4,0xFFE4C4,0xEED5B7,0xCDB79E,0x8B7D6B,0x000000,0xFFEBCD,0xFFEBCD,0x0000FF,0x8A2BE2,0x0000FF,0x0000EE,
  0x0000CD,0x00008B,0x8A2BE2,0xA52A2A,0xFF4040,0xEE3B3B,0xCD3333,0x8B2323,0xDEB887,0xFFD39B,0xEEC591,0xCDAA7D,0x8B7355,0x5F9EA0,0x5F9EA0,0x98F5FF,
  0x8EE5EE,0x7AC5CD,0x53868B,0x7FFF00,0x7FFF00,0x76EE00,0x66CD00,0x458B00,0xD2691E,0xFF7F24,0xEE7621,0xCD661D,0x8B4513,0xFF7F50,0xFF7256,0xEE6A50,
  0xCD5B45,0x8B3E2F,0x6495ED,0x6495ED,0xFFF8DC,0xFFF8DC,0xEEE8CD,0xCDC8B1,0x8B8878,0xDC143C,0x00FFFF,0x00FFFF,0x00EEEE,0x00CDCD,0x008B8B,0x00008B,
  0x008B8B,0xB8860B,0xA9A9A9,0x006400,0xA9A9A9,0xBDB76B,0x8B008B,0x556B2F,0xFF8C00,0x9932CC,0x8B0000,0xE9967A,0x8FBC8F,0x483D8B,0x2F4F4F,0x2F4F4F,
  0x00CED1,0x9400D3,0x00008B,0x008B8B,0xB8860B,0xFFB90F,0xEEAD0E,0xCD950C,0x8B6508,0xA9A9A9,0x006400,0xA9A9A9,0xBDB76B,0x8B008B,0x556B2F,0xCAFF70,
  0xBCEE68,0xA2CD5A,0x6E8B3D,0xFF8C00,0xFF7F00,0xEE7600,0xCD6600,0x8B4500,0x9932CC,0xBF3EFF,0xB23AEE,0x9A32CD,0x68228B,0x8B0000,0xE9967A,0x8FBC8F,
  0xC1FFC1,0xB4EEB4,0x9BCD9B,0x698B69,0x483D8B,0x2F4F4F,0x97FFFF,0x8DEEEE,0x79CDCD,0x528B8B,0x2F4F4F,0x00CED1,0x9400D3,0xFF1493,0x00BFFF,0xFF1493,
  0xFF1493,0xEE1289,0xCD1076,0x8B0A50,0x00BFFF,0x00BFFF,0x00B2EE,0x009ACD,0x00688B,0x696969,0x696969,0x696969,0x696969,0x1E90FF,0x1E90FF,0x1E90FF,
  0x1C86EE,0x1874CD,0x104E8B,0xB22222,0xFF3030,0xEE2C2C,0xCD2626,0x8B1A1A,0xFFFAF0,0xFFFAF0,0x228B22,0x228B22,0xFF00FF,0xDCDCDC,0xF8F8FF,0xF8F8FF,
  0xFFD700,0xFFD700,0xEEC900,0xCDAD00,0x8B7500,0xDAA520,0xFFC125,0xEEB422,0xCD9B1D,0x8B6914,0xBEBEBE,0x000000,0x030303,0x1A1A1A,0xFFFFFF,0x1C1C1C,
  0x1F1F1F,0x212121,0x242424,0x262626,0x292929,0x2B2B2B,0x2E2E2E,0x303030,0x050505,0x333333,0x363636,0x383838,0x3B3B3B,0x3D3D3D,0x404040,0x424242,
  0x454545,0x474747,0x4A4A4A,0x080808,0x4D4D4D,0x4F4F4F,0x525252,0x545454,0x575757,0x595959,0x5C5C5C,0x5E5E5E,0x616161,0x636363,0x0A0A0A,0x666666,
  0x696969,0x6B6B6B,0x6E6E6E,0x707070,0x737373,0x757575,0x787878,0x7A7A7A,0x7D7D7D,0x0D0D0D,0x7F7F7F,0x828282,0x858585,0x878787,0x8A8A8A,0x8C8C8C,
  0x8F8F8F,0x919191,0x949494,0x969696,0x0F0F0F,0x999999,0x9C9C9C,0x9E9E9E,0xA1A1A1,0xA3A3A3,0xA6A6A6,0xA8A8A8,0xABABAB,0xADADAD,0xB0B0B0,0x121212,
  0xB3B3B3,0xB5B5B5,0xB8B8B8,0xBABABA,0xBDBDBD,0xBFBFBF,0xC2C2C2,0xC4C4C4,0xC7C7C7,0xC9C9C9,0x141414,0xCCCCCC,0xCFCFCF,0xD1D1D1,0xD4D4D4,0xD6D6D6,
  0xD9D9D9,0xDBDBDB,0xDEDEDE,0xE0E0E0,0xE3E3E3,0x171717,0xE5E5E5,0xE8E8E8,0xEBEBEB,0xEDEDED,0xF0F0F0,0xF2F2F2,0xF5F5F5,0xF7F7F7,0xFAFAFA,0xFCFCFC,
  0x00FF00,0xADFF2F,0x00FF00,0x00EE00,0x00CD00,0x008B00,0xADFF2F,0xBEBEBE,0x000000,0x030303,0x1A1A1A,0xFFFFFF,0x1C1C1C,0x1F1F1F,0x212121,0x242424,
  0x262626,0x292929,0x2B2B2B,0x2E2E2E,0x303030,0x050505,0x333333,0x363636,0x383838,0x3B3B3B,0x3D3D3D,0x404040,0x424242,0x454545,0x474747,0x4A4A4A,
  0x080808,0x4D4D4D,0x4F4F4F,0x525252,0x545454,0x575757,0x595959,0x5C5C5C,0x5E5E5E,0x616161,0x636363,0x0A0A0A,0x666666,0x696969,0x6B6B6B,0x6E6E6E,
  0x707070,0x737373,0x757575,0x787878,0x7A7A7A,0x7D7D7D,0x0D0D0D,0x7F7F7F,0x828282,0x858585,0x878787,0x8A8A8A,0x8C8C8C,0x8F8F8F,0x919191,0x949494,
  0x969696,0x0F0F0F,0x999999,0x9C9C9C,0x9E9E9E,0xA1A1A1,0xA3A3A3,0xA6A6A6,0xA8A8A8,0xABABAB,0xADADAD,0xB0B0B0,0x121212,0xB3B3B3,0xB5B5B5,0xB8B8B8,
  0xBABABA,0xBDBDBD,0xBFBFBF,0xC2C2C2,0xC4C4C4,0xC7C7C7,0xC9C9C9,0x141414,0xCCCCCC,0xCFCFCF,0xD1D1D1,0xD4D4D4,0xD6D6D6,0xD9D9D9,0xDBDBDB,0xDEDEDE,
  0xE0E0E0,0xE3E3E3,0x171717,0xE5E5E5,0xE8E8E8,0xEBEBEB,0xEDEDED,0xF0F0F0,0xF2F2F2,0xF5F5F5,0xF7F7F7,0xFAFAFA,0xFCFCFC,0xF0FFF0,0xF0FFF0,0xE0EEE0,
  0xC1CDC1,0x838B83,0xFF69B4,0xFF69B4,0xFF6EB4,0xEE6AA7,0xCD6090,0x8B3A62,0xCD5C5C,0xCD5C5C,0xFF6A6A,0xEE6363,0xCD5555,0x8B3A3A,0x4B0082,0xFFFFF0,
  0xFFFFF0,0xEEEEE0,0xCDCDC1,0x8B8B83,0xF0E68C,0xFFF68F,0xEEE685,0xCDC673,0x8B864E,0xE6E6FA,0xFFF0F5,0xFFF0F5,0xFFF0F5,0xEEE0E5,0xCDC1C5,0x8B8386,
  0x7CFC00,0x7CFC00,0xFFFACD,0xFFFACD,0xFFFACD,0xEEE9BF,0xCDC9A5,0x8B8970,0xADD8E6,0xF08080,0xE0FFFF,0xEEDD82,0xFAFAD2,0xD3D3D3,0x90EE90,0xD3D3D3,
  0xFFB6C1,0xFFA07A,0x20B2AA,0x87CEFA,0x8470FF,0x778899,0x778899,0xB0C4DE,0xFFFFE0,0xADD8E6,0xBFEFFF,0xB2DFEE,0x9AC0CD,0x68838B,0xF08080,0xE0FFFF,
  0xE0FFFF,0xD1EEEE,0xB4CDCD,0x7A8B8B,0xEEDD82,0xFFEC8B,0xEEDC82,0xCDBE70,0x8B814C,0xFAFAD2,0xD3D3D3,0x90EE90,0xD3D3D3,0xFFB6C1,0xFFAEB9,0xEEA2AD,
  0xCD8C95,0x8B5F65,0xFFA07A,0xFFA07A,0xEE9572,0xCD8162,0x8B5742,0x20B2AA,0x87CEFA,0xB0E2FF,0xA4D3EE,0x8DB6CD,0x607B8B,0x8470FF,0x778899,0x778899,
  0xB0C4DE,0xCAE1FF,0xBCD2EE,0xA2B5CD,0x6E7B8B,0xFFFFE0,0xFFFFE0,0xEEEED1,0xCDCDB4,0x8B8B7A,0x00FF00,0x32CD32,0x32CD32,0xFAF0E6,0xFF00FF,0xFF00FF,
  0xEE00EE,0xCD00CD,0x8B008B,0xB03060,0xFF34B3,0xEE30A7,0xCD2990,0x8B1C62,0x66CDAA,0x0000CD,0xBA55D3,0x9370DB,0x3CB371,0x7B68EE,0x00FA9A,0x48D1CC,
  0xC71585,0x66CDAA,0x0000CD,0xBA55D3,0xE066FF,0xD15FEE,0xB452CD,0x7A378B,0x9370DB,0xAB82FF,0x9F79EE,0x8968CD,0x5D478B,0x3CB371,0x7B68EE,0x00FA9A,
  0x48D1CC,0xC71585,0x191970,0x191970,0xF5FFFA,0xF5FFFA,0xFFE4E1,0xFFE4E1,0xFFE4E1,0xEED5D2,0xCDB7B5,0x8B7D7B,0xFFE4B5,0xFFDEAD,0xFFDEAD,0xFFDEAD,
  0xEECFA1,0xCDB38B,0x8B795E,0x000080,0x000080,0x000080,0xFDF5E6,0xFDF5E6,0x808000,0x6B8E23,0x6B8E23,0xC0FF3E,0xB3EE3A,0x9ACD32,0x698B22,0xFFA500,
  0xFF4500,0xFFA500,0xEE9A00,0xCD8500,0x8B5A00,0xFF4500,0xFF4500,0xEE4000,0xCD3700,0x8B2500,0xDA70D6,0xFF83FA,0xEE7AE9,0xCD69C9,0x8B4789,0xEEE8AA,
  0x98FB98,0xAFEEEE,0xDB7093,0xEEE8AA,0x98FB98,0x9AFF9A,0x90EE90,0x7CCD7C,0x548B54,0xAFEEEE,0xBBFFFF,0xAEEEEE,0x96CDCD,0x668B8B,0xDB7093,0xFF82AB,
  0xEE799F,0xCD6889,0x8B475D,0xFFEFD5,0xFFEFD5,0xFFDAB9,0xFFDAB9,0xFFDAB9,0xEECBAD,0xCDAF95,0x8B7765,0xCD853F,0xFFC0CB,0xFFB5C5,0xEEA9B8,0xCD919E,
  0x8B636C,0xDDA0DD,0xFFBBFF,0xEEAEEE,0xCD96CD,0x8B668B,0xB0E0E6,0xB0E0E6,0xA020F0,0x9B30FF,0x912CEE,0x7D26CD,0x551A8B,0x663399,0x663399,0xFF0000,
  0xFF0000,0xEE0000,0xCD0000,0x8B0000,0xBC8F8F,0xBC8F8F,0xFFC1C1,0xEEB4B4,0xCD9B9B,0x8B6969,0x4169E1,0x4169E1,0x4876FF,0x436EEE,0x3A5FCD,0x27408B,
  0x8B4513,0x8B4513,0xFA8072,0xFF8C69,0xEE8262,0xCD7054,0x8B4C39,0xF4A460,0xF4A460,0x2E8B57,0x2E8B57,0x54FF9F,0x4EEE94,0x43CD80,0x2E8B57,0xFFF5EE,
  0xFFF5EE,0xEEE5DE,0xCDC5BF,0x8B8682,0xA0522D,0xFF8247,0xEE7942,0xCD6839,0x8B4726,0xC0C0C0,0x87CEEB,0x87CEEB,0x87CEFF,0x7EC0EE,0x6CA6CD,0x4A708B,
  0x6A5ACD,0x708090,0x708090,0x6A5ACD,0x836FFF,0x7A67EE,0x6959CD,0x473C8B,0x708090,0xC6E2FF,0xB9D3EE,0x9FB6CD,0x6C7B8B,0x708090,0xFFFAFA,0xFFFAFA,
  0xEEE9E9,0xCDC9C9,0x8B8989,0x00FF7F,0x00FF7F,0x00FF7F,0x00EE76,0x00CD66,0x008B45,0x4682B4,0x4682B4,0x63B8FF,0x5CACEE,0x4F94CD,0x36648B,0xD2B48C,
  0xFFA54F,0xEE9A49,0xCD853F,0x8B5A2B,0x008080,0xD8BFD8,0xFFE1FF,0xEED2EE,0xCDB5CD,0x8B7B8B,0xFF6347,0xFF6347,0xEE5C42,0xCD4F39,0x8B3626,0x40E0D0,
  0x00F5FF,0x00E5EE,0x00C5CD,0x00868B,0xEE82EE,0xD02090,0xD02090,0xFF3E96,0xEE3A8C,0xCD3278,0x8B2252,0x808080,0x008000,0x808080,0x800000,0x800080,
  0x808080,0x008000,0x808080,0x800000,0x800080,0xF5DEB3,0xFFE7BA,0xEED8AE,0xCDBA96,0x8B7E66,0xFFFFFF,0xF5F5F5,0xF5F5F5,0xBEBEBE,0x00FF00,0xBEBEBE,
  0xB03060,0xA020F0,0xBEBEBE,0x00FF00,0xBEBEBE,0xB03060,0xA020F0,0xFFFF00,0x9ACD32,0xFFFF00,0xEEEE00,0xCDCD00,0x8B8B00,0x9ACD32,
};

void octo_json_get_str(octo_str*s,octo_str*dest){
  dest->pos=0;
  octo_str_match(s,"\"");
  while(!octo_str_match(s,"\"")){
    octo_str_append(dest,
      octo_str_match(s,"\\\"")?'"':
      octo_str_match(s,"\\\\")?'\\':
      octo_str_match(s,"\\/")?'/':
      octo_str_match(s,"\\b")?'\b':
      octo_str_match(s,"\\f")?'\f':
      octo_str_match(s,"\\n")?'\n':
      octo_str_match(s,"\\r")?'\r':
      octo_str_match(s,"\\t")?'\t':
      octo_str_match(s,"\\u")?(s->pos+=6,'@'): // todo: support \uXXXX and utf-8 in general
      octo_str_next(s)
    );
  }
  octo_str_append(dest,'\0');
}
int octo_json_get_int(octo_str*s){
  octo_str_match(s,"\""); // discard quotes for numbers-as-strings.
  int neg=octo_str_match(s,"-"), val=0;
  while(isdigit(octo_str_peek(s))) val=(10*val)+(octo_str_next(s)-'0');
  octo_str_match(s,"\""); // discard quotes for numbers-as-strings.  
  return neg?-val:val;
}
int octo_json_get_bool(octo_str*s){
  return octo_str_match(s,"true")?1: octo_str_match(s,"false")?0: octo_json_get_int(s)!=0;
}
int octo_json_get_color(octo_str*s,octo_str*temp){
  octo_json_get_str(s,temp);
  for(int z=0;z<temp->pos;z++)temp->root[z]=tolower(temp->root[z]);
  temp->pos=0;
  if(octo_str_peek(temp)=='#'&&strlen(temp->root)==7){return strtol(temp->root+1,NULL,16);}
  int named_colors=sizeof(octo_css_color_names)/sizeof(char*);
  for(int z=0;z<named_colors;z++){
    if(strcmp(octo_css_color_names[z],temp->root)==0)return octo_css_color_values[z];
  }
  printf("unhandled css color: '%s'\n",temp->root);
  return 0;
}

char* octo_cart_parse_json(octo_str*s,octo_options*o){
  s->pos=0;
  octo_str source, key, val;
  octo_str_init(&source), octo_str_init(&key), octo_str_init(&val);
  source.root[0]='\0';
  if(octo_str_next(s)!='{')goto cleanup;
  while(octo_str_peek(s)=='"'){
    octo_json_get_str(s,&key);
    if(octo_str_next(s)!=':')break;
    if(strcmp(key.root,"program")==0){octo_json_get_str(s,&source);}
    else if(strcmp(key.root,"options")==0){
      if(octo_str_next(s)!='{')break;
      while(octo_str_peek(s)=='"'){
        octo_json_get_str(s,&key);
        if(octo_str_next(s)!=':')goto cleanup;
        if     (strcmp(key.root,"tickrate"       )==0){int t=octo_json_get_int(s);o->tickrate=CLAMP(1,t,50000);}
        else if(strcmp(key.root,"fillColor"      )==0)o->colors[1]=octo_json_get_color(s,&val);
        else if(strcmp(key.root,"fillColor2"     )==0)o->colors[2]=octo_json_get_color(s,&val);
        else if(strcmp(key.root,"blendColor"     )==0)o->colors[3]=octo_json_get_color(s,&val);
        else if(strcmp(key.root,"backgroundColor")==0)o->colors[0]=octo_json_get_color(s,&val);
        else if(strcmp(key.root,"buzzColor"      )==0)o->colors[5]=octo_json_get_color(s,&val);
        else if(strcmp(key.root,"quietColor"     )==0)o->colors[4]=octo_json_get_color(s,&val);
        else if(strcmp(key.root,"shiftQuirks"    )==0)o->q_shift  =octo_json_get_bool(s);
        else if(strcmp(key.root,"loadStoreQuirks")==0)o->q_loadstore=octo_json_get_bool(s);
        else if(strcmp(key.root,"vfOrderQuirks"  )==0)octo_json_get_bool(s);/*deprecated*/
        else if(strcmp(key.root,"clipQuirks"     )==0)o->q_clip   =octo_json_get_bool(s);
        else if(strcmp(key.root,"vBlankQuirks"   )==0)o->q_vblank =octo_json_get_bool(s);
        else if(strcmp(key.root,"jumpQuirks"     )==0)o->q_jump0  =octo_json_get_bool(s);
        else if(strcmp(key.root,"logicQuirks"    )==0)o->q_logic  =octo_json_get_bool(s);
        else if(strcmp(key.root,"screenRotation" )==0){
          o->rotation=octo_json_get_int(s);
          if(o->rotation!=90&&o->rotation!=180&&o->rotation!=270)o->rotation=0;
        }
        else if(strcmp(key.root,"maxSize")==0){
          o->max_rom=octo_json_get_int(s);
          if(o->max_rom!=3216&&o->max_rom!=3583&&o->max_rom!=3584)o->max_rom=65024;
        }
        else if(strcmp(key.root,"touchInputMode")==0){
          octo_json_get_str(s,&val);
          o->touch_mode=0;
          for(int z=0;z<(int)(sizeof(octo_touch_modes)/sizeof(char*));z++)if(strcmp(val.root,octo_touch_modes[z])==0)o->touch_mode=z;
        }
        else if(strcmp(key.root,"fontStyle")==0){
          o->font=0;
          for(int z=0;z<(int)(sizeof(octo_font_styles)/sizeof(char*));z++)if(strcmp(val.root,octo_font_styles[z])==0)o->font=z;
        }
        else if(octo_str_match(s,"null")){} // ignore null values
        else if(octo_str_peek(s)=='"'){octo_json_get_str(s,&val);} // ignore unknown strings
        else {octo_json_get_int(s);} // ignore unknown numbers?
        if(octo_str_next(s)!=',')break;
      }
    }
    else if(octo_str_match(s,"null")){} // ignore null values
    else if(octo_str_peek(s)=='"'){octo_json_get_str(s,&val);} // ignore unknown strings
    else {octo_json_get_int(s);} // ignore unknown numbers?
    if(octo_str_next(s)!=',')break;
  }
  cleanup:
  free(key.root),free(val.root);
  return source.root;
}

/**
*
* GIF decoding and encoding
*
**/

typedef struct {
  char* data;
  int palette[256]; // local color table
  int colors;
} octo_gif_frame;

typedef struct {
  octo_list frames; // [octo_gif_frame]
  int palette[256]; // global color table
  int colors;
  uint16_t width;
  uint16_t height;
} octo_gif;

void octo_gif_frame_destroy(octo_gif_frame*x){
  free(x->data),free(x);
}
void octo_gif_destroy(octo_gif*x){
  octo_list_destroy(&x->frames,OCTO_DESTRUCTOR(octo_gif_frame_destroy)),free(x);
}

void octo_lzw_decode(int min_code,octo_str*source,octo_str*dest){
  int prefix[4096], suffix[4096], code[4096];
  int clear=1<<min_code, size=min_code+1, mask=(1<<size)-1, next=clear+2, old=-1;
  int first, i=0,b=0,d=0;
  for(int z=0;z<clear;z++)suffix[z]=z;
  while(i<source->pos){
    while(b<size)d+=(0xFF&source->root[i++])<<b, b+=8;
    int t=d&mask; d>>=size, b-=size;
    if(t>next||t==clear+1)break;
    if(t==clear){size=min_code+1, mask=(1<<size)-1, next=clear+2, old=-1;}
    else if (old==-1) octo_str_append(dest,suffix[old=first=t]);
    else{
      int ci=0, tt=t;
      if    (t==next) code[ci++]=first,     t=old;
      while (t>clear){code[ci++]=suffix[t], t=prefix[t]; if(ci>=4096)printf("overflowed code chunk!\n");}
      octo_str_append(dest,first=suffix[t]);
      while(ci>0)octo_str_append(dest,code[--ci]);
      if(next<4096){
        prefix[next]=old, suffix[next++]=first;
        if((next&mask)==0&&next<4096)size++, mask+=next;
      }
      old=tt;
    }
  }
}

octo_gif* octo_gif_decode(octo_str*b){
  int length=b->pos; b->pos=0;
  octo_gif*g=calloc(1,sizeof(octo_gif));
  octo_list_init(&g->frames);
  octo_str compressed;
  octo_str_init(&compressed);
  if(!octo_str_match(b,"GIF89a"))octo_str_match(b,"GIF87a");
  g->width =octo_str_nextshort(b);
  g->height=octo_str_nextshort(b);
  uint8_t packed=octo_str_next(b);
  octo_str_nextshort(b); // ignore: background color index, pixel aspect ratio
  if(packed&0x80){// global color table?
    g->colors=1<<((packed&0x07)+1);
    for(int z=0;z<g->colors;z++){
      int cr=octo_str_next(b),cg=octo_str_next(b),cb=octo_str_next(b);
      g->palette[z]=(cr<<16)|(cg<<8)|cb;
    }
  }
  while(b->pos<length){
    uint8_t type=octo_str_next(b);
    if(type==0x3B)break; // end
    if(type==0x21){ // text, gce, comment, app...?
      octo_str_next(b); // ignore extension type
      while(1){uint8_t s=octo_str_next(b);if(!s)break;b->pos+=s;}
    }
    if(type==0x2C){ // image descriptor
      octo_gif_frame*frame=calloc(1,sizeof(octo_gif_frame));
      octo_list_append(&g->frames,frame);
      uint16_t xo=octo_str_nextshort(b);
      uint16_t yo=octo_str_nextshort(b);
      uint16_t iw=octo_str_nextshort(b);
      uint16_t ih=octo_str_nextshort(b);
      uint8_t  packed=octo_str_next(b);
      if(packed&0x80){// local color table?
        frame->colors=1<<((packed&0x07)+1);
        for(int z=0;z<frame->colors;z++){
          uint8_t cr=octo_str_next(b),cg=octo_str_next(b),cb=octo_str_next(b);
          frame->palette[z]=(cr<<16)|(cg<<8)|cb;
        }
      }
      uint8_t min_code=octo_str_next(b);
      compressed.pos=0;
      while(1){uint8_t s=octo_str_next(b);if(!s)break;for(int z=0;z<s;z++)octo_str_append(&compressed,octo_str_next(b));}
      octo_str decompressed;
      octo_str_init(&decompressed);
      octo_lzw_decode(min_code,&compressed,&decompressed);
      if(iw!=g->width||ih!=g->height||xo!=0||yo!=0){
        int size=g->width*g->height;
        frame->data=calloc(size,1);
        if(g->frames.count>=2){
          octo_gif_frame*prev=octo_list_get(&g->frames,g->frames.count-2);
          memcpy(frame->data,prev->data,size);
        }
        for(int y=0;y<ih;y++)for(int x=0;x<iw;x++){
          int px=x+xo, py=y+yo;
          if(px>=0&&py>=0&&px<g->width&&py<g->height)frame->data[px+(g->width*py)]=decompressed.root[x+(iw*y)];
        }
        octo_str_destroy(&decompressed);
      }
      else{
        frame->data=decompressed.root;
      }
    }
  }
  octo_str_destroy(&compressed);
  return g;
}

void octo_gif_encode(octo_str*b,octo_gif*g){
  b->pos=0;
  int z=ceil(log(g->colors)/log(2)); // bits for colortable
  octo_str_join(b,"GIF89a");
  octo_str_short(b,g->width);
  octo_str_short(b,g->height);
  octo_str_append(b,0xF0|(z-1)); // global colortable, 8-bits per channel, 2^z colors
  octo_str_append(b,0);          // background color index
  octo_str_append(b,0);          // 1:1 pixel aspect ratio
  for(int z=0;z<g->colors;z++)   // color table entries:
    octo_str_append(b,0xFF&(g->palette[z]>>16)), // R
    octo_str_append(b,0xFF&(g->palette[z]>> 8)), // G
    octo_str_append(b,0xFF&(g->palette[z]    )); // B
  for(int z=0;z<g->frames.count;z++){
    octo_str_short(b,0xF921); // graphic control extension
    octo_str_append(b,4);     // payload size
    octo_str_append(b,4);     // do not dispose frame
    octo_str_short(b,1);      // delay: 1/100th of a second
    octo_str_append(b,0);     // no transparent color
    octo_str_append(b,0);     // terminator
    octo_str_append(b,0x2C);  // image descriptor
    octo_str_short(b,0);      // x offset
    octo_str_short(b,0);      // y offset
    octo_str_short(b,g->width);
    octo_str_short(b,g->height);
    octo_str_append(b,0);     // no local colortable
    octo_str_append(b,7);     // minimum LZW code size
    octo_gif_frame*frame=octo_list_get(&g->frames,z);
    int size=g->width*g->height;
    for(int off=0;off<size;off+=64){ // assume size is divisible by 64(!)
      octo_str_append(b,1+64); // block size
      octo_str_append(b,0x80); // LZW CLEAR
      for(int i=off;i<MIN(off+64,size);i++)octo_str_append(b,frame->data[i]);
    }
    octo_str_append(b,0); // end of frame
  }
  octo_str_append(b,0x3B); // end of GIF
}

/**
*
* Octocart encoding and decoding
*
**/

char octo_cart_label_font[]={ // 8x5 pixels, A-Z0-9.-
  0x3F, 0x50, 0x90, 0x50, 0x3F, 0x00, 0xFF, 0x91, 0x91, 0x91, 0x6E, 0x00, 0x7E, 0x81, 0x81,
  0x81, 0x42, 0x00, 0xFF, 0x81, 0x81, 0x81, 0x7E, 0x00, 0xFF, 0x91, 0x91, 0x81, 0x81, 0x00,
  0xFF, 0x90, 0x90, 0x80, 0x80, 0x00, 0x7E, 0x81, 0x91, 0x91, 0x9E, 0x00, 0xFF, 0x10, 0x10,
  0x10, 0xFF, 0x00, 0x81, 0x81, 0xFF, 0x81, 0x81, 0x00, 0x02, 0x81, 0x81, 0xFE, 0x80, 0x00,
  0xFF, 0x10, 0x20, 0x50, 0x8F, 0x00, 0xFF, 0x01, 0x01, 0x01, 0x01, 0x00, 0xFF, 0x40, 0x20,
  0x40, 0xFF, 0x00, 0xFF, 0x40, 0x20, 0x10, 0xFF, 0x00, 0x7E, 0x81, 0x81, 0x81, 0x7E, 0x00,
  0xFF, 0x90, 0x90, 0x90, 0x60, 0x00, 0x7E, 0x81, 0x85, 0x82, 0x7D, 0x00, 0xFF, 0x90, 0x90,
  0x98, 0x67, 0x00, 0x62, 0x91, 0x91, 0x91, 0x4E, 0x00, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x00,
  0xFE, 0x01, 0x01, 0x01, 0xFE, 0x00, 0xFC, 0x02, 0x01, 0x02, 0xFC, 0x00, 0xFF, 0x02, 0x04,
  0x02, 0xFF, 0x00, 0xC7, 0x28, 0x10, 0x28, 0xC7, 0x00, 0xC0, 0x20, 0x1F, 0x20, 0xC0, 0x00,
  0x87, 0x89, 0x91, 0xA1, 0xC1, 0x00, 0x7E, 0x81, 0x99, 0x81, 0x7E, 0x00, 0x21, 0x41, 0xFF,
  0x01, 0x01, 0x00, 0x43, 0x85, 0x89, 0x91, 0x61, 0x00, 0x82, 0x81, 0xA1, 0xD1, 0x8E, 0x00,
  0xF0, 0x10, 0x10, 0xFF, 0x10, 0x00, 0xF2, 0x91, 0x91, 0x91, 0x9E, 0x00, 0x7E, 0x91, 0x91,
  0x91, 0x4E, 0x00, 0x80, 0x90, 0x9F, 0xB0, 0xD0, 0x00, 0x6E, 0x91, 0x91, 0x91, 0x6E, 0x00,
  0x62, 0x91, 0x91, 0x91, 0x7E, 0x00, 0x00, 0x00, 0x06, 0x06, 0x00, 0x00, 0x00, 0x10, 0x10,
  0x10, 0x10, 0x00,
};
char octo_cart_base_image[]={
  0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0xA0, 0x00, 0x80, 0x00, 0xA2, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x66, 0x50, 0xBF, 0xBE, 0xA6, 0xF6, 0xE3, 0x9F, 0xF6, 0xEA, 0xCF, 0xFF, 0xFF,
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xF9, 0x04, 0x09, 0x00, 0x00, 0x06, 0x00,
  0x2C, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x00, 0x80, 0x00, 0x00, 0x03, 0xFF, 0x08, 0x1A, 0xDC,
  0xFE, 0x30, 0xCA, 0x49, 0xAB, 0xBD, 0x38, 0x6B, 0xAC, 0xBA, 0x0F, 0x42, 0x28, 0x8E, 0x64,
  0x69, 0x9E, 0x68, 0xAA, 0xAE, 0x6C, 0xEB, 0xBE, 0x6D, 0xE0, 0x75, 0xA0, 0xD0, 0x91, 0xB7,
  0x3D, 0x87, 0xF9, 0xEC, 0xFF, 0xC0, 0xA0, 0x70, 0x48, 0x2C, 0x1A, 0x8F, 0xC4, 0x90, 0xEC,
  0xC3, 0x6B, 0x00, 0x44, 0x00, 0xA7, 0x00, 0x62, 0x93, 0xC2, 0xAE, 0xD8, 0xAC, 0x76, 0x7B,
  0x5A, 0x2E, 0xAA, 0x20, 0xC6, 0x33, 0x1A, 0xAE, 0x4D, 0x6B, 0xE4, 0xF3, 0x18, 0xC9, 0x6E,
  0xBB, 0xDF, 0xF0, 0xA1, 0x92, 0x76, 0x16, 0x31, 0xEA, 0xA5, 0x3B, 0x7E, 0xCF, 0xED, 0xFB,
  0xFF, 0x7F, 0x4B, 0x65, 0x63, 0x77, 0x62, 0x3A, 0x76, 0x35, 0x86, 0x69, 0x71, 0x8C, 0x8D,
  0x8E, 0x8F, 0x3B, 0x53, 0x51, 0x4A, 0x32, 0x7A, 0x6A, 0x96, 0x98, 0x95, 0x66, 0x80, 0x9C,
  0x9D, 0x9E, 0x2C, 0x99, 0x9B, 0x67, 0x9B, 0x96, 0xA3, 0x3C, 0x90, 0xA8, 0xA9, 0xAA, 0x45,
  0x7C, 0x9F, 0xAE, 0xAF, 0xB0, 0x7D, 0xA5, 0xB1, 0xB4, 0xB5, 0xB6, 0x5D, 0xA2, 0xB7, 0xBA,
  0xBB, 0xAF, 0xB3, 0xBC, 0xBF, 0xC0, 0xB2, 0xB9, 0xC1, 0xC4, 0xC5, 0x31, 0xC3, 0xC6, 0xC9,
  0xCA, 0x23, 0xBE, 0xCB, 0xCE, 0xC9, 0xCD, 0xAB, 0xD2, 0xD3, 0xD4, 0x46, 0x26, 0xBE, 0x64,
  0x1B, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0x86, 0x24, 0xBE, 0xE0, 0xE4, 0xE5, 0xE6, 0xE7,
  0x1A, 0x79, 0xB9, 0x0F, 0x03, 0xED, 0xEE, 0x03, 0x0C, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
  0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xEE, 0x04, 0xFE, 0xFC, 0xF4, 0x1E, 0xA8, 0x53, 0x17, 0xAF,
  0x5E, 0x00, 0x80, 0x08, 0x13, 0x2A, 0x5C, 0x98, 0xCF, 0xDF, 0x3F, 0x86, 0x0E, 0x06, 0x8A,
  0x2B, 0xC8, 0xB0, 0xA2, 0xC5, 0x8B, 0x0C, 0x1D, 0x3E, 0x5C, 0xD8, 0x40, 0xFF, 0x22, 0xB3,
  0x83, 0x18, 0x43, 0x8A, 0x1C, 0x59, 0x4F, 0xA3, 0xC5, 0x71, 0xEB, 0x40, 0x92, 0x5C, 0xC9,
  0xF2, 0xA2, 0xC3, 0x93, 0x29, 0xD5, 0xB5, 0x9C, 0x49, 0x33, 0xE1, 0x4B, 0x88, 0x31, 0x27,
  0xD6, 0xDC, 0xC9, 0xF3, 0xDE, 0x4D, 0x85, 0x28, 0x65, 0xF6, 0x1C, 0x4A, 0x74, 0xC0, 0x4F,
  0x84, 0x41, 0x75, 0x16, 0x5D, 0xCA, 0xF3, 0xE8, 0xBE, 0xA4, 0x1F, 0x99, 0x4A, 0x9D, 0x5A,
  0x92, 0x00, 0x54, 0x44, 0x54, 0xB3, 0x36, 0xD5, 0xB8, 0xD1, 0xDF, 0x55, 0x4A, 0x5A, 0xC3,
  0xB6, 0xE4, 0xEA, 0xF4, 0xEB, 0x19, 0xB1, 0x68, 0x57, 0x9A, 0x9C, 0x67, 0x96, 0x62, 0xDA,
  0xB7, 0x34, 0xDB, 0xAA, 0x84, 0x4B, 0x77, 0xA5, 0xDC, 0xBA, 0x78, 0x49, 0xDE, 0xCD, 0xCB,
  0xF7, 0xE2, 0xDE, 0xBE, 0x80, 0x81, 0xE6, 0x8C, 0x1A, 0xB8, 0x30, 0xC0, 0xBF, 0x86, 0x13,
  0xDF, 0x43, 0xAC, 0xB8, 0xB1, 0x3C, 0xC6, 0x8E, 0x23, 0x43, 0x8E, 0xDC, 0x78, 0x32, 0xE5,
  0xC4, 0x96, 0x2F, 0x17, 0xCE, 0xAC, 0x19, 0x30, 0xE7, 0xCE, 0x7C, 0x3F, 0x83, 0xC6, 0x2B,
  0x7A, 0x34, 0xDD, 0xD2, 0xA6, 0xDF, 0xA2, 0x4E, 0x8D, 0x76, 0x35, 0xEB, 0xB0, 0xAE, 0x5F,
  0x67, 0x8D, 0x2D, 0x7B, 0x2A, 0xED, 0xDA, 0x4C, 0x6F, 0xE3, 0x2E, 0xAA, 0x7B, 0xF7, 0xD0,
  0xDE, 0xBE, 0x79, 0x02, 0x0F, 0x5E, 0x73, 0x38, 0xF1, 0x99, 0xC6, 0x8F, 0xB3, 0x4C, 0xAE,
  0x5C, 0xEF, 0x60, 0xAC, 0xCD, 0xD3, 0x32, 0x8F, 0x1E, 0x72, 0x3A, 0x75, 0xBF, 0xCF, 0xC1,
  0x5E, 0x87, 0x9D, 0xFD, 0xEC, 0x76, 0xAD, 0xD6, 0xBF, 0x73, 0xEC, 0xEE, 0x56, 0x7C, 0x6E,
  0xF2, 0x73, 0xCD, 0x2F, 0x0D, 0xAF, 0xFE, 0x30, 0xFA, 0xF6, 0x52, 0xD9, 0xC3, 0xD7, 0x27,
  0x7F, 0x3E, 0xBE, 0xFA, 0xF6, 0xED, 0xE1, 0xCF, 0x1F, 0xF0, 0x3D, 0xFF, 0x9E, 0xFF, 0xFB,
  0xFD, 0xF7, 0x4E, 0x80, 0x02, 0xB6, 0x43, 0x60, 0x81, 0x8C, 0xAD, 0x55, 0x20, 0x46, 0x09,
  0x3A, 0xB5, 0x20, 0x4E, 0x1E, 0x41, 0xD7, 0x8F, 0x83, 0x0F, 0x0A, 0x16, 0xA1, 0x76, 0xEF,
  0x10, 0x50, 0x61, 0x75, 0xFE, 0xD9, 0xB3, 0xD1, 0x86, 0xEE, 0x5D, 0xE8, 0x9D, 0x87, 0x1A,
  0x82, 0x88, 0x14, 0x7A, 0xE9, 0x35, 0x97, 0xE2, 0x6F, 0x72, 0x95, 0x77, 0x9C, 0x8B, 0x00,
  0x76, 0x34, 0x11, 0x41, 0xE5, 0xAD, 0x68, 0x9A, 0x03, 0xE7, 0xC9, 0xF8, 0x11, 0x8D, 0xE8,
  0xF4, 0xE8, 0xE3, 0x8F, 0xDC, 0x88, 0x38, 0x0A, 0x90, 0x44, 0x16, 0x69, 0xA4, 0x8E, 0x3B,
  0x3E, 0xA3, 0xA4, 0x32, 0xCD, 0x2C, 0xE9, 0xA4, 0x2E, 0x4D, 0x3E, 0x29, 0x65, 0x2C, 0x51,
  0x4E, 0x69, 0xA5, 0x27, 0x55, 0x5E, 0xA9, 0xA5, 0x1F, 0x59, 0x6E, 0xE9, 0x65, 0x16, 0x5D,
  0x7E, 0x29, 0xA6, 0x0B, 0x61, 0x8E, 0x69, 0x66, 0x0A, 0x65, 0x9E, 0xA9, 0xE6, 0x8C, 0x6B,
  0xB6, 0x79, 0x8C, 0x9B, 0x70, 0xAA, 0x90, 0x66, 0x9C, 0x66, 0xCE, 0x49, 0xA7, 0x98, 0x76,
  0xDE, 0xE9, 0x65, 0x9E, 0x7A, 0x6A, 0xC9, 0x67, 0x9F, 0x56, 0xFE, 0x09, 0xA8, 0x94, 0x82,
  0x0E, 0xEA, 0x64, 0xA1, 0x86, 0x2A, 0x89, 0x68, 0xA2, 0xCE, 0x2C, 0xCA, 0x28, 0x93, 0xC8,
  0x3C, 0xFA, 0xA5, 0xA3, 0x92, 0x16, 0x43, 0x69, 0xA5, 0xC1, 0x5C, 0x8A, 0xE9, 0x2F, 0x9A,
  0x6E, 0xBA, 0x4B, 0xA7, 0x9E, 0xDE, 0x02, 0x6A, 0xA8, 0xB5, 0x8C, 0x4A, 0x2A, 0x95, 0x91,
  0x9E, 0x7A, 0x68, 0xAA, 0x7B, 0x4E, 0x00, 0xA7, 0xA9, 0xBB, 0x3C, 0x81, 0x82, 0xAC, 0x6B,
  0xC2, 0x1A, 0x2B, 0xAD, 0x50, 0xE0, 0xAA, 0xA6, 0xAD, 0xBC, 0x28, 0x70, 0x0A, 0x9D, 0xBC,
  0xFE, 0xE2, 0xEB, 0x9D, 0x4B, 0xA8, 0xAA, 0x65, 0x24, 0xC6, 0x3E, 0x59, 0xCD, 0xB2, 0x05,
  0xCC, 0x36, 0xCB, 0x46, 0x02, 0x00, 0x3B, 
};

#include <sys/stat.h>

uint8_t octo_cart_byte(octo_gif*g,int*offset) {
  int size=g->width*g->height, i=(*offset)%size;
  octo_gif_frame*f=octo_list_get(&g->frames,(*offset)/size);
  int* pal=f->colors?f->palette:g->palette;
  int ca=pal[0xFF&f->data[i]], cb=pal[0xFF&f->data[i+1]];
  uint8_t a=((ca>>13)&8)|((ca>>7)&6)|(ca&1);
  uint8_t b=((cb>>13)&8)|((cb>>7)&6)|(cb&1);
  return (*offset)+=2, (a<<4)|b;
}

char* octo_cart_load(const char*filename,octo_options*o){
  octo_str source;
  {
    struct stat st;
    if(stat(filename,&st)!=0)return NULL;
    source.pos=source.size=st.st_size;
    source.root=malloc(source.pos);
    FILE*source_file=fopen(filename,"rb");
    fread(source.root,sizeof(char),source.pos,source_file);
    fclose(source_file);
  }
  octo_gif* g=octo_gif_decode(&source);
  octo_str json;
  octo_str_init(&json);
  int offset=0, size=0;
  for(int z=0;z<4;z++) size=(size<<8)|(0xFF&octo_cart_byte(g,&offset));
  for(int z=0;z<size;z++) octo_str_append(&json,octo_cart_byte(g,&offset));
  octo_str_append(&json,'\0');
  char* program=octo_cart_parse_json(&json,o);
  octo_str_destroy(&json);
  octo_gif_destroy(g);
  return program;
}

void octo_cart_save(FILE*dest,char*program,octo_options*o,char*label_pix,char*label_text){
  octo_str base_data;
  base_data.pos=base_data.size=sizeof(octo_cart_base_image);
  base_data.root=octo_cart_base_image;
  octo_gif* base=octo_gif_decode(&base_data);
  octo_gif_frame*base_frame=octo_list_get(&base->frames,0);
  if(label_pix!=NULL){
    // labels are {0,1,2?} arrays of 128x64 pixels,
    // to be placed at (16,21) and mapped to colors {3,1,4}:
    for(int y=0;y<64;y++)for(int x=0;x<128;x++){
      int c=label_pix[x+(y*128)];
      base_frame->data[16+x+((21+y)*base->width)]=(c==0?3: c==1?1: 4);
    }
  }
  if(label_text!=NULL){
    // a whimsically impressionistic imitation
    // of a crummy dot-matrix printer:
    char* alpha="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
    int cursorx=16, cursory=32;
    for(size_t i=0;i<strlen(label_text);i++){
      char c=toupper(label_text[i]), a=strlen(alpha)-1;
      for(int z=0;z<a;z++)if(alpha[z]==c){a=z;break;}
      if(c==' ')cursorx+=6;
      else if(c=='\n')cursorx=16,cursory+=9;
      else{
        for(int x=0;x<6;x++)for(int y=0;y<8;y++){
          if(x+cursorx>base->width-16)                    continue;
          if(y+cursory>base->height)                      continue;
          if((rand()%100)>95)                             continue;
          if(!((octo_cart_label_font[(a*6)+x]>>(7-y))&1)) continue;
          base_frame->data[(x+cursorx)+ base->width*(y+cursory)]=1;
        }
        cursorx+=6;
      }
      cursorx+=(rand()%10)>8?1:0;
      cursory+=(rand()%10)>8?1:0;
    }
  }
  octo_str json;
  octo_str_init(&json);
  json.pos+=4; // reserve space for size bytes
  octo_cart_format_json(&json,program,o);
  json.pos-=1; // don't write out the null terminator!
  json.root[0]=((json.pos-4)>>24)&0xFF;
  json.root[1]=((json.pos-4)>>16)&0xFF;
  json.root[2]=((json.pos-4)>> 8)&0xFF;
  json.root[3]=((json.pos-4)    )&0xFF;
  octo_gif* cart=malloc(sizeof(octo_gif));
  octo_list_init(&cart->frames);
  cart->width =base->width;
  cart->height=base->height;
  cart->colors=base->colors*16;
  for(int c=0;c<base->colors;c++){
    // use 1 bit from the red/blue channels and 2 from the green channel to store data:
    for(int x=0;x<16;x++)cart->palette[(16*c)+x]=(base->palette[c]&0xFEFCFE)|((x&0x8)<<13)|((x&0x6)<<7)|(x&1);
  }
  int frame_size=base->width*base->height, frame_count=ceil((json.pos*2.0)/frame_size);
  for(int z=0;z<frame_count;z++){
    octo_gif_frame*frame=calloc(1,sizeof(octo_gif_frame));
    octo_list_append(&cart->frames,frame);
    frame->data=malloc(frame_size);
    for(int i=0;i<frame_size;i++){
      int src=(i+frame_size*z)/2;                              // every byte in the payload becomes 2 pixels
      int nyb=src>=json.pos?0: (json.root[src]>>(i%2==0?4:0)); // alternate high, low nybbles
      frame->data[i]=(base_frame->data[i]*16)+(nyb&0xF);       // multiply out colors and mix in data
    }
  }
  octo_str file;
  octo_str_init(&file);
  octo_gif_encode(&file,cart);
  fwrite(file.root,sizeof(char),file.pos,dest);
  octo_gif_destroy(base);
  octo_gif_destroy(cart);
  octo_str_destroy(&json);
  octo_str_destroy(&file);
}
