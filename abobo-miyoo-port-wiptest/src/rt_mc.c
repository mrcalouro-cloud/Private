#include "rt.h"
#include <math.h>
Char *chars[65536]; InitAct *inits; int ninits; MC *ROOT;
static uint8_t *scene_buf; static size_t scene_len;
const char *Sname_exports[65536];
typedef struct { const char *name; Char *c; Val ctor; } Export;
static Export *exps; static int nexps;
uint8_t keys_down[256], keys_pressed[256]; int g_frame; double g_mouse_x, g_mouse_y;
/* ---- loader ---- */
static const uint8_t *rp;
static uint8_t r8(void){ return *rp++; }
static uint16_t r16(void){ uint16_t v=rp[0]|(rp[1]<<8); rp+=2; return v; }
static uint32_t r32(void){ uint32_t v=rp[0]|(rp[1]<<8)|(rp[2]<<16)|((uint32_t)rp[3]<<24); rp+=4; return v; }
static float rf(void){ union{uint32_t u;float f;}x; x.u=r32(); return x.f; }
static const char *rs(void){ int n=r16(); const char *s=intern_n((const char*)rp,n); rp+=n; return s; }
static void rmat(Mat *m){ m->a=rf(); m->b=rf(); m->c=rf(); m->d=rf(); m->tx=rf(); m->ty=rf(); }
static void load_timeline(Char *c){
  c->nframes=r16(); c->frames=calloc(c->nframes?c->nframes:1,sizeof(Frame));
  for(int i=0;i<c->nframes;i++){ Frame *f=&c->frames[i]; int n=r16(); f->cmd=calloc(n?n:1,sizeof(Cmd)); f->ncmd=n;
    for(int j=0;j<n;j++){ Cmd *cm=&f->cmd[j]; cm->type=r8();
      if(cm->type==3){ cm->alen=r32(); cm->act=rp; rp+=cm->alen; }
      else if(cm->type==4){ cm->label=rs(); if(!f->label) f->label=cm->label; }
      else if(cm->type==2){ cm->depth=r16(); }
      else if(cm->type==1){ cm->depth=r16(); cm->fl=r8(); cm->move=r8();
        cm->m=(Mat){1,0,0,1,0,0}; for(int k=0;k<4;k++){ cm->cx.m[k]=256; cm->cx.a[k]=0; }
        if(cm->fl&1) cm->ch=r16();
        if(cm->fl&2) rmat(&cm->m);
        if(cm->fl&4){ for(int k=0;k<8;k++){ int16_t v=(int16_t)r16(); if(k<4) cm->cx.m[k]=v; else cm->cx.a[k-4]=v; } }
        if(cm->fl&8) cm->name=rs();
        if(cm->fl&16) cm->clipdepth=r16();
        if(cm->fl&64) cm->cls=rs();
        if(cm->fl&32){ cm->nca=r8(); cm->ca=calloc(cm->nca,sizeof(ClipAct)); for(int k=0;k<cm->nca;k++){ cm->ca[k].flags=r32(); cm->ca[k].key=r8(); cm->ca[k].len=r32(); cm->ca[k].code=rp; rp+=cm->ca[k].len; } }
      }
    } }
}
void scene_load(const char *dir){
  char p[512]; snprintf(p,512,"%s/scene.bin",dir); FILE *fp=fopen(p,"rb"); if(!fp){ fprintf(stderr,"cannot open %s\n",p); exit(1); }
  fseek(fp,0,SEEK_END); scene_len=ftell(fp); fseek(fp,0,SEEK_SET); scene_buf=malloc(scene_len); fread(scene_buf,1,scene_len,fp); fclose(fp);
  rp=scene_buf+4; int ni=0; inits=calloc(512,sizeof(InitAct));
  while(rp<scene_buf+scene_len){ uint8_t k=r8(); if(k==0) break;
    if(k==1){ uint16_t id=r16(); Char *c=calloc(1,sizeof(Char)); c->kind=1; c->id=id; for(int i=0;i<4;i++) c->bounds[i]=rf(); c->nfills=r16(); c->fills=calloc(c->nfills?c->nfills:1,sizeof(Fill));
      for(int i=0;i<c->nfills;i++){ Fill *f=&c->fills[i]; f->type=r8(); f->bid=r16(); if(f->type==0){ for(int q=0;q<4;q++) f->rgba[q]=r8(); f->nseg=r16(); f->seg=malloc(sizeof(float)*4*(f->nseg?f->nseg:1)); for(int q=0;q<f->nseg*4;q++) f->seg[q]=rf(); } if(f->type==1){ rmat(&f->m); f->ft=r8(); f->m.a/=20; f->m.b/=20; f->m.c/=20; f->m.d/=20; } }
      chars[id]=c; }
    else if(k==2||k==6){ uint16_t id=r16(); Char *c=calloc(1,sizeof(Char)); c->kind=2; c->id=id; load_timeline(c); if(k==6) chars[0]=c; else chars[id]=c; if(k==6) c->kind=2; }
    else if(k==7){ uint16_t id=r16(); Char *c=calloc(1,sizeof(Char)); c->kind=3; c->id=id; c->nbrec=r16(); c->brec=calloc(c->nbrec?c->nbrec:1,sizeof(BRec));
      for(int i=0;i<c->nbrec;i++){ BRec *b=&c->brec[i]; b->st=r8(); b->ch=r16(); b->depth=r16(); rmat(&b->m); for(int k2=0;k2<8;k2++){ int16_t v=(int16_t)r16(); if(k2<4) b->cx.m[k2]=v; else b->cx.a[k2-4]=v; } }
      c->nbact=r16(); c->bact=calloc(c->nbact?c->nbact:1,sizeof(BAct)); for(int i=0;i<c->nbact;i++){ c->bact[i].cond=r16(); c->bact[i].len=r32(); c->bact[i].code=rp; rp+=c->bact[i].len; }
      chars[id]=c; }
    else if(k==3){ uint16_t id=r16(); uint32_t l=r32(); inits[ni].sid=id; inits[ni].code=rp; inits[ni].len=l; ni++; rp+=l; }
    else if(k==4){ uint16_t id=r16(); const char *n=rs(); Sname_exports[id]=n; if(chars[id]) chars[id]->name=n; }
    else if(k==5){ uint16_t id=r16(); if(!chars[id]){ Char *c=calloc(1,sizeof(Char)); c->kind=5; c->id=id; chars[id]=c; } }
    else { fprintf(stderr,"bad record %d\n",k); exit(1); }
  }
  ninits=ni;
  /* names for exports whose char was defined later */
  for(int i=0;i<65536;i++) if(Sname_exports[i]&&chars[i]) chars[i]->name=Sname_exports[i];
}
static uint16_t exp_ids[2048]; static int nexp_ids;
Char *find_export(const char *name){ if(!nexp_ids){ for(int i=1;i<65536;i++) if(Sname_exports[i]&&nexp_ids<2048) exp_ids[nexp_ids++]=i; } for(int k=0;k<nexp_ids;k++){ int i=exp_ids[k]; if(chars[i]&&!strcmp(Sname_exports[i],name)) return chars[i]; } return NULL; }
/* ---- queue ---- */
typedef struct { MC *m; const uint8_t *code; uint32_t len; Obj *fnobj; int kind; } QItem;
static QItem q[8192]; static int qn, qh;
static void enqueue(MC *m,const uint8_t *code,uint32_t len){ if(qn<8192){ q[qn].m=m; q[qn].code=code; q[qn].len=len; qn++; } }
static void drain(void){
  int guard=0;
  while(qh<qn && guard++<100000){ QItem it=q[qh++]; if(it.m&&!it.m->dead&&it.m->obj) exec_block(it.m,it.code,it.len,NULL); }
  qh=qn=0;
}
/* ---- MC ---- */
static void kids_insert(MC *p,MC *c,int depth,int tl){
  if(p->nkids==p->ckids){ p->ckids=p->ckids?p->ckids*2:8; p->kids=realloc(p->kids,p->ckids*sizeof(MC*)); p->kdepth=realloc(p->kdepth,p->ckids*sizeof(int)); p->ktl=realloc(p->ktl,p->ckids*sizeof(int)); }
  int i=p->nkids; while(i>0&&p->kdepth[i-1]>depth){ p->kids[i]=p->kids[i-1]; p->kdepth[i]=p->kdepth[i-1]; p->ktl[i]=p->ktl[i-1]; i--; }
  p->kids[i]=c; p->kdepth[i]=depth; p->ktl[i]=tl; p->nkids++;
}
MC *mc_at_depth(MC *m,int d){ for(int i=0;i<m->nkids;i++) if(m->kdepth[i]==d) return m->kids[i]; return NULL; }
MC *mc_child(MC *m,const char *name){ for(int i=0;i<m->nkids;i++) if(m->kids[i]->name==name) return m->kids[i]; return NULL; }
void mc_event(MC *m,uint32_t flag,const char *fname){
  if(!m||m->dead) return;
  for(int i=0;i<m->nca;i++) if((m->ca[i].flags&flag) && (flag!=0x20000)) enqueue(m,m->ca[i].code,m->ca[i].len);
  if(fname && m->obj){ Val fn=getprop(m->obj,vobj(m->obj),intern(fname)); if(fn.t==T_OBJ&&(fn.u.o->kind==K_FUNC||fn.u.o->kind==K_NATIVE)){ call_func(fn,vobj(m->obj),0,NULL); } }
}
static void detach(MC *m){
  if(m->parent){ MC *p=m->parent; for(int i=0;i<p->nkids;i++) if(p->kids[i]==m){ memmove(&p->kids[i],&p->kids[i+1],(p->nkids-i-1)*sizeof(MC*)); memmove(&p->kdepth[i],&p->kdepth[i+1],(p->nkids-i-1)*sizeof(int)); memmove(&p->ktl[i],&p->ktl[i+1],(p->nkids-i-1)*sizeof(int)); p->nkids--; break; } }
}
void mc_remove(MC *m){
  if(m&&m->name&&!strcmp(m->name,"bg")&&getenv("ABOBO_DEBUG")) fprintf(stderr,"[f%d] REMOVE bg depth=%d dead=%d\n",g_frame,m->depth,m->dead);
  if(!m||m->dead) return; m->dead=1;
  if(m->nca||m->obj){ /* unload */ for(int i=0;i<m->nca;i++) if(m->ca[i].flags&4) exec_block(m,m->ca[i].code,m->ca[i].len,NULL); if(m->obj){ Val fn=getprop(m->obj,vobj(m->obj),intern("onUnload")); if(fn.t==T_OBJ) call_func(fn,vobj(m->obj),0,NULL);} }
  while(m->nkids) mc_remove(m->kids[m->nkids-1]);
  detach(m);
  if(m->obj){ m->obj->mc=NULL; }
  free(m->kids); free(m->kdepth); free(m->ktl); m->kids=NULL;
  /* keep struct allocated (dead) to keep pointers in queue safe */
}
static void apply_cmd_place(MC *m,Cmd *c,int goto_mode);
MC *mc_create(Char *def,MC *parent,int depth,const char *name,int timeline){
  MC *m=calloc(1,sizeof(MC)); m->def=def; m->parent=parent; m->name=name; m->visible=1; m->m=(Mat){1,0,0,1,0,0};
  for(int k=0;k<4;k++){ m->cx.m[k]=256; m->cx.a[k]=0; } m->depth=depth;
  m->obj=newobj(MCProto); m->obj->kind=K_MC; m->obj->mc=m; m->playing=1; m->cur=0;
  if(parent){ MC *old=mc_at_depth(parent,depth); if(old) mc_remove(old); kids_insert(parent,m,depth,timeline); }
  m->scope=newscope(m->obj,NULL);
  return m;
}
void mc_worldmat(MC *m,Mat *out){ Mat a={1,0,0,1,0,0}; MC *chain[64]; int n=0; for(MC *p=m;p&&n<64;p=p->parent) chain[n++]=p;
  for(int i=n-1;i>=0;i--){ Mat t; mat_mul(&t,&a,&chain[i]->m); a=t; } *out=a; }
void mat_mul(Mat *r,const Mat *a,const Mat *b){ Mat t; t.a=a->a*b->a+a->c*b->b; t.b=a->b*b->a+a->d*b->b; t.c=a->a*b->c+a->c*b->d; t.d=a->b*b->c+a->d*b->d; t.tx=a->a*b->tx+a->c*b->ty+a->tx; t.ty=a->b*b->tx+a->d*b->ty+a->ty; *r=t; }
/* instantiate children for a character */
static MC *instantiate(Char *c,MC *parent,int depth,const char *name,int tl);
static MC *instantiate(Char *c,MC *parent,int depth,const char *name,int tl){
  MC *m=mc_create(c,parent,depth,name,tl);
  if(c->kind==2){ m->cur=0; mc_goto(m,1,1); }
  return m;
}
void run_ctor_pub(MC *m,Char *c);
static void run_ctor(MC *m,Char *c){
  for(int i=0;i<nexps;i++) if(exps[i].c==c && exps[i].ctor.t==T_OBJ){ Val pr=getmember(exps[i].ctor,intern("prototype")); if(pr.t==T_OBJ) m->obj->proto=pr.u.o; call_func(exps[i].ctor,vobj(m->obj),0,NULL); break; }
}
void register_class(const char *name,Val ctor){ Char *c=find_export(name); if(!c) return; for(int i=0;i<nexps;i++) if(exps[i].c==c){ exps[i].ctor=ctor; return; } exps=realloc(exps,(nexps+1)*sizeof(Export)); exps[nexps].name=name; exps[nexps].c=c; exps[nexps].ctor=ctor; nexps++; }
static void fire_load(MC *m){
  if(m->onload_done) return; m->onload_done=1;
  for(int i=0;i<m->nca;i++) if(m->ca[i].flags&0x200) enqueue(m,m->ca[i].code,m->ca[i].len);
  for(int i=0;i<m->nca;i++) if(m->ca[i].flags&0x1) enqueue(m,m->ca[i].code,m->ca[i].len);
}
static void apply_cmd_place(MC *m,Cmd *c,int goto_mode){
  int D=c->depth-16384;
  MC *ex=mc_at_depth(m,D);
  if(c->ch && (!c->move || !ex || (ex->def&&ex->def->id!=c->ch)) ){
    if(goto_mode && ex && ex->def && ex->def->id==c->ch && m->ktl[0]>=0){ /* reuse */ }
    else {
      Char *def=chars[c->ch]; if(!def) return;
      MC *k=instantiate(def,m,D,c->name,1);
      k->m=c->m; k->cx=c->cx; if(c->nca){ k->ca=c->ca; k->nca=c->nca; }
      k->clipdepth=c->clipdepth;
      if(def->name) run_ctor(k,def);
      fire_load(k);
      return;
    }
    ex=mc_at_depth(m,D);
  }
  if(ex){ if(c->fl&2) ex->m=c->m; if(c->fl&4) ex->cx=c->cx; if(c->fl&8) ex->name=c->name; if(c->fl&16) ex->clipdepth=c->clipdepth; if(c->ch&&c->nca){ ex->ca=c->ca; ex->nca=c->nca; } }
}
static void exec_frame_cmds(MC *m,int fr,int goto_mode,int run_actions){
  Frame *f=&m->def->frames[fr-1];
  for(int i=0;i<f->ncmd;i++){ Cmd *c=&f->cmd[i];
    if(c->type==1) apply_cmd_place(m,c,goto_mode);
    else if(c->type==2){ MC *ex=mc_at_depth(m,c->depth-16384); if(ex) { int idx=-1; for(int k=0;k<m->nkids;k++) if(m->kids[k]==ex) idx=k; if(idx<0||m->ktl[idx]) mc_remove(ex); } }
    else if(c->type==3 && run_actions) enqueue(m,c->act,c->alen);
  }
}
int mc_findlabel(MC *m,const char *l){ if(!m->def||m->def->kind!=2) return 0; for(int i=0;i<m->def->nframes;i++){ for(int j=0;j<m->def->frames[i].ncmd;j++){ Cmd *c=&m->def->frames[i].cmd[j]; if(c->type==4&&!strcasecmp(c->label,l)) return i+1; } } return 0; }
void mc_goto(MC *m,int frame,int play){
  if(m==ROOT&&getenv("ABOBO_DEBUG")) fprintf(stderr,"[f%d] ROOT goto %d (cur %d) play=%d\n",g_frame,frame,m->cur,play);
  if(!m||m->dead||!m->def||m->def->kind!=2) return;
  int n=m->def->nframes; if(n<1) return; if(frame<1) frame=1; if(frame>n) frame=n;
  m->playing=play;
  if(m->cur==frame && m->cur!=0) return;
  if(m->cur==0){ for(int f=1;f<frame;f++) exec_frame_cmds(m,f,0,0); exec_frame_cmds(m,frame,0,1); m->cur=frame; return; }
  if(frame>m->cur){ for(int f=m->cur+1;f<frame;f++) exec_frame_cmds(m,f,0,0); exec_frame_cmds(m,frame,0,1); }
  else { /* backwards: remove timeline children then replay 1..frame, keeping same-char same-depth */
    int nk=m->nkids; MC *old[512]; int od[512]; int on=0;
    for(int i=0;i<nk&&on<512;i++) if(m->ktl[i]){ old[on]=m->kids[i]; od[on]=m->kdepth[i]; on++; }
    /* mark by temporarily detaching old ones */
    for(int i=0;i<on;i++){ /* detach without unload */ MC *o=old[i]; detach(o); }
    int reused[512]={0};
    for(int f=1;f<=frame;f++){ Frame *fr=&m->def->frames[f-1];
      for(int i=0;i<fr->ncmd;i++){ Cmd *c=&fr->cmd[i];
        if(c->type==1){
          int D=c->depth-16384; MC *ex=mc_at_depth(m,D);
          if(c->ch && !c->move){ /* try to revive an old one */
            int found=-1; for(int k=0;k<on;k++) if(!reused[k]&&old[k]->depth==D&&old[k]->def&&old[k]->def->id==c->ch&&!old[k]->dead){ found=k; break; }
            if(found>=0 && !ex){ reused[found]=1; MC *o=old[found]; kids_insert(m,o,D,1); o->m=c->m; o->cx=c->cx; o->name=c->name; o->clipdepth=c->clipdepth; if(c->nca){ o->ca=c->ca; o->nca=c->nca; } continue; }
          }
          apply_cmd_place(m,c,0);
        } else if(c->type==2){ MC *ex=mc_at_depth(m,c->depth-16384); if(ex) mc_remove(ex); }
        else if(c->type==3 && f==frame) enqueue(m,c->act,c->alen);
      } }
    for(int i=0;i<on;i++) if(!reused[i]){ MC *o=old[i]; o->parent=NULL; mc_remove(o); }
  }
  m->cur=frame;
}
void mc_tick(MC *m){
  if(!m||m->dead) return;
  if(m->def&&m->def->kind==2){
    if(m->playing){ int nx=m->cur+1; if(nx>m->def->nframes) nx=1; if(m->def->nframes>1){ mc_goto(m,nx,1); } }
  }
  MC *snap[1024]; int n=m->nkids; if(n>1024)n=1024; for(int i=0;i<n;i++) snap[i]=m->kids[i];
  for(int i=0;i<n;i++) mc_tick(snap[i]);
}
static void enter_frame_events(MC *m){
  if(!m||m->dead) return;
  mc_event(m,0x2,"onEnterFrame");
  MC *snap[1024]; int n=m->nkids; if(n>1024)n=1024; for(int i=0;i<n;i++) snap[i]=m->kids[i];
  for(int i=0;i<n;i++) enter_frame_events(snap[i]);
}
void run_inits(void){
  for(int i=0;i<ninits;i++){ exec_block(ROOT,inits[i].code,inits[i].len,NULL); drain(); }
}
static void key_events(MC *m,int down){
  if(!m||m->dead) return; mc_event(m,down?0x40:0x80,down?"onKeyDown":"onKeyUp");
  MC *snap[1024]; int n=m->nkids; if(n>1024)n=1024; for(int i=0;i<n;i++) snap[i]=m->kids[i]; for(int i=0;i<n;i++) key_events(snap[i],down);
}
extern void key_listeners_dispatch(int down); extern void intervals_tick(void); extern void sounds_tick(void);
static uint8_t prev_keys[256];
void world_tick(void){
  g_frame++;
  for(int k=0;k<256;k++){ if(keys_down[k]&&!prev_keys[k]){ key_events(ROOT,1); key_listeners_dispatch(1); drain(); break; } }
  for(int k=0;k<256;k++){ if(!keys_down[k]&&prev_keys[k]){ key_events(ROOT,0); key_listeners_dispatch(0); drain(); break; } }
  memcpy(prev_keys,keys_down,256);
  enter_frame_events(ROOT); drain();
  mc_tick(ROOT); drain();
  intervals_tick(); drain();
  if((g_frame&63)==0) gc_collect();
}
/* ---- props ---- */
static const char *P[32]; static int pinit;
static void pi(void){ if(pinit) return; pinit=1; const char *n[]={"_x","_y","_xscale","_yscale","_alpha","_visible","_rotation","_name","_parent","_currentframe","_totalframes","_width","_height","_target","_framesloaded","_xmouse","_ymouse","_root","_url","_focusrect","_droptarget","_quality","_highquality"}; for(int i=0;i<23;i++) P[i]=intern(n[i]); }
void mc_bbox(MC *m,float *x0,float *y0,float *x1,float *y1,Mat *pm,int *has);
Val tostr_mc_target(MC *m);
Val mc_getprop(MC *m,const char *name,int *found){
  pi(); *found=1;
  if(name[0]=='_'){
    if(name==P[0]) return vnum(m->m.tx); if(name==P[1]) return vnum(m->m.ty);
    if(name==P[2]) return vnum(hypot(m->m.a,m->m.b)*100); if(name==P[3]) return vnum(hypot(m->m.c,m->m.d)*100);
    if(name==P[4]) return vnum(m->cx.m[3]*100.0/256.0); if(name==P[5]) return vbool(m->visible);
    if(name==P[6]) return vnum(atan2(m->m.b,m->m.a)*180/M_PI); if(name==P[7]) return vstr(m->name?m->name:intern(""));
    if(name==P[8]){ if(m->parent) return vobj(m->parent->obj); return UNDEF; }
    if(name==P[9]) return vnum(m->cur); if(name==P[10]||name==P[14]) return vnum(m->def?m->def->nframes:1);
    if(name==P[11]||name==P[12]){ float x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; int has=0; Mat id={1,0,0,1,0,0}; mc_bbox(m,&x0,&y0,&x1,&y1,&id,&has); if(!has) return vnum(0); 
      /* bbox in parent space */ return vnum(name==P[11]?x1-x0:y1-y0); }
    if(name==P[13]) { Val v=tostr_mc_target(m); return v; }
    if(name==P[15]||name==P[16]){ Mat w; mc_worldmat(m,&w); double det=w.a*w.d-w.b*w.c; if(det==0) return vnum(0); double dx=g_mouse_x-w.tx, dy=g_mouse_y-w.ty; double lx=( w.d*dx-w.c*dy)/det, ly=(-w.b*dx+w.a*dy)/det; return vnum(name==P[15]?lx:ly); }
    if(name==P[17]) return vobj(ROOT->obj);
    if(name==P[18]) return vstr(intern("file:///game.swf"));
    if(name==P[19]) return vbool(0); if(name==P[20]) return vstr(intern("")); if(name==P[21]) return vstr(intern("HIGH")); if(name==P[22]) return vnum(1);
  }
  MC *c=mc_child(m,name); if(c) return vobj(c->obj);
  *found=0; return UNDEF;
}
Val tostr_mc_target(MC *m){ const char *parts[64]; int n=0; for(MC *p=m;p&&n<63;p=p->parent) parts[n++]=p->isroot?"":(p->name?p->name:"?"); char b[512]; b[0]=0; for(int i=n-1;i>=0;i--){ strcat(b,"/"); strcat(b,parts[i]); } if(n==1) strcpy(b,"/"); return vstr(intern(b)); }
int mc_setprop(MC *m,const char *name,Val v){
  pi(); if(name[0]!='_') return 0;
  double d=tonum(v);
  if(name==P[0]){ if(!isnan(d)) m->m.tx=d; return 1; } if(name==P[1]){ if(!isnan(d)) m->m.ty=d; return 1; }
  if(name==P[2]){ if(isnan(d)) return 1; double sx=hypot(m->m.a,m->m.b), r=atan2(m->m.b,m->m.a); if(sx==0) r=0; double ns=d/100; m->m.a=ns*cos(r); m->m.b=ns*sin(r); return 1; }
  if(name==P[3]){ if(isnan(d)) return 1; double sy=hypot(m->m.c,m->m.d), r=atan2(m->m.b,m->m.a); (void)sy; double ns=d/100; m->m.c=-ns*sin(r); m->m.d=ns*cos(r); return 1; }
  if(name==P[4]){ if(!isnan(d)) m->cx.m[3]=(int16_t)(d*256/100); return 1; }
  if(name==P[5]){ m->visible=tobool(v); return 1; }
  if(name==P[6]){ if(isnan(d)) return 1; double sx=hypot(m->m.a,m->m.b), sy=hypot(m->m.c,m->m.d), r=d*M_PI/180; m->m.a=sx*cos(r); m->m.b=sx*sin(r); m->m.c=-sy*sin(r); m->m.d=sy*cos(r); return 1; }
  if(name==P[7]){ m->name=intern(tostr(v)); return 1; }
  if(name==P[11]||name==P[12]){ float x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; int has=0; Mat id={1,0,0,1,0,0}; mc_bbox(m,&x0,&y0,&x1,&y1,&id,&has); if(has&&!isnan(d)){ double cw=x1-x0,ch=y1-y0; if(name==P[11]&&cw>0){ double s=d/cw; m->m.a*=s; m->m.b*=s; } if(name==P[12]&&ch>0){ double s=d/ch; m->m.c*=s; m->m.d*=s; } } return 1; }
  if(name==P[9]||name==P[10]||name==P[8]||name==P[13]) return 1;
  if(name==P[21]||name==P[22]||name==P[19]) return 1;
  return 0;
}
MC *mc_resolve_path(MC *base,const char *path){
  MC *cur=base; char buf[512]; strncpy(buf,path,511); buf[511]=0; char *p=buf;
  if(*p=='/'){ cur=ROOT; p++; }
  while(*p&&cur){ char *e=p; while(*e&&*e!='/'&&*e!='.') e++; char sv=*e; *e=0;
    if(*p){ if(!strcmp(p,"..")||!strcmp(p,"_parent")) cur=cur->parent; else if(!strcmp(p,"_root")||!strcmp(p,"_level0")) cur=ROOT; else if(!strcmp(p,"this")) {} else { MC *c=mc_child(cur,intern(p)); cur=c; } }
    if(!sv) break; p=e+1; }
  return cur;
}

void run_ctor_pub(MC *m,Char *c){ run_ctor(m,c); }

/* ---- mouse input ---- */
int mc_shape_hit(MC *m,double x,double y);
static MC *press_target; void buttons_mouse(int type,double x,double y);
static void mouse_rec(MC *m,int down,int *handled_press,double x,double y){
  if(!m||m->dead) return;
  MC *snap[1024]; int n=m->nkids; if(n>1024)n=1024; for(int i=0;i<n;i++) snap[i]=m->kids[i];
  mc_event(m,down==1?0x10:down==2?0x20:0x8, down==1?"onMouseDown":down==2?"onMouseUp":"onMouseMove");
  for(int i=0;i<n;i++) mouse_rec(snap[i],down,handled_press,x,y);
}
int mc_shape_hit(MC *m,double x,double y);
int mc_masked_out(MC *a,double x,double y){
  for(MC *c=a;c&&c->parent;c=c->parent){ MC *p=c->parent;
    for(int i=0;i<p->nkids;i++){ MC *k=p->kids[i]; if(k->clipdepth>0&&k->depth+16384<c->depth+16384&&c->depth+16384<=k->clipdepth){ if(!mc_shape_hit(k,x,y)) return 1; } }
    if(c->maskmc&&!c->maskmc->dead){ if(!mc_shape_hit(c->maskmc,x,y)) return 1; }
  }
  return 0;
}
static MC *find_press(MC *m,const char *fname,double x,double y){
  if(!m||m->dead||!m->visible) return NULL; MC *best=NULL;
  for(int i=m->nkids-1;i>=0;i--){ MC *r=find_press(m->kids[i],fname,x,y); if(r) return r; }
  if(m->obj){ Val fn=getprop(m->obj,vobj(m->obj),intern(fname)); if(fn.t==T_OBJ&&fn.u.o->kind==K_FUNC){ if(mc_shape_hit(m,x,y)&&!mc_masked_out(m,x,y)) return m; } }
  return best;
}
extern void mouse_listeners_dispatch(int type);
void input_mouse(int type,double x,double y){
  g_mouse_x=x; g_mouse_y=y;
  if(type==0){ mouse_rec(ROOT,0,NULL,x,y); mouse_listeners_dispatch(0); drain(); return; }
  buttons_mouse(type,x,y);
  if(type==1){ mouse_rec(ROOT,1,NULL,x,y); mouse_listeners_dispatch(1); drain();
    press_target=find_press(ROOT,"onPress",x,y); if(getenv("ABOBO_DEBUG")) fprintf(stderr,"[click f%d] press target=%s\n",g_frame,press_target?(press_target->name?press_target->name:"?"):"none"); if(press_target){ call_method(vobj(press_target->obj),"onPress",0,NULL); drain(); } }
  if(type==2){ mouse_rec(ROOT,2,NULL,x,y); mouse_listeners_dispatch(2); drain();
    if(press_target&&!press_target->dead&&press_target->obj){ call_method(vobj(press_target->obj),"onRelease",0,NULL); drain(); } press_target=NULL; }
}

/* ---- buttons ---- */
static MC *btn_down;
int mc_masked_out(MC *a,double x,double y);
static int btn_hit(MC *b,double x,double y){
  for(int i=0;i<b->nkids;i++){ MC *k=b->kids[i]; if(!k->hitonly) continue; float x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; int has=0; Mat pw; mc_worldmat(b,&pw); mc_bbox(k,&x0,&y0,&x1,&y1,&pw,&has); if(has&&x>=x0&&x<=x1&&y>=y0&&y<=y1) return 1; }
  /* fall back to up-state shapes */
  for(int i=0;i<b->nkids;i++){ MC *k=b->kids[i]; if(k->hitonly||!k->visible) continue; float x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; int has=0; Mat pw; mc_worldmat(b,&pw); mc_bbox(k,&x0,&y0,&x1,&y1,&pw,&has); if(has&&x>=x0&&x<=x1&&y>=y0&&y<=y1) return 1; }
  return 0;
}
static MC *find_button(MC *m,double x,double y){
  if(!m||m->dead||!m->visible) return NULL;
  for(int i=m->nkids-1;i>=0;i--){ MC *r=find_button(m->kids[i],x,y); if(r) return r; }
  if(m->isbutton&&btn_hit(m,x,y)&&!mc_masked_out(m,x,y)) return m; return NULL;
}
static void btn_run(MC *b,int condmask){ if(!b||b->dead||!b->def) return; MC *host=b->parent?b->parent:ROOT; for(int i=0;i<b->def->nbact;i++) if(b->def->bact[i].cond&condmask) exec_block(host,b->def->bact[i].code,b->def->bact[i].len,NULL); }
void buttons_mouse(int type,double x,double y){
  if(type==1){ MC *b=find_button(ROOT,x,y); btn_down=b; if(b){ btn_run(b,0x4|0x80); drain(); } }
  else if(type==2){ MC *b=btn_down; btn_down=NULL; if(b&&!b->dead){ if(btn_hit(b,g_mouse_x,g_mouse_y)) btn_run(b,0x8); else btn_run(b,0x100); drain(); } }
}
static void btn_keys_rec(MC *m,int key){
  if(!m||m->dead) return; MC *snap[512]; int n=m->nkids>512?512:m->nkids; for(int i=0;i<n;i++) snap[i]=m->kids[i];
  if(m->isbutton&&m->def) for(int i=0;i<m->def->nbact;i++){ int kc=(m->def->bact[i].cond>>9)&127; if(kc&&kc==key){ btn_run(m,0xfe00); } }
  for(int i=0;i<n;i++) btn_keys_rec(snap[i],key);
}
void buttons_key(int key){ btn_keys_rec(ROOT,key); drain(); }
