#include "rt.h"
#include <ctype.h>
#include <float.h>
Obj *G,*ObjectProto,*ArrayProto,*FunctionProto,*StringProto,*NumberProto,*BooleanProto,*MCProto,*SoundProto;
Val g_throw;
/* ---------- string interning ---------- */
static const char **stab; static int scap, scnt;
static uint32_t hs(const char*s,int n){ uint32_t h=2166136261u; for(int i=0;i<n;i++){ h^=(uint8_t)s[i]; h*=16777619u; } return h; }
const char *intern_n(const char *s,int n){
  if(scap==0){ scap=4096; stab=calloc(scap,sizeof(char*)); }
  if(scnt*2>=scap){ int oc=scap; const char **ot=stab; scap*=2; stab=calloc(scap,sizeof(char*)); for(int i=0;i<oc;i++) if(ot[i]){ uint32_t h=hs(ot[i],strlen(ot[i]))&(scap-1); while(stab[h]) h=(h+1)&(scap-1); stab[h]=ot[i]; } free(ot); }
  uint32_t h=hs(s,n)&(scap-1);
  while(stab[h]){ if(strncmp(stab[h],s,n)==0 && stab[h][n]==0) return stab[h]; h=(h+1)&(scap-1); }
  char *c=malloc(n+1); memcpy(c,s,n); c[n]=0; stab[h]=c; scnt++; return c;
}
const char *intern(const char *s){ return intern_n(s,strlen(s)); }
/* ---------- conversions ---------- */
Val num2str(double d){
  char b[64];
  if(isnan(d)) return vstr(intern("NaN"));
  if(isinf(d)) return vstr(intern(d>0?"Infinity":"-Infinity"));
  if(d==floor(d) && fabs(d)<1e15) snprintf(b,64,"%.0f",d); else { snprintf(b,64,"%.15g",d); }
  if(!strcmp(b,"-0")) strcpy(b,"0");
  return vstr(intern(b));
}
double tonum(Val v){
  switch(v.t){
    case T_NUM: return v.u.n; case T_BOOL: return v.u.b; case T_UNDEF: case T_NULL: return NAN;
    case T_STR: { const char*s=v.u.s; while(isspace((unsigned char)*s)) s++; if(!*s) return NAN;
      char *e; double d; if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')){ d=(double)strtoul(s,&e,16);} else d=strtod(s,&e);
      while(isspace((unsigned char)*e)) e++; return *e?NAN:d; }
    case T_OBJ: if(v.u.o->kind==K_ARRAY||v.u.o->kind==K_MC) return NAN; return NAN;
  }
  return NAN;
}
const char *tostr(Val v){
  switch(v.t){
    case T_STR: return v.u.s; case T_NUM: return num2str(v.u.n).u.s;
    case T_BOOL: return intern(v.u.b?"true":"false"); case T_UNDEF: return intern("undefined"); case T_NULL: return intern("null");
    case T_OBJ: {
      Obj *o=v.u.o;
      if(o->kind==K_ARRAY){ size_t cap=64; char *b=malloc(cap); b[0]=0; size_t n=0;
        for(int i=0;i<o->alen;i++){ const char*s=o->arr[i].t==T_UNDEF||o->arr[i].t==T_NULL?"":tostr(o->arr[i]); size_t l=strlen(s)+2; if(n+l+1>cap){ cap=(n+l)*2; b=realloc(b,cap);} if(i) b[n++]=','; memcpy(b+n,s,strlen(s)); n+=strlen(s); b[n]=0; }
        const char*r=intern(b); free(b); return r; }
      if(o->kind==K_FUNC||o->kind==K_NATIVE) return intern("[type Function]");
      if(o->kind==K_MC&&o->mc){ char b[256]; MC *m=o->mc; char tmp[256]; tmp[0]=0; int n=0; MC *p=m; const char *parts[64]; while(p){ parts[n++]=p->isroot?"_level0":(p->name?p->name:"?"); p=p->parent; if(n>=63)break; }
        b[0]=0; for(int i=n-1;i>=0;i--){ strcat(b,parts[i]); if(i) strcat(b,"."); } return intern(b); }
      /* user toString? */
      Val ts=getmember(v,"toString"); if(ts.t==T_OBJ && (ts.u.o->kind==K_FUNC)) { Val r=call_func(ts,v,0,NULL); if(r.t!=T_OBJ) return tostr(r); }
      return intern("[object Object]"); }
  }
  return "";
}
int tobool(Val v){
  switch(v.t){ case T_BOOL: return v.u.b; case T_NUM: return v.u.n!=0 && !isnan(v.u.n);
    case T_STR: { double d=tonum(v); return d!=0 && !isnan(d); }
    case T_OBJ: return 1; default: return 0; }
}
const char *typeof_(Val v){
  switch(v.t){ case T_UNDEF: return "undefined"; case T_NULL: return "null"; case T_NUM: return "number"; case T_BOOL: return "boolean"; case T_STR: return "string";
    default: if(v.u.o->kind==K_FUNC||v.u.o->kind==K_NATIVE) return "function"; if(v.u.o->kind==K_MC) return "movieclip"; return "object"; }
}
int strict_eq(Val a,Val b){
  if(a.t!=b.t) return 0;
  switch(a.t){ case T_UNDEF: case T_NULL: return 1; case T_NUM: return a.u.n==b.u.n; case T_BOOL: return a.u.b==b.u.b; case T_STR: return a.u.s==b.u.s; default: return a.u.o==b.u.o; }
}
int equals2(Val a,Val b){
  if(a.t==b.t) return strict_eq(a,b);
  if((a.t==T_UNDEF||a.t==T_NULL)&&(b.t==T_UNDEF||b.t==T_NULL)) return 1;
  if(a.t==T_OBJ||b.t==T_OBJ){ if(a.t==T_OBJ&&b.t==T_OBJ) return a.u.o==b.u.o; return 0; }
  if(a.t==T_UNDEF||a.t==T_NULL||b.t==T_UNDEF||b.t==T_NULL) return 0;
  return tonum(a)==tonum(b);
}
/* ---------- objects ---------- */
static Obj *allobjs; static size_t nobjs;
Obj *newobj(Obj *proto){ Obj *o=calloc(1,sizeof(Obj)); o->proto=proto; o->gcnext=allobjs; allobjs=o; nobjs++; return o; }
Obj *newarray(void){ Obj *o=newobj(ArrayProto); o->kind=K_ARRAY; return o; }
Obj *newnative(NativeFn f){ Obj *o=newobj(FunctionProto); o->kind=K_NATIVE; o->nat=f; return o; }
Val newfunc_native(NativeFn f){ return vobj(newnative(f)); }
static void hrebuild(Obj *o){
  int cap=8; while(cap<o->nprops*3) cap<<=1;
  free(o->hash); o->hash=malloc(cap*sizeof(int)); o->hcap=cap; for(int i=0;i<cap;i++) o->hash[i]=-1;
  for(int i=0;i<o->nprops;i++) if(o->props[i].name){ uint32_t h=((uintptr_t)o->props[i].name>>3)&(cap-1); while(o->hash[h]>=0) h=(h+1)&(cap-1); o->hash[h]=i; }
}
Prop *findprop(Obj *o,const char *name){
  if(o->nprops<=6){ for(int i=0;i<o->nprops;i++) if(o->props[i].name==name) return &o->props[i]; return NULL; }
  if(!o->hash) hrebuild(o);
  uint32_t h=((uintptr_t)name>>3)&(o->hcap-1);
  while(o->hash[h]>=0){ if(o->props[o->hash[h]].name==name) return &o->props[o->hash[h]]; h=(h+1)&(o->hcap-1); }
  return NULL;
}
static Prop *addprop(Obj *o,const char *name){
  if(o->nprops==o->cprops){ o->cprops=o->cprops?o->cprops*2:4; o->props=realloc(o->props,o->cprops*sizeof(Prop)); }
  Prop *p=&o->props[o->nprops++]; memset(p,0,sizeof(Prop)); p->name=name; p->v=UNDEF;
  if(o->hash){ if(o->nprops*2>o->hcap) hrebuild(o); else { uint32_t h=((uintptr_t)name>>3)&(o->hcap-1); while(o->hash[h]>=0) h=(h+1)&(o->hcap-1); o->hash[h]=o->nprops-1; } }
  return p;
}
void delprop(Obj *o,const char *name){
  Prop *p=findprop(o,name); if(!p) return; 
  /* swap-less deletion: mark and rebuild */
  int i=p-o->props; memmove(&o->props[i],&o->props[i+1],(o->nprops-i-1)*sizeof(Prop)); o->nprops--; if(o->hash){ free(o->hash); o->hash=NULL; o->hcap=0; }
}
static int isidx(const char *s,int *out){ if(!*s||*s=='0'&&s[1]) return 0; int v=0; for(const char*c=s;*c;c++){ if(*c<'0'||*c>'9') return 0; v=v*10+(*c-'0'); if(v>10000000) return 0; } *out=v; return 1; }
void arr_push(Obj *a,Val v){ if(a->alen==a->acap){ a->acap=a->acap?a->acap*2:8; a->arr=realloc(a->arr,a->acap*sizeof(Val)); } a->arr[a->alen++]=v; }
Val arr_get(Obj *a,int i){ return (i>=0&&i<a->alen)?a->arr[i]:UNDEF; }
static void arr_setlen(Obj *a,int n){ if(n<0)n=0; if(n>a->acap){ a->acap=n; a->arr=realloc(a->arr,a->acap*sizeof(Val)); } for(int i=a->alen;i<n;i++) a->arr[i]=UNDEF; a->alen=n; }
static const char *S_proto,*S_length;
static int lookup(Obj *o,Val self,const char *name,Val *out){
  if(!S_proto){ S_proto=intern("__proto__"); S_length=intern("length"); }
  for(int depth=0;o&&depth<200;o=o->proto,depth++){
    if(o->kind==K_ARRAY){ if(name==S_length){ *out=vnum(o->alen); return 1;} int ix; if(isidx(name,&ix)){ if(ix<o->alen){ *out=o->arr[ix]; return 1; } } }
    if(o->kind==K_MC && o->mc){ int f=0; Val v=mc_getprop(o->mc,name,&f); if(f){ *out=v; return 1; } }
    if(name==S_proto){ *out=vobj(o->proto); return 1; }
    Prop *p=findprop(o,name);
    if(p){ if(p->get){ *out=call_func(vobj(p->get),self,0,NULL); return 1; } if(p->set&&!p->get){ *out=UNDEF; return 1;} *out=p->v; return 1; }
  }
  return 0;
}
Val getprop(Obj *o,Val self,const char *name){ Val r; if(lookup(o,self,name,&r)) return r; return UNDEF; }
int has_lookup(Obj *o,const char*name,Val *out){ return lookup(o,vobj(o),name,out); }
Val getmember(Val b,const char *name){
  switch(b.t){
    case T_OBJ: return getprop(b.u.o,b,name);
    case T_STR: { if(name==intern("length")) return vnum(strlen(b.u.s)); return getprop(StringProto,b,name); }
    case T_NUM: return getprop(NumberProto,b,name);
    case T_BOOL: return getprop(BooleanProto,b,name);
    default: return UNDEF;
  }
}
void setprop(Obj *o,const char *name,Val v){
  if(!S_proto){ S_proto=intern("__proto__"); S_length=intern("length"); }
  if(name==S_proto){ o->proto= v.t==T_OBJ?v.u.o:NULL; return; }
  if(o->kind==K_ARRAY){ if(name==S_length){ arr_setlen(o,(int)tonum(v)); return; } int ix; if(isidx(name,&ix)){ if(ix>=o->alen) arr_setlen(o,ix+1); o->arr[ix]=v; return; } }
  if(o->kind==K_MC && o->mc){ if(mc_setprop(o->mc,name,v)) return; }
  /* setter in chain? */
  for(Obj *q=o;q;q=q->proto){ Prop *p=findprop(q,name); if(p){ if(p->set){ call_func(vobj(p->set),vobj(o),1,&v); return; } if(p->get && !p->set) return; if(q==o){ p->v=v; return; } break; } }
  Prop *p=findprop(o,name); if(!p) p=addprop(o,name); p->v=v;
}
void setmember(Val b,const char *name,Val v){ if(b.t==T_OBJ) setprop(b.u.o,name,v); }
void defnative(Obj *o,const char*name,NativeFn f){ Prop *p=addprop(o,intern(name)); p->v=vobj(newnative(f)); p->hidden=1; }
/* ---------- GC ---------- */
static Val *roots[256]; static int nroots; void addroot(Val *v){ roots[nroots++]=v; }
static void markv(Val v);
static void marko(Obj *o){
  while(o && !o->mark){ o->mark=1;
    for(int i=0;i<o->nprops;i++){ markv(o->props[i].v); if(o->props[i].get) marko(o->props[i].get); if(o->props[i].set) marko(o->props[i].set); }
    for(int i=0;i<o->alen;i++) markv(o->arr[i]);
    for(Scope*s=o->scope;s;s=s->next){ marko(s->obj); marko(s->holder); }
    if(o->kind==K_OBJ && o->extn==1 && o->ext){ Scope *sn=o->ext; for(;sn;sn=sn->next){ marko(sn->obj); marko(sn->holder);} }
    if(o->kind==K_MC && o->mc){ MC *m=o->mc; if(m->scope) marko(m->scope->holder); for(int i=0;i<m->nkids;i++) marko(m->kids[i]->obj); if(m->parent) marko(m->parent->obj); }
    marko(o->proto==o?NULL:NULL); Obj *n=o->proto; o=n; }
}
static void markv(Val v){ if(v.t==T_OBJ) marko(v.u.o); }
extern Val *vstack; extern int *vsp_ptr;
extern void gc_extra_roots(void (*cb)(Val));
static void cbmark(Val v){ markv(v); }
void gc_collect(void){
  for(Obj *o=allobjs;o;o=o->gcnext) o->mark=0;
  marko(G); marko(ObjectProto); marko(ArrayProto); marko(FunctionProto); marko(StringProto); marko(NumberProto); marko(BooleanProto); marko(MCProto); marko(SoundProto);
  for(int i=0;i<nroots;i++) markv(*roots[i]);
  if(ROOT) marko(ROOT->obj);
  gc_extra_roots(cbmark);
  Obj **pp=&allobjs; size_t freed=0;
  while(*pp){ Obj *o=*pp; if(!o->mark){ *pp=o->gcnext; if(o->kind==K_MC&&o->mc) o->mc->obj=NULL; if(o->kind==K_OBJ&&o->extn==1) free(o->ext); free(o->props); free(o->hash); free(o->arr); free(o); freed++; nobjs--; } else pp=&o->gcnext; }
  (void)freed;
}
size_t gc_count(void){ return nobjs; }
void rt_init(void){
  ObjectProto=newobj(NULL); FunctionProto=newobj(ObjectProto); ArrayProto=newobj(ObjectProto); StringProto=newobj(ObjectProto);
  NumberProto=newobj(ObjectProto); BooleanProto=newobj(ObjectProto); MCProto=newobj(ObjectProto); SoundProto=newobj(ObjectProto);
  G=newobj(ObjectProto);
}
