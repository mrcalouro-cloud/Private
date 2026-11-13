#include "rt.h"
#include <time.h>
void bitmaps_open(const char *dir);
static double simms;
double now_ms(void){ return simms; }
void audio_start(int id,int loops,int vol,Obj *h){ if(getenv("ABOBO_DEBUG")) fprintf(stderr,"[audio] start %d\n",id); }
void audio_stop(Obj *h){} void audio_stopall(void){} void audio_setvol(Obj *h,int v){}
static void dump(const char *path,uint32_t *fb,int w,int h){ FILE *f=fopen(path,"wb"); fprintf(f,"P6\n%d %d\n255\n",w,h); for(int i=0;i<w*h;i++){ fputc((fb[i]>>16)&255,f); fputc((fb[i]>>8)&255,f); fputc(fb[i]&255,f); } fclose(f); }
void dump_tree(MC *m,int ind,int maxd);
int main(int argc,char **argv){
  const char *dir=argc>1?argv[1]:"../assets"; int frames=argc>2?atoi(argv[2]):60; int every=argc>3?atoi(argv[3]):30;
  rt_init(); builtins_init(); scene_load(dir); bitmaps_open(dir);
  Char *rc=chars[0]; ROOT=mc_create(rc,NULL,0,intern("_level0"),0); ROOT->isroot=1; setprop(G,intern("_root"),vobj(ROOT->obj));
  run_inits(); mc_goto(ROOT,1,1);
  static uint32_t fb[256*254];
  /* optional scripted input: ABOBO_KEYS="frame:key:1,..." */
  const char *inp=getenv("ABOBO_INPUT"); /* "frame:c:x:y;frame:k+:code;frame:k-:code" */
  for(int i=0;i<frames;i++){ simms=i*1000.0/24;
    if(inp){ const char *p=inp; while(*p){ int fr; char t[4]; int a=0,b=0; int n=0; if(sscanf(p,"%d:%3[^:;]%n",&fr,t,&n)<2) break; p+=n; if(*p==':'){ p++; sscanf(p,"%d%n",&a,&n); p+=n; } if(*p==':'){ p++; sscanf(p,"%d%n",&b,&n); p+=n; } if(*p==';') p++;
        if(fr==i){ if(!strcmp(t,"c")){ input_mouse(1,a,b); } else if(!strcmp(t,"cu")){ input_mouse(2,a,b); } else if(!strcmp(t,"k+")) keys_down[a&255]=1; else if(!strcmp(t,"k-")) keys_down[a&255]=0; } } }
    { const char *gt=getenv("ABOBO_GOTO"); if(gt){ int gf=atoi(gt); const char *lb=strchr(gt,':'); if(lb&&gf==i){ int fr=mc_findlabel(ROOT,lb+1); setprop(ROOT->obj,intern("realgo"),vbool(1)); mc_goto(ROOT,fr,1); } } }
    world_tick(); 
    if(getenv("ABOBO_BENCH")) render_frame(fb,256,254);
    if(every&&(i%every==0||i==frames-1)){ render_frame(fb,256,254); char p[128]; snprintf(p,128,"/tmp/frame_%04d.ppm",i); dump(p,fb,256,254); }
  }
  if(getenv("ABOBO_TREE")) dump_tree(ROOT,0,atoi(getenv("ABOBO_TREE")));
  fprintf(stderr,"done; root frame %d/%d, kids %d\n",ROOT->cur,ROOT->def->nframes,ROOT->nkids);
  return 0;
}
void dump_tree(MC *m,int ind,int maxd){
  for(int i=0;i<ind;i++) fputc(' ',stderr);
  fprintf(stderr,"%s id=%d d=%d f=%d/%d %s nca=%d",m->name?m->name:"-",m->def?m->def->id:-1,m->depth,m->cur,m->def?m->def->nframes:0,m->visible?"":"(hidden)",m->nca);
  if(m->obj) for(int i=0;i<m->obj->nprops;i++){ Prop *p=&m->obj->props[i]; if(p->v.t==T_OBJ&&p->v.u.o->kind==K_FUNC) fprintf(stderr," fn:%s",p->name); }
  fputc('\n',stderr);
  { const char *pp=getenv("ABOBO_PROPS"); if(pp&&m->def&&m->def->id==atoi(pp)&&m->obj){ for(int i=0;i<m->obj->nprops;i++){ Prop *p=&m->obj->props[i]; if(p->v.t==T_NUM||p->v.t==T_BOOL||p->v.t==T_STR) fprintf(stderr,"     .%s=%s\n",p->name,tostr(p->v)); } } }
  if(ind/2<maxd) for(int i=0;i<m->nkids;i++) dump_tree(m->kids[i],ind+2,maxd);
}
