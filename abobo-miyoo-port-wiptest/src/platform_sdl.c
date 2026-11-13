/* SDL2 front end for the Miyoo Mini Plus (Onion OS). NOT YET TESTED ON DEVICE. */
#include "rt.h"
#ifdef USE_SDL1
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif
#include <time.h>
#define SW 256
#define SH 254
static char datadir[512];
double now_ms(void){ return (double)SDL_GetTicks(); }
/* ---- audio ---- */
typedef struct { Obj *h; int chan; Mix_Chunk *ck; int music; } Voice;
static Voice voices[32]; static Mix_Chunk *cache[65536]; static Mix_Music *curmus;
static Mix_Chunk *load_chunk(int id){ if(cache[id]) return cache[id]; char p[600]; snprintf(p,600,"%s/sounds/%d.wav",datadir,id); cache[id]=Mix_LoadWAV(p); return cache[id]; }
void audio_start(int id,int loops,int vol,Obj *h){
  char p[600]; snprintf(p,600,"%s/sounds/%d.wav",datadir,id); FILE *f=fopen(p,"rb"); if(!f) return; fseek(f,0,SEEK_END); long sz=ftell(f); fclose(f);
  int mloops= loops>=9999?-1:(loops<1?0:loops-1);
  if(sz>600000){ /* long: stream as music */ if(curmus){ Mix_HaltMusic(); Mix_FreeMusic(curmus); } curmus=Mix_LoadMUS(p); if(curmus){ Mix_VolumeMusic(vol*128/100); Mix_PlayMusic(curmus,mloops<0?-1:mloops+1); for(int i=0;i<32;i++) if(!voices[i].h){ voices[i].h=h; voices[i].music=1; break; } } return; }
  Mix_Chunk *c=load_chunk(id); if(!c) return; int ch=Mix_PlayChannel(-1,c,mloops); if(ch>=0){ Mix_Volume(ch,vol*128/100); for(int i=0;i<32;i++) if(!voices[i].h||voices[i].chan==ch){ voices[i].h=h; voices[i].chan=ch; voices[i].music=0; break; } }
}
void audio_stop(Obj *h){ for(int i=0;i<32;i++) if(voices[i].h==h){ if(voices[i].music) Mix_HaltMusic(); else Mix_HaltChannel(voices[i].chan); voices[i].h=NULL; } }
void audio_stopall(void){ Mix_HaltChannel(-1); Mix_HaltMusic(); for(int i=0;i<32;i++) voices[i].h=NULL; }
void audio_setvol(Obj *h,int vol){ for(int i=0;i<32;i++) if(voices[i].h==h){ if(voices[i].music) Mix_VolumeMusic(vol*128/100); else Mix_Volume(voices[i].chan,vol*128/100); } }
/* ---- input: Miyoo Mini keyboard mapping (Onion) -> Flash key codes ----
   D-pad = arrows, A = 'A'(65), B = 'S'(83), X = SPACE, Y = 'D'(68), Start = 'P'(80, pause), Select = 'M'(77, mute)
   Hold R1 for POINTER MODE: D-pad moves an on-screen pointer, A/Start click. L1 = Enter(13). */
static int pointer_mode; static double px=128,py=127; static int click_down;
static void set_key(int code,int down){ if(code>0&&code<256) keys_down[code]=down; }
static void handle_key(int k,int down){
  switch(k){
    case SDLK_UP: if(pointer_mode){ if(down) py-=6; } else set_key(38,down); break;
    case SDLK_DOWN: if(pointer_mode){ if(down) py+=6; } else set_key(40,down); break;
    case SDLK_LEFT: if(pointer_mode){ if(down) px-=6; } else set_key(37,down); break;
    case SDLK_RIGHT: if(pointer_mode){ if(down) px+=6; } else set_key(39,down); break;
    case SDLK_SPACE: /* A */ if(pointer_mode||0){ if(down&&!click_down){ click_down=1; input_mouse(1,px,py);} if(!down&&click_down){ click_down=0; input_mouse(2,px,py);} } set_key(65,down); break;
    case SDLK_LCTRL: /* B */ set_key(83,down); break;
    case SDLK_LSHIFT: /* X */ set_key(32,down); break;
    case SDLK_LALT: /* Y */ set_key(68,down); break;
    case SDLK_RETURN: /* Start: pause + click at pointer/centre */ set_key(80,down); if(down){ input_mouse(1,pointer_mode?px:128,pointer_mode?py:127);} else input_mouse(2,pointer_mode?px:128,pointer_mode?py:127); break;
    case SDLK_RCTRL: /* Select */ set_key(77,down); break;
    case SDLK_e: /* L1 */ set_key(13,down); break;
    case SDLK_t: /* R1 */ pointer_mode=down; break;
    case SDLK_TAB: /* L2 */ set_key(81,down); break;
    case SDLK_BACKSPACE: /* R2 */ set_key(69,down); break;
    case SDLK_ESCAPE: /* Menu */ if(down){ SDL_Event q; q.type=SDL_QUIT; SDL_PushEvent(&q);} break;
    default: break;
  }
}
#ifdef USE_SDL1
static SDL_Surface *screen; static int xmap[484], ymap[480];
static void video_init(void){
  screen=SDL_SetVideoMode(640,480,32,SDL_SWSURFACE); if(!screen){ fprintf(stderr,"SDL_SetVideoMode: %s\n",SDL_GetError()); exit(1); }
  for(int x=0;x<484;x++) xmap[x]=x*SW/484; for(int y=0;y<480;y++) ymap[y]=y*SH/480;
  SDL_ShowCursor(SDL_DISABLE);
}
static void video_present(const uint32_t *fb){
  int swap=(screen->format->Rmask==0xff); SDL_LockSurface(screen);
  for(int y=0;y<480;y++){ uint32_t *dst=(uint32_t*)((uint8_t*)screen->pixels+y*screen->pitch)+78; const uint32_t *src=fb+ymap[y]*SW;
    if(!swap) for(int x=0;x<484;x++) dst[x]=src[xmap[x]]; else for(int x=0;x<484;x++){ uint32_t p=src[xmap[x]]; dst[x]=(p&0xff00ff00)|((p&0xff)<<16)|((p>>16)&0xff); } }
  SDL_UnlockSurface(screen); SDL_Flip(screen);
}
#else
static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex;
static void video_init(void){
  win=SDL_CreateWindow("Abobo",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,640,480,SDL_WINDOW_SHOWN);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0"); ren=SDL_CreateRenderer(win,-1,0);
  tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,SW,SH); SDL_ShowCursor(0);
}
static void video_present(const uint32_t *fb){
  SDL_UpdateTexture(tex,NULL,fb,SW*4); SDL_RenderClear(ren); SDL_Rect dst={78,0,484,480}; SDL_RenderCopy(ren,tex,NULL,&dst); SDL_RenderPresent(ren);
}
#endif
int main(int argc,char **argv){
  snprintf(datadir,512,"%s",argc>1?argv[1]:"./data");
  rt_init(); builtins_init(); scene_load(datadir); bitmaps_open(datadir);
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){ fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
  Mix_OpenAudio(22050,AUDIO_S16SYS,1,1024); Mix_AllocateChannels(24);
  video_init();
  Char *rc=chars[0]; ROOT=mc_create(rc,NULL,0,intern("_level0"),0); ROOT->isroot=1; setprop(G,intern("_root"),vobj(ROOT->obj));
  run_inits(); mc_goto(ROOT,1,1);
  static uint32_t fb[SW*SH]; int running=1; Uint32 last=SDL_GetTicks(); double acc=0; const double step=1000.0/24.0;
  int maxf=getenv("ABOBO_MAXFRAMES")?atoi(getenv("ABOBO_MAXFRAMES")):0; int nframes=0;
  while(running){
    if(maxf&&nframes>=maxf) break;
    SDL_Event e; while(SDL_PollEvent(&e)){ if(e.type==SDL_QUIT) running=0; else if(e.type==SDL_KEYDOWN
#ifndef USE_SDL1
        &&!e.key.repeat
#endif
        ) handle_key(e.key.keysym.sym,1); else if(e.type==SDL_KEYUP) handle_key(e.key.keysym.sym,0); }
    if(px<0)px=0; if(px>255)px=255; if(py<0)py=0; if(py>253)py=253; g_mouse_x=px; g_mouse_y=py;
    Uint32 now=SDL_GetTicks(); acc+=now-last; last=now; int ticks=0;
    while(acc>=step&&ticks<3){ world_tick(); acc-=step; ticks++; } if(acc>step*3) acc=0;
    if(ticks){ nframes+=ticks; render_frame(fb,SW,SH);
      if(pointer_mode){ for(int d=-4;d<=4;d++){ int x=(int)px+d,y=(int)py; if(x>=0&&x<SW) fb[y*SW+x]=0xffff00ff; x=(int)px; y=(int)py+d; if(y>=0&&y<SH) fb[y*SW+x]=0xffff00ff; } }
      video_present(fb); }
    else SDL_Delay(2);
  }
  if(getenv("ABOBO_MAXFRAMES")){ FILE *o=fopen("/tmp/sdl_last.ppm","wb"); if(o){ fprintf(o,"P6\n%d %d\n255\n",SW,SH); for(int i=0;i<SW*SH;i++){ fputc((fb[i]>>16)&255,o); fputc((fb[i]>>8)&255,o); fputc(fb[i]&255,o);} fclose(o);} }
  SDL_Quit(); return 0;
}
