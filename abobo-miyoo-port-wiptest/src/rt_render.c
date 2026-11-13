#include "rt.h"
#include <math.h>
#include <zlib.h>
/* ---- bitmap cache ---- */
typedef struct { uint16_t id; int w,h; uint32_t off,csz; uint32_t *px; uint32_t last; } BM;
static BM *bms; static int nbm; static FILE *pak; static uint32_t tick; static size_t cache_bytes; static size_t cache_limit=48u*1024*1024;
static int bmidx[65536];
void bitmaps_open(const char *dir){
  char p[512]; snprintf(p,512,"%s/bitmaps.pak",dir); pak=fopen(p,"rb"); if(!pak){ fprintf(stderr,"no pak\n"); exit(1); }
  uint8_t h[8]; fread(h,1,8,pak); nbm=*(uint32_t*)(h+4); bms=calloc(nbm,sizeof(BM)); for(int i=0;i<65536;i++) bmidx[i]=-1;
  for(int i=0;i<nbm;i++){ uint8_t e[20]; fread(e,1,20,pak); bms[i].id=*(uint16_t*)e; bms[i].w=*(uint32_t*)(e+4); bms[i].h=*(uint32_t*)(e+8); bms[i].off=*(uint32_t*)(e+12); bms[i].csz=*(uint32_t*)(e+16); bmidx[bms[i].id]=i; }
}
static BM *getbm(int id){
  int i=bmidx[id]; if(i<0) return NULL; BM *b=&bms[i]; b->last=++tick;
  if(!b->px){
    size_t sz=(size_t)b->w*b->h*4;
    while(cache_bytes+sz>cache_limit){ BM *old=NULL; for(int k=0;k<nbm;k++) if(bms[k].px&&(!old||bms[k].last<old->last)&&bms[k].last!=tick) old=&bms[k]; if(!old) break; cache_bytes-=(size_t)old->w*old->h*4; free(old->px); old->px=NULL; }
    uint8_t *c=malloc(b->csz); fseek(pak,b->off,SEEK_SET); fread(c,1,b->csz,pak);
    b->px=malloc(sz); uLongf dl=sz; uncompress((Bytef*)b->px,&dl,c,b->csz); free(c); cache_bytes+=sz;
    /* convert RGBA bytes -> ARGB uint32 */
    uint8_t *s=(uint8_t*)b->px; uint32_t *d=b->px; for(size_t k=0;k<(size_t)b->w*b->h;k++){ uint8_t r=s[k*4],g=s[k*4+1],bb=s[k*4+2],a=s[k*4+3]; d[k]=((uint32_t)a<<24)|((uint32_t)r<<16)|((uint32_t)g<<8)|bb; }
  }
  return b;
}
/* ---- bbox ---- */
void mc_bbox(MC *m,float *x0,float *y0,float *x1,float *y1,Mat *pm,int *has){
  /* bbox of m's content in the coordinate space given by *pm applied to m's local coords */
  Mat mm; mat_mul(&mm,pm,&m->m);
  if(m->def&&m->def->kind==1){ Char *c=m->def; float xs[4]={c->bounds[0],c->bounds[1],c->bounds[1],c->bounds[0]}, ys[4]={c->bounds[2],c->bounds[2],c->bounds[3],c->bounds[3]};
    for(int i=0;i<4;i++){ float X=mm.a*xs[i]+mm.c*ys[i]+mm.tx, Y=mm.b*xs[i]+mm.d*ys[i]+mm.ty; if(X<*x0)*x0=X; if(X>*x1)*x1=X; if(Y<*y0)*y0=Y; if(Y>*y1)*y1=Y; } if(c->bounds[1]>c->bounds[0]) *has=1; }
  for(int i=0;i<m->nkids;i++) mc_bbox(m->kids[i],x0,y0,x1,y1,&mm,has);
}
/* ---- software drawing ---- */
static uint32_t *FB; static int FW,FH;
static uint8_t *MASK; static uint8_t *COVER;
static inline void put(int x,int y,uint32_t sr,uint32_t sg,uint32_t sb,uint32_t sa){
  if(COVER){ if(sa>16) COVER[y*FW+x]=255; return; }
  if(MASK){ sa=sa*MASK[y*FW+x]/255; } if(!sa) return;
  uint32_t *d=&FB[y*FW+x];
  if(sa>=255) *d=0xff000000|(sr<<16)|(sg<<8)|sb;
  else { uint32_t dr=(*d>>16)&255,dg=(*d>>8)&255,db=*d&255; dr=(sr*sa+dr*(255-sa))/255; dg=(sg*sa+dg*(255-sa))/255; db=(sb*sa+db*(255-sa))/255; *d=0xff000000|(dr<<16)|(dg<<8)|db; }
}
static void render_solid_poly(Fill *f,const Mat *w,const CX *cx);
static void draw_shape(Char *c,const Mat *w,const CX *cx){
  double wdet=w->a*w->d-w->b*w->c; if(wdet==0) return;
  float xs[4]={c->bounds[0],c->bounds[1],c->bounds[1],c->bounds[0]}, ys[4]={c->bounds[2],c->bounds[2],c->bounds[3],c->bounds[3]};
  float mnx=1e9,mny=1e9,mxx=-1e9,mxy=-1e9; for(int i=0;i<4;i++){ float X=w->a*xs[i]+w->c*ys[i]+w->tx, Y=w->b*xs[i]+w->d*ys[i]+w->ty; if(X<mnx)mnx=X; if(X>mxx)mxx=X; if(Y<mny)mny=Y; if(Y>mxy)mxy=Y; }
  int ix0=(int)floor(mnx), ix1=(int)ceil(mxx), iy0=(int)floor(mny), iy1=(int)ceil(mxy); if(ix0<0)ix0=0; if(iy0<0)iy0=0; if(ix1>FW)ix1=FW; if(iy1>FH)iy1=FH;
  int ma=cx->m[3], aa=cx->a[3]; int simple=(cx->m[0]==256&&cx->m[1]==256&&cx->m[2]==256&&cx->a[0]==0&&cx->a[1]==0&&cx->a[2]==0&&ma==256&&aa==0);
  if(COVER){ int any=0; for(int fi=0;fi<c->nfills;fi++) if(c->fills[fi].type==1||(c->fills[fi].type==0&&c->fills[fi].nseg>=2)) any=1;
    if(!any||c->nfills==0){ for(int y=iy0;y<iy1;y++) for(int x=ix0;x<ix1;x++){ double dx=x+0.5-w->tx, dy=y+0.5-w->ty; double lx=( w->d*dx-w->c*dy)/wdet, ly=(-w->b*dx+w->a*dy)/wdet; if(lx>=c->bounds[0]&&lx<c->bounds[1]&&ly>=c->bounds[2]&&ly<c->bounds[3]) COVER[y*FW+x]=255; } return; } }
  for(int fi=0;fi<c->nfills;fi++){ Fill *f=&c->fills[fi]; if(f->type>1) continue; if(f->type==0){ if(f->nseg>=2) render_solid_poly(f,w,cx); continue; }
    BM *b=NULL; double fa=0,fb=0,fc=0,fd=0,fdet=1; int repeat=0;
    if(f->type==1){ b=getbm(f->bid); if(!b||!b->px) continue; fa=f->m.a;fb=f->m.b;fc=f->m.c;fd=f->m.d; fdet=fa*fd-fb*fc; if(fdet==0) continue; repeat=(f->ft==0x40||f->ft==0x42); }
    /* fast path: unscaled, unrotated 1:1 bitmap blit (most sprites) */
    if(f->type==1 && !COVER && w->b==0 && w->c==0 && f->m.b==0 && f->m.c==0 && fa>0 && fd>0 && w->a>0 && w->d>0 && fabs(w->a*fa-1)<1e-4 && fabs(w->d*fd-1)<1e-4){
      double kx=0.5-w->tx-f->m.tx/fa, ky=0.5-w->ty-f->m.ty/fd; int sx=(int)floor(kx), sy=(int)floor(ky);
      int xa=(int)ceil(w->tx+w->a*c->bounds[0]-0.5), xb=(int)ceil(w->tx+w->a*c->bounds[1]-0.5), ya=(int)ceil(w->ty+w->d*c->bounds[2]-0.5), yb=(int)ceil(w->ty+w->d*c->bounds[3]-0.5);
      if(xa<0)xa=0; if(ya<0)ya=0; if(xb>FW)xb=FW; if(yb>FH)yb=FH;
      for(int y=ya;y<yb;y++){ int py=y+sy; if(repeat){ py%=b->h; if(py<0)py+=b->h; } else if(py<0||py>=b->h) continue; const uint32_t *row=b->px+(size_t)py*b->w;
        for(int x=xa;x<xb;x++){ int px=x+sx; if(repeat){ px%=b->w; if(px<0)px+=b->w; } else if(px<0||px>=b->w) continue;
          uint32_t s=row[px]; uint32_t sa=s>>24; if(!sa) continue; uint32_t sr=(s>>16)&255,sg=(s>>8)&255,sb=s&255;
          if(!simple){ int rr=(sr*cx->m[0]>>8)+cx->a[0], gg=(sg*cx->m[1]>>8)+cx->a[1], bl=(sb*cx->m[2]>>8)+cx->a[2]; int aa2=(sa*ma>>8)+aa; sr=rr<0?0:rr>255?255:rr; sg=gg<0?0:gg>255?255:gg; sb=bl<0?0:bl>255?255:bl; sa=aa2<0?0:aa2>255?255:aa2; if(!sa) continue; }
          if(!MASK && sa==255) FB[y*FW+x]=0xff000000|(sr<<16)|(sg<<8)|sb; else put(x,y,sr,sg,sb,sa); } }
      continue; }
    for(int y=iy0;y<iy1;y++) for(int x=ix0;x<ix1;x++){
      double dx=x+0.5-w->tx, dy=y+0.5-w->ty; double lx=( w->d*dx-w->c*dy)/wdet, ly=(-w->b*dx+w->a*dy)/wdet;
      if(lx<c->bounds[0]||lx>=c->bounds[1]||ly<c->bounds[2]||ly>=c->bounds[3]) continue;
      uint32_t sr,sg,sb,sa;
      if(f->type==0){ sr=f->rgba[0]; sg=f->rgba[1]; sb=f->rgba[2]; sa=f->rgba[3]; }
      else {
        double tx=lx-f->m.tx, ty=ly-f->m.ty; double bx=( fd*tx-fc*ty)/fdet, by=(-fb*tx+fa*ty)/fdet;
        int px=(int)floor(bx), py=(int)floor(by);
        if(repeat){ px%=b->w; if(px<0)px+=b->w; py%=b->h; if(py<0)py+=b->h; } else { if(px<0||py<0||px>=b->w||py>=b->h) continue; }
        uint32_t s=b->px[(size_t)py*b->w+px]; sa=s>>24; if(!sa) continue; sr=(s>>16)&255; sg=(s>>8)&255; sb=s&255;
      }
      if(!simple){ int rr=(sr*cx->m[0]>>8)+cx->a[0], gg=(sg*cx->m[1]>>8)+cx->a[1], bl=(sb*cx->m[2]>>8)+cx->a[2]; int aa2=(sa*ma>>8)+aa; sr=rr<0?0:rr>255?255:rr; sg=gg<0?0:gg>255?255:gg; sb=bl<0?0:bl>255?255:bl; sa=aa2<0?0:aa2>255?255:aa2; if(!sa) continue; }
      put(x,y,sr,sg,sb,sa);
    }
  }
}
static int cmpf(const void *a,const void *b){ float x=*(float*)a,y=*(float*)b; return x<y?-1:x>y?1:0; }
static void render_solid_poly(Fill *f,const Mat *w,const CX *cx){
  int n=f->nseg; if(n<2) return;
  float *s=malloc(sizeof(float)*4*n); float mny=1e9,mxy=-1e9;
  for(int i=0;i<n;i++){ float x0=f->seg[i*4],y0=f->seg[i*4+1],x1=f->seg[i*4+2],y1=f->seg[i*4+3];
    s[i*4]=w->a*x0+w->c*y0+w->tx; s[i*4+1]=w->b*x0+w->d*y0+w->ty; s[i*4+2]=w->a*x1+w->c*y1+w->tx; s[i*4+3]=w->b*x1+w->d*y1+w->ty;
    if(s[i*4+1]<mny)mny=s[i*4+1]; if(s[i*4+1]>mxy)mxy=s[i*4+1]; if(s[i*4+3]<mny)mny=s[i*4+3]; if(s[i*4+3]>mxy)mxy=s[i*4+3]; }
  int y0i=(int)floor(mny), y1i=(int)ceil(mxy); if(y0i<0)y0i=0; if(y1i>FH)y1i=FH;
  float *xs=malloc(sizeof(float)*(n+2)); 
  uint32_t sr=f->rgba[0],sg=f->rgba[1],sb=f->rgba[2],sa=f->rgba[3];
  int simple=(cx->m[0]==256&&cx->m[1]==256&&cx->m[2]==256&&cx->m[3]==256&&cx->a[0]==0&&cx->a[1]==0&&cx->a[2]==0&&cx->a[3]==0);
  if(!simple){ int rr=(sr*cx->m[0]>>8)+cx->a[0], gg=(sg*cx->m[1]>>8)+cx->a[1], bl=(sb*cx->m[2]>>8)+cx->a[2]; int aa=(sa*cx->m[3]>>8)+cx->a[3]; sr=rr<0?0:rr>255?255:rr; sg=gg<0?0:gg>255?255:gg; sb=bl<0?0:bl>255?255:bl; sa=aa<0?0:aa>255?255:aa; }
  for(int y=y0i;y<y1i;y++){ float yy=y+0.5f; int k=0;
    for(int i=0;i<n;i++){ float ya=s[i*4+1],yb=s[i*4+3]; if((ya<=yy&&yb>yy)||(yb<=yy&&ya>yy)){ float t=(yy-ya)/(yb-ya); xs[k++]=s[i*4]+t*(s[i*4+2]-s[i*4]); } }
    qsort(xs,k,sizeof(float),cmpf);
    for(int j=0;j+1<k;j+=2){ int xa=(int)ceil(xs[j]-0.5f), xb=(int)ceil(xs[j+1]-0.5f); if(xa<0)xa=0; if(xb>FW)xb=FW; for(int x=xa;x<xb;x++) put(x,y,sr,sg,sb,sa); } }
  free(s); free(xs);
}
static uint8_t *poolbuf[16]; static int poolused;
static uint8_t *mask_alloc(void){ if(poolused>=16) return NULL; if(!poolbuf[poolused]) poolbuf[poolused]=malloc(FW*FH); return poolbuf[poolused++]; }
static void mask_free(void){ if(poolused>0) poolused--; }
static void draw_mc(MC *m,const Mat *pm,const CX *pcx);
/* render subtree of mc into a coverage mask (its true world position), intersected with current MASK */
static uint8_t *build_mask(MC *mk){
  uint8_t *buf=mask_alloc(); if(!buf) return NULL; memset(buf,0,FW*FH);
  uint8_t *sv=COVER, *svm=MASK; COVER=buf; MASK=NULL;
  Mat pw={1,0,0,1,0,0}; if(mk->parent) mc_worldmat(mk->parent,&pw); CX id={{256,256,256,256},{0,0,0,0}};
  int sv_ismask=mk->ismask, sv_vis=mk->visible; mk->ismask=0; mk->visible=1; int sd=mk->clipdepth; mk->clipdepth=0; draw_mc(mk,&pw,&id); mk->ismask=sv_ismask; mk->visible=sv_vis; mk->clipdepth=sd;
  COVER=sv; MASK=svm;
  if(MASK) for(int i=0;i<FW*FH;i++) buf[i]=buf[i]*MASK[i]/255;
  return buf;
}
static void draw_mc(MC *m,const Mat *pm,const CX *pcx){
  if(!m||m->dead||!m->visible||m->hitonly||m->ismask) return;
  if(m->parent==ROOT&&m->name&&(!strcmp(m->name,"coverer")||!strcmp(m->name,"mpopup"))) return; /* fullscreen cover + Newgrounds medal/ad overlays: not used here */
  Mat w; mat_mul(&w,pm,&m->m);
  CX cx; for(int i=0;i<4;i++){ cx.m[i]=(int16_t)((pcx->m[i]*m->cx.m[i])>>8); cx.a[i]=(int16_t)(((pcx->a[i]*m->cx.m[i])>>8)+m->cx.a[i]); }
  uint8_t *outer=MASK; int pushed=0;
  if(!getenv("ABOBO_NOMASK")&&m->maskmc&&!m->maskmc->dead){ uint8_t *nb=build_mask(m->maskmc); if(nb){ MASK=nb; pushed++; } }
  if(m->def&&m->def->kind==1) draw_shape(m->def,&w,&cx);
  int until=-1; uint8_t *cur=MASK; int cpush=0;
  for(int i=0;i<m->nkids;i++){
    MC *k=m->kids[i];
    if(until>=0 && k->depth+16384>until){ for(;cpush>0;cpush--) mask_free(); MASK=cur; until=-1; }
    if(k->clipdepth>0 && getenv("ABOBO_NOMASK")) { continue; }
    if(k->clipdepth>0){ if(until>=0){ for(;cpush>0;cpush--) mask_free(); MASK=cur; }
      /* mask from k within this parent's coordinate space */
      uint8_t *buf=mask_alloc(); if(buf){ memset(buf,0,FW*FH); uint8_t *sv=COVER,*svm=MASK; COVER=buf; MASK=NULL; int sd=k->clipdepth; k->clipdepth=0; draw_mc(k,&w,&cx); k->clipdepth=sd; COVER=sv; MASK=svm; if(MASK) for(int p=0;p<FW*FH;p++) buf[p]=buf[p]*MASK[p]/255; MASK=buf; cpush=1; until=k->clipdepth; }
      continue; }
    draw_mc(k,&w,&cx);
  }
  for(;cpush>0;cpush--) mask_free(); for(;pushed>0;pushed--) mask_free();
  MASK=outer;
}
void render_frame(uint32_t *fb,int w,int h){
  FB=fb; FW=w; FH=h; for(int i=0;i<w*h;i++) fb[i]=0xff000000;
  Mat id={1,0,0,1,0,0}; CX cx={{256,256,256,256},{0,0,0,0}}; draw_mc(ROOT,&id,&cx);
}
/* pixel-accurate hit test of world point against visible shapes of m */
static int hit_rec(MC *m,const Mat *pm,double x,double y,int pixel){
  if(!m||m->dead||!m->visible||m->hitonly) return 0; Mat w; mat_mul(&w,pm,&m->m);
  if(m->def&&m->def->kind==1){ Char *c=m->def; double det=w.a*w.d-w.b*w.c; if(det!=0){ double dx=x-w.tx,dy=y-w.ty; double lx=(w.d*dx-w.c*dy)/det, ly=(-w.b*dx+w.a*dy)/det;
      if(lx>=c->bounds[0]&&lx<c->bounds[1]&&ly>=c->bounds[2]&&ly<c->bounds[3]){ if(!pixel) return 1; for(int fi=0;fi<c->nfills;fi++){ Fill *f=&c->fills[fi]; if(f->type!=1) continue; BM *b=getbm(f->bid); if(!b||!b->px) continue; double fdet=f->m.a*f->m.d-f->m.b*f->m.c; if(!fdet) continue; double tx=lx-f->m.tx,ty=ly-f->m.ty; double bx=(f->m.d*tx-f->m.c*ty)/fdet, by=(-f->m.b*tx+f->m.a*ty)/fdet; int px=(int)floor(bx),py=(int)floor(by); if(px>=0&&py>=0&&px<b->w&&py<b->h&&(b->px[(size_t)py*b->w+px]>>24)>16) return 1; } } } }
  for(int i=0;i<m->nkids;i++) if(hit_rec(m->kids[i],&w,x,y,pixel)) return 1;
  return 0;
}
int mc_pixel_hit(MC *m,double x,double y){ Mat pw={1,0,0,1,0,0}; if(m->parent) mc_worldmat(m->parent,&pw); return hit_rec(m,&pw,x,y,1); }
int mc_shape_hit(MC *m,double x,double y){ Mat pw={1,0,0,1,0,0}; if(m->parent) mc_worldmat(m->parent,&pw); return hit_rec(m,&pw,x,y,0); }
