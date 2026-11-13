#include "rt.h"
#include <math.h>
#include <time.h>
#include <ctype.h>
extern Val tostr_mc_target(MC *m);
void register_class(const char *name,Val ctor);
void mc_bbox(MC *m,float *x0,float *y0,float *x1,float *y1,Mat *pm,int *has);
void audio_start(int sndid,int loops,int vol,Obj *h); void audio_stop(Obj *h); void audio_stopall(void); void audio_setvol(Obj *h,int vol);
#define ARG(i) ((i)<argc?argv[i]:UNDEF)
#define SELF_OBJ() (self.t==T_OBJ?self.u.o:NULL)
static const char *S(const char *s){ return intern(s); }
static int dbg;
/* ---------- Object / Function ---------- */
static Val Object_ctor(Val self,int argc,Val *argv){ return self; }
static Val Obj_hasOwn(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return vbool(0); const char *n=intern(tostr(ARG(0))); if(findprop(o,n)) return vbool(1); if(o->kind==K_MC&&o->mc){ int f; mc_getprop(o->mc,n,&f); return vbool(f);} return vbool(0); }
static Val Obj_toString(Val self,int argc,Val *argv){ if(self.t==T_OBJ&&self.u.o->kind==K_MC) return vstr(tostr(self)); return vstr(S("[object Object]")); }
static Val Obj_valueOf(Val self,int argc,Val *argv){ return self; }
static Val Obj_addProperty(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return vbool(0); const char *n=intern(tostr(ARG(0))); Prop *p=findprop(o,n); if(!p){ setprop(o,n,UNDEF); p=findprop(o,n); } p->get=ARG(1).t==T_OBJ?ARG(1).u.o:NULL; p->set=ARG(2).t==T_OBJ?ARG(2).u.o:NULL; return vbool(1); }
static Val Obj_registerClass(Val self,int argc,Val *argv){ register_class(tostr(ARG(0)),ARG(1)); return vbool(1); }
static Val Obj_isPrototypeOf(Val self,int argc,Val *argv){ if(self.t!=T_OBJ||ARG(0).t!=T_OBJ) return vbool(0); for(Obj*q=ARG(0).u.o->proto;q;q=q->proto) if(q==self.u.o) return vbool(1); return vbool(0); }
static Val Fn_call(Val self,int argc,Val *argv){ return call_func(self,ARG(0),argc>0?argc-1:0,argv+1); }
static Val Fn_apply(Val self,int argc,Val *argv){ Val a[64]; int n=0; if(ARG(1).t==T_OBJ&&ARG(1).u.o->kind==K_ARRAY){ Obj *ar=ARG(1).u.o; n=ar->alen>64?64:ar->alen; for(int i=0;i<n;i++) a[i]=ar->arr[i]; } return call_func(self,ARG(0),n,a); }
static Val Nop(Val self,int argc,Val *argv){ return UNDEF; }
static Val RetTrue(Val self,int argc,Val *argv){ return vbool(1); }
static Val ASSetPropFlags(Val self,int argc,Val *argv){ return UNDEF; }
/* ---------- globals ---------- */
static Val g_parseInt(Val self,int argc,Val *argv){ const char *s=tostr(ARG(0)); int base=argc>1?(int)tonum(argv[1]):10; if(argc>1&&base==0) base=10; while(isspace((unsigned char)*s)) s++; if(base==10&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')) base=16; char *e; double v=(double)strtol(s,&e,base); if(e==s) return vnum(NAN); return vnum(v); }
static Val g_parseFloat(Val self,int argc,Val *argv){ const char *s=tostr(ARG(0)); char *e; double v=strtod(s,&e); if(e==s) return vnum(NAN); return vnum(v); }
static Val g_isNaN(Val self,int argc,Val *argv){ return vbool(isnan(tonum(ARG(0)))); }
static Val g_int(Val self,int argc,Val *argv){ double d=tonum(ARG(0)); return vnum(isnan(d)?0:(d<0?ceil(d):floor(d))); }
static Val g_Number(Val self,int argc,Val *argv){ return vnum(tonum(ARG(0))); }
static Val g_String(Val self,int argc,Val *argv){ return vstr(tostr(ARG(0))); }
static Val g_Boolean(Val self,int argc,Val *argv){ return vbool(tobool(ARG(0))); }
static Val g_getTimer(Val self,int argc,Val *argv){ return vnum(floor(now_ms())); }
static Val g_trace(Val self,int argc,Val *argv){ if(dbg) fprintf(stderr,"[trace] %s\n",tostr(ARG(0))); return UNDEF; }
static Val g_eval(Val self,int argc,Val *argv){ return UNDEF; }
/* ---------- Math ---------- */
#define M1(n,f) static Val Math_##n(Val self,int argc,Val *argv){ return vnum(f(tonum(ARG(0)))); }
M1(abs,fabs) M1(acos,acos) M1(asin,asin) M1(atan,atan) M1(ceil,ceil) M1(cos,cos) M1(exp,exp) M1(floor,floor) M1(log,log) M1(sin,sin) M1(sqrt,sqrt) M1(tan,tan)
static Val Math_round(Val self,int argc,Val *argv){ return vnum(floor(tonum(ARG(0))+0.5)); }
static Val Math_atan2(Val self,int argc,Val *argv){ return vnum(atan2(tonum(ARG(0)),tonum(ARG(1)))); }
static Val Math_pow(Val self,int argc,Val *argv){ return vnum(pow(tonum(ARG(0)),tonum(ARG(1)))); }
static Val Math_max(Val self,int argc,Val *argv){ double m=-INFINITY; for(int i=0;i<argc;i++){ double d=tonum(argv[i]); if(isnan(d)) return vnum(NAN); if(d>m) m=d; } return vnum(m); }
static Val Math_min(Val self,int argc,Val *argv){ double m=INFINITY; for(int i=0;i<argc;i++){ double d=tonum(argv[i]); if(isnan(d)) return vnum(NAN); if(d<m) m=d; } return vnum(m); }
static Val Math_random(Val self,int argc,Val *argv){ return vnum(rand()/(RAND_MAX+1.0)); }
/* ---------- Number / Boolean ---------- */
static Val Num_toString(Val self,int argc,Val *argv){ double d=tonum(self); if(argc>0&&tonum(argv[0])!=10){ int b=(int)tonum(argv[0]); char buf[80]; int n=0; long long v=(long long)d; int neg=v<0; if(neg) v=-v; if(v==0) buf[n++]='0'; while(v){ buf[n++]="0123456789abcdefghijklmnopqrstuvwxyz"[v%b]; v/=b; } if(neg) buf[n++]='-'; for(int i=0;i<n/2;i++){ char t=buf[i]; buf[i]=buf[n-1-i]; buf[n-1-i]=t; } buf[n]=0; return vstr(intern(buf)); } return num2str(d); }
static Val Num_valueOf(Val self,int argc,Val *argv){ return vnum(tonum(self)); }
static Val Num_toFixed(Val self,int argc,Val *argv){ char b[64]; snprintf(b,64,"%.*f",(int)tonum(ARG(0)),tonum(self)); return vstr(intern(b)); }
/* ---------- String ---------- */
static const char *ss(Val self){ return tostr(self); }
static Val Str_charAt(Val self,int argc,Val *argv){ const char *s=ss(self); int i=(int)tonum(ARG(0)); if(i<0||i>=(int)strlen(s)) return vstr(S("")); return vstr(intern_n(s+i,1)); }
static Val Str_charCodeAt(Val self,int argc,Val *argv){ const char *s=ss(self); int i=(int)tonum(ARG(0)); if(i<0||i>=(int)strlen(s)) return vnum(NAN); return vnum((uint8_t)s[i]); }
static Val Str_substr(Val self,int argc,Val *argv){ const char *s=ss(self); int n=strlen(s); int st=(int)tonum(ARG(0)); if(st<0){ st+=n; if(st<0) st=0; } if(st>n) st=n; int ln=argc>1&&ARG(1).t!=T_UNDEF?(int)tonum(argv[1]):n-st; if(ln<0||st+ln>n) ln=n-st; return vstr(intern_n(s+st,ln)); }
static Val Str_substring(Val self,int argc,Val *argv){ const char *s=ss(self); int n=strlen(s); int a=(int)tonum(ARG(0)); int b=argc>1&&ARG(1).t!=T_UNDEF?(int)tonum(argv[1]):n; if(a<0)a=0; if(b<0)b=0; if(a>n)a=n; if(b>n)b=n; if(a>b){ int t=a;a=b;b=t; } return vstr(intern_n(s+a,b-a)); }
static Val Str_slice(Val self,int argc,Val *argv){ const char *s=ss(self); int n=strlen(s); int a=(int)tonum(ARG(0)); int b=argc>1&&ARG(1).t!=T_UNDEF?(int)tonum(argv[1]):n; if(a<0)a+=n; if(b<0)b+=n; if(a<0)a=0; if(b>n)b=n; if(a>b) return vstr(S("")); return vstr(intern_n(s+a,b-a)); }
static Val Str_indexOf(Val self,int argc,Val *argv){ const char *s=ss(self); int st=argc>1?(int)tonum(argv[1]):0; if(st<0)st=0; if(st>(int)strlen(s)) return vnum(-1); const char *p=strstr(s+st,tostr(ARG(0))); return vnum(p?p-s:-1); }
static Val Str_lastIndexOf(Val self,int argc,Val *argv){ const char *s=ss(self),*t=tostr(ARG(0)); const char *last=NULL,*p=s; while((p=strstr(p,t))){ last=p; p++; } return vnum(last?last-s:-1); }
static Val Str_upper(Val self,int argc,Val *argv){ char *b=strdup(ss(self)); for(char*c=b;*c;c++) *c=toupper((unsigned char)*c); const char *r=intern(b); free(b); return vstr(r); }
static Val Str_lower(Val self,int argc,Val *argv){ char *b=strdup(ss(self)); for(char*c=b;*c;c++) *c=tolower((unsigned char)*c); const char *r=intern(b); free(b); return vstr(r); }
static Val Str_split(Val self,int argc,Val *argv){ const char *s=ss(self); Obj *a=newarray(); if(argc==0||ARG(0).t==T_UNDEF){ arr_push(a,vstr(intern(s))); return vobj(a); } const char *d=tostr(argv[0]); int dl=strlen(d);
  if(dl==0){ for(const char*c=s;*c;c++) arr_push(a,vstr(intern_n(c,1))); return vobj(a); }
  const char *p=s; while(1){ const char *q=strstr(p,d); if(!q){ arr_push(a,vstr(intern(p))); break; } arr_push(a,vstr(intern_n(p,q-p))); p=q+dl; } return vobj(a); }
static Val Str_toString(Val self,int argc,Val *argv){ return vstr(ss(self)); }
static Val Str_concat(Val self,int argc,Val *argv){ char b[2048]; strncpy(b,ss(self),2000); b[2000]=0; for(int i=0;i<argc;i++) strncat(b,tostr(argv[i]),2040-strlen(b)); return vstr(intern(b)); }
static Val Str_fromCharCode(Val self,int argc,Val *argv){ char b[64]; int n=0; for(int i=0;i<argc&&n<62;i++) b[n++]=(char)(int)tonum(argv[i]); b[n]=0; return vstr(intern(b)); }
/* ---------- Array ---------- */
static Obj *A(Val self){ return (self.t==T_OBJ&&self.u.o->kind==K_ARRAY)?self.u.o:NULL; }
static Val Array_ctor(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return UNDEF; o->kind=K_ARRAY; if(argc==1&&argv[0].t==T_NUM){ int n=(int)argv[0].u.n; setprop(o,intern("length"),vnum(n)); } else for(int i=0;i<argc;i++) arr_push(o,argv[i]); return self; }
static Val Arr_push(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return UNDEF; for(int i=0;i<argc;i++) arr_push(a,argv[i]); return vnum(a->alen); }
static Val Arr_pop(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a||!a->alen) return UNDEF; return a->arr[--a->alen]; }
static Val Arr_shift(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a||!a->alen) return UNDEF; Val v=a->arr[0]; memmove(a->arr,a->arr+1,(a->alen-1)*sizeof(Val)); a->alen--; return v; }
static Val Arr_unshift(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return UNDEF; for(int i=0;i<argc;i++) arr_push(a,UNDEF); memmove(a->arr+argc,a->arr,(a->alen-argc)*sizeof(Val)); for(int i=0;i<argc;i++) a->arr[i]=argv[i]; return vnum(a->alen); }
static Val Arr_splice(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return UNDEF; int n=a->alen; int st=(int)tonum(ARG(0)); if(st<0){ st+=n; if(st<0) st=0; } if(st>n) st=n; int dc=argc>1?(int)tonum(argv[1]):n-st; if(dc<0)dc=0; if(st+dc>n) dc=n-st; int ins=argc>2?argc-2:0;
  Obj *r=newarray(); for(int i=0;i<dc;i++) arr_push(r,a->arr[st+i]);
  int nl=n-dc+ins; Val *nv=malloc((nl+1)*sizeof(Val)); memcpy(nv,a->arr,st*sizeof(Val)); for(int i=0;i<ins;i++) nv[st+i]=argv[2+i]; memcpy(nv+st+ins,a->arr+st+dc,(n-st-dc)*sizeof(Val));
  free(a->arr); a->arr=nv; a->alen=nl; a->acap=nl+1; return vobj(r); }
static Val Arr_slice(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return UNDEF; int n=a->alen; int s=(int)tonum(ARG(0)); int e=argc>1&&ARG(1).t!=T_UNDEF?(int)tonum(argv[1]):n; if(s<0)s+=n; if(e<0)e+=n; if(s<0)s=0; if(e>n)e=n; Obj *r=newarray(); for(int i=s;i<e;i++) arr_push(r,a->arr[i]); return vobj(r); }
static Val Arr_concat(Val self,int argc,Val *argv){ Obj *a=A(self); Obj *r=newarray(); if(a) for(int i=0;i<a->alen;i++) arr_push(r,a->arr[i]); for(int k=0;k<argc;k++){ Obj *b=A(argv[k]); if(b) for(int i=0;i<b->alen;i++) arr_push(r,b->arr[i]); else arr_push(r,argv[k]); } return vobj(r); }
static Val Arr_join(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return vstr(S("")); const char *sep=argc>0?tostr(argv[0]):","; size_t cap=256,n=0; char *b=malloc(cap); b[0]=0; for(int i=0;i<a->alen;i++){ const char *t=(a->arr[i].t==T_UNDEF||a->arr[i].t==T_NULL)?"":tostr(a->arr[i]); size_t need=n+strlen(t)+strlen(sep)+2; if(need>cap){ cap=need*2; b=realloc(b,cap);} if(i){ strcpy(b+n,sep); n+=strlen(sep);} strcpy(b+n,t); n+=strlen(t); } const char *r=intern(b); free(b); return vstr(r); }
static Val Arr_reverse(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return self; for(int i=0,j=a->alen-1;i<j;i++,j--){ Val t=a->arr[i]; a->arr[i]=a->arr[j]; a->arr[j]=t; } return self; }
static Val sortfn; static const char *sortkey;
static int cmp_def(const void *x,const void *y){ return strcmp(tostr(*(Val*)x),tostr(*(Val*)y)); }
static int cmp_fn(const void *x,const void *y){ Val a[2]={*(Val*)x,*(Val*)y}; double d=tonum(call_func(sortfn,UNDEF,2,a)); return d<0?-1:d>0?1:0; }
static int cmp_key(const void *x,const void *y){ Val a=getmember(*(Val*)x,sortkey), b=getmember(*(Val*)y,sortkey); if(a.t==T_NUM&&b.t==T_NUM) return a.u.n<b.u.n?-1:a.u.n>b.u.n?1:0; return strcmp(tostr(a),tostr(b)); }
static int stable_sort_cmp_wrapper(Val *base,int n,int (*c)(const void*,const void*)){ /* insertion sort for stability */ for(int i=1;i<n;i++){ Val k=base[i]; int j=i-1; while(j>=0&&c(&base[j],&k)>0){ base[j+1]=base[j]; j--; } base[j+1]=k; } return 0; }
static Val Arr_sort(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return self; if(argc>0&&argv[0].t==T_OBJ){ sortfn=argv[0]; stable_sort_cmp_wrapper(a->arr,a->alen,cmp_fn); } else stable_sort_cmp_wrapper(a->arr,a->alen,cmp_def); return self; }
static Val Arr_sortOn(Val self,int argc,Val *argv){ Obj *a=A(self); if(!a) return self; sortkey=intern(tostr(ARG(0))); stable_sort_cmp_wrapper(a->arr,a->alen,cmp_key); return self; }
/* ---------- Key ---------- */
static Obj *keyobj; static Obj *listeners; static Obj *mlisteners;
static int lastkey;
static Val Key_isDown(Val self,int argc,Val *argv){ int k=(int)tonum(ARG(0)); return vbool(k>=0&&k<256&&keys_down[k]); }
static Val Key_getCode(Val self,int argc,Val *argv){ return vnum(lastkey); }
static Val Key_addListener(Val self,int argc,Val *argv){ if(!listeners) { listeners=newarray(); } if(ARG(0).t==T_OBJ) arr_push(listeners,ARG(0)); return UNDEF; }
static Val Key_removeListener(Val self,int argc,Val *argv){ if(!listeners) return vbool(0); for(int i=0;i<listeners->alen;i++) if(listeners->arr[i].u.o==ARG(0).u.o){ memmove(&listeners->arr[i],&listeners->arr[i+1],(listeners->alen-i-1)*sizeof(Val)); listeners->alen--; return vbool(1);} return vbool(0); }
void key_listeners_dispatch(int down){
  static uint8_t prev[256]; for(int k=0;k<256;k++){ int ch=down? (keys_down[k]&&!prev[k]) : (!keys_down[k]&&prev[k]); if(ch){ lastkey=k; if(listeners){ Val snap[64]; int n=listeners->alen>64?64:listeners->alen; for(int i=0;i<n;i++) snap[i]=listeners->arr[i]; for(int i=0;i<n;i++) call_method(snap[i],down?"onKeyDown":"onKeyUp",0,NULL); } } }
  memcpy(prev,keys_down,256); }
/* ---------- intervals ---------- */
typedef struct { int id; Val fn,obj; const char *mname; double iv,next; Val args[8]; int nargs; int active; } Intv; static Intv ivs[64]; static int nextiv=1;
static Val g_setInterval(Val self,int argc,Val *argv){ for(int i=0;i<64;i++) if(!ivs[i].active){ Intv *v=&ivs[i]; memset(v,0,sizeof *v); int a=0; if(ARG(1).t==T_STR&&ARG(0).t==T_OBJ){ v->obj=argv[0]; v->mname=intern(argv[1].u.s); a=2; } else { v->fn=ARG(0); a=1; } v->iv=tonum(ARG(a)); v->next=now_ms()+v->iv; for(int k=a+1;k<argc&&v->nargs<8;k++) v->args[v->nargs++]=argv[k]; v->id=nextiv++; v->active=1; return vnum(v->id); } return vnum(0); }
static Val g_clearInterval(Val self,int argc,Val *argv){ int id=(int)tonum(ARG(0)); for(int i=0;i<64;i++) if(ivs[i].active&&ivs[i].id==id) ivs[i].active=0; return UNDEF; }
void intervals_tick(void){ for(int i=0;i<64;i++) if(ivs[i].active&&now_ms()>=ivs[i].next){ Intv *v=&ivs[i]; v->next+=v->iv; if(v->next<now_ms()) v->next=now_ms()+v->iv; Val fn=v->fn; if(v->mname) fn=getmember(v->obj,v->mname); call_func(fn,v->obj,v->nargs,v->args); } }
void intervals_roots(void (*cb)(Val)){ for(int i=0;i<64;i++) if(ivs[i].active){ cb(ivs[i].fn); cb(ivs[i].obj); for(int k=0;k<ivs[i].nargs;k++) cb(ivs[i].args[k]); } if(listeners) cb(vobj(listeners)); if(mlisteners) cb(vobj(mlisteners)); }
/* ---------- MovieClip methods ---------- */
static MC *M(Val self){ if(self.t==T_OBJ&&self.u.o->kind==K_MC) return self.u.o->mc; return NULL; }
static Val MC_gotoAndPlay(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return UNDEF; int f=ARG(0).t==T_NUM?(int)argv[0].u.n:mc_findlabel(m,tostr(ARG(0))); if(f>0) mc_goto(m,f,1); return UNDEF; }
static Val MC_gotoAndStop(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return UNDEF; int f=ARG(0).t==T_NUM?(int)argv[0].u.n:mc_findlabel(m,tostr(ARG(0))); if(f>0) mc_goto(m,f,0); return UNDEF; }
static Val MC_play(Val self,int argc,Val *argv){ MC *m=M(self); if(m) m->playing=1; return UNDEF; }
static Val MC_stop(Val self,int argc,Val *argv){ MC *m=M(self); if(m) m->playing=0; return UNDEF; }
static Val MC_nextFrame(Val self,int argc,Val *argv){ MC *m=M(self); if(m) mc_goto(m,m->cur+1,0); return UNDEF; }
static Val MC_prevFrame(Val self,int argc,Val *argv){ MC *m=M(self); if(m) mc_goto(m,m->cur-1,0); return UNDEF; }
static Char emptychar={2};
static void copy_init(MC *k,Val init){ if(init.t!=T_OBJ) return; Obj *o=init.u.o; for(int i=0;i<o->nprops;i++) if(o->props[i].name) setprop(k->obj,o->props[i].name,o->props[i].v); }
static void run_ctor_for(MC *k,Char *c); extern void run_ctor_pub(MC *k,Char *c);
static Val MC_attachMovie(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return UNDEF; Char *c=find_export(tostr(ARG(0))); if(!c){ if(dbg) fprintf(stderr,"attachMovie: missing '%s'\n",tostr(ARG(0))); return UNDEF; }
  MC *k=mc_create(c,m,(int)tonum(ARG(2)),intern(tostr(ARG(1))),0); if(c->kind==2) mc_goto(k,1,1); copy_init(k,ARG(3)); run_ctor_pub(k,c); return vobj(k->obj); }
static Val MC_createEmpty(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return UNDEF; MC *k=mc_create(&emptychar,m,(int)tonum(ARG(1)),intern(tostr(ARG(0))),0); return vobj(k->obj); }
static Val MC_duplicate(Val self,int argc,Val *argv){ MC *m=M(self); if(!m||!m->parent) return UNDEF; MC *k=mc_create(m->def,m->parent,(int)tonum(ARG(1)),intern(tostr(ARG(0))),0); k->m=m->m; k->cx=m->cx; k->ca=m->ca; k->nca=m->nca; if(k->def->kind==2) mc_goto(k,1,1); copy_init(k,ARG(2)); return vobj(k->obj); }
static Val MC_remove(Val self,int argc,Val *argv){ MC *m=M(self); if(m&&!m->isroot) mc_remove(m); return UNDEF; }
static Val MC_swapDepths(Val self,int argc,Val *argv){ MC *m=M(self); if(!m||!m->parent) return UNDEF; MC *p=m->parent; int nd; MC *other=NULL; if(ARG(0).t==T_NUM){ nd=(int)argv[0].u.n; other=mc_at_depth(p,nd);} else { other=M(ARG(0)); if(!other) return UNDEF; nd=other->depth; }
  int od=m->depth; /* detach & reinsert both */
  int ia=-1,ib=-1; for(int i=0;i<p->nkids;i++){ if(p->kids[i]==m) ia=i; if(other&&p->kids[i]==other) ib=i; }
  if(ia>=0) p->kdepth[ia]=nd; if(ib>=0) p->kdepth[ib]=od; m->depth=nd; if(other) other->depth=od;
  /* resort */ for(int i=1;i<p->nkids;i++){ MC *k=p->kids[i]; int d=p->kdepth[i],t=p->ktl[i]; int j=i-1; while(j>=0&&p->kdepth[j]>d){ p->kids[j+1]=p->kids[j]; p->kdepth[j+1]=p->kdepth[j]; p->ktl[j+1]=p->ktl[j]; j--; } p->kids[j+1]=k; p->kdepth[j+1]=d; p->ktl[j+1]=t; }
  return UNDEF; }
static Val MC_getNextHighestDepth(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return vnum(0); int mx=-1; for(int i=0;i<m->nkids;i++) if(m->kdepth[i]>mx) mx=m->kdepth[i]; return vnum(mx<0?0:mx+1); }
static Val MC_getDepth(Val self,int argc,Val *argv){ MC *m=M(self); return vnum(m?m->depth:0); }
static void worldbbox(MC *m,float *x0,float *y0,float *x1,float *y1,int *has){ *x0=*y0=1e9; *x1=*y1=-1e9; *has=0; Mat pw={1,0,0,1,0,0}; if(m->parent) mc_worldmat(m->parent,&pw); mc_bbox(m,x0,y0,x1,y1,&pw,has); }
int mc_pixel_hit(MC *m,double x,double y);
static Val MC_hitTest(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return vbool(0);
  if(argc>=2&&ARG(0).t==T_NUM){ double x=argv[0].u.n,y=argv[1].u.n; if(argc>2&&tobool(argv[2])) return vbool(mc_pixel_hit(m,x,y)); float a,b,c,d; int h; worldbbox(m,&a,&b,&c,&d,&h); return vbool(h&&x>=a&&x<=c&&y>=b&&y<=d); }
  MC *o=M(ARG(0)); if(!o) return vbool(0); float a,b,c,d,e,f,g,hh; int h1,h2; worldbbox(m,&a,&b,&c,&d,&h1); worldbbox(o,&e,&f,&g,&hh,&h2); return vbool(h1&&h2&&a<=g&&c>=e&&b<=hh&&d>=f); }
static Val MC_getBounds(Val self,int argc,Val *argv){ MC *m=M(self); Obj *r=newobj(ObjectProto); if(!m) return vobj(r); float a,b,c,d; int h; worldbbox(m,&a,&b,&c,&d,&h); MC *t=M(ARG(0)); if(t&&t!=m->parent||!t){ } 
  if(t){ Mat w; mc_worldmat(t,&w); double det=w.a*w.d-w.b*w.c; if(det!=0){ double xs[2]={a,c},ys[2]={b,d}; double mnx=1e9,mny=1e9,mxx=-1e9,mxy=-1e9; for(int i=0;i<2;i++)for(int j=0;j<2;j++){ double dx=xs[i]-w.tx,dy=ys[j]-w.ty; double lx=(w.d*dx-w.c*dy)/det,ly=(-w.b*dx+w.a*dy)/det; if(lx<mnx)mnx=lx; if(lx>mxx)mxx=lx; if(ly<mny)mny=ly; if(ly>mxy)mxy=ly; } a=mnx;c=mxx;b=mny;d=mxy; } }
  setprop(r,S("xMin"),vnum(a)); setprop(r,S("xMax"),vnum(c)); setprop(r,S("yMin"),vnum(b)); setprop(r,S("yMax"),vnum(d)); return vobj(r); }
static Val MC_localToGlobal(Val self,int argc,Val *argv){ MC *m=M(self); if(!m||ARG(0).t!=T_OBJ) return UNDEF; Mat w; mc_worldmat(m,&w); double x=tonum(getmember(argv[0],S("x"))),y=tonum(getmember(argv[0],S("y"))); setmember(argv[0],S("x"),vnum(w.a*x+w.c*y+w.tx)); setmember(argv[0],S("y"),vnum(w.b*x+w.d*y+w.ty)); return UNDEF; }
static Val MC_globalToLocal(Val self,int argc,Val *argv){ MC *m=M(self); if(!m||ARG(0).t!=T_OBJ) return UNDEF; Mat w; mc_worldmat(m,&w); double det=w.a*w.d-w.b*w.c; if(!det) return UNDEF; double x=tonum(getmember(argv[0],S("x")))-w.tx,y=tonum(getmember(argv[0],S("y")))-w.ty; setmember(argv[0],S("x"),vnum((w.d*x-w.c*y)/det)); setmember(argv[0],S("y"),vnum((-w.b*x+w.a*y)/det)); return UNDEF; }
static Val MC_setMask(Val self,int argc,Val *argv){ MC *m=M(self); if(!m) return UNDEF; MC *k=M(ARG(0)); m->maskmc=k; if(k) k->ismask=1; return vbool(1); }
static Val MC_bytes(Val self,int argc,Val *argv){ return vnum(100); }
/* ---------- Sound ---------- */
static Val Sound_ctor(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(o){ setprop(o,S("__vol"),vnum(100)); } return self; }
static Val Sound_attach(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return UNDEF; const char *n=tostr(ARG(0)); int id=0; for(int i=1;i<65536;i++) if(Sname_exports[i]&&!strcmp(Sname_exports[i],n)){ id=i; break; } setprop(o,S("__id"),vnum(id)); return UNDEF; }
static Val Sound_start(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return UNDEF; int id=(int)tonum(getprop(o,self,S("__id"))); int loops=argc>1?(int)tonum(argv[1]):1; int vol=(int)tonum(getprop(o,self,S("__vol"))); if(id) audio_start(id,loops,vol,o); return UNDEF; }
static Val Sound_stop(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(o) audio_stop(o); return UNDEF; }
static Val Sound_setVolume(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(o){ setprop(o,S("__vol"),vnum(tonum(ARG(0)))); audio_setvol(o,(int)tonum(ARG(0))); } return UNDEF; }
static Val Sound_getVolume(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); return o?getprop(o,self,S("__vol")):vnum(100); }
static Val Sound_getPosition(Val self,int argc,Val *argv){ return vnum(0); }
static Val g_stopAllSounds(Val self,int argc,Val *argv){ audio_stopall(); return UNDEF; }
/* ---------- misc classes ---------- */
static Val SO_getLocal(Val self,int argc,Val *argv){ Obj *o=newobj(ObjectProto); setprop(o,S("data"),vobj(newobj(ObjectProto))); defnative(o,"flush",RetTrue); defnative(o,"clear",Nop); return vobj(o); }
static Val Color_ctor(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(o) setprop(o,S("__t"),ARG(0)); return self; }
static Val Color_setTransform(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return UNDEF; MC *m=M(getprop(o,self,S("__t"))); if(!m||ARG(0).t!=T_OBJ) return UNDEF; Val t=argv[0]; const char *k[]={"ra","ga","ba","aa"},*b[]={"rb","gb","bb","ab"}; for(int i=0;i<4;i++){ Val a=getmember(t,S(k[i])),bb=getmember(t,S(b[i])); if(a.t!=T_UNDEF) m->cx.m[i]=(int16_t)(tonum(a)*256/100); if(bb.t!=T_UNDEF) m->cx.a[i]=(int16_t)tonum(bb); } return UNDEF; }
static Val Color_setRGB(Val self,int argc,Val *argv){ Obj *o=SELF_OBJ(); if(!o) return UNDEF; MC *m=M(getprop(o,self,S("__t"))); if(!m) return UNDEF; int c=(int)tonum(ARG(0)); m->cx.m[0]=m->cx.m[1]=m->cx.m[2]=0; m->cx.a[0]=(c>>16)&255; m->cx.a[1]=(c>>8)&255; m->cx.a[2]=c&255; return UNDEF; }
static Val Date_ctor(Val self,int argc,Val *argv){ return self; }
static Val Date_getTime(Val self,int argc,Val *argv){ return vnum((double)time(NULL)*1000); }
static Val Date_getNum(Val self,int argc,Val *argv){ return vnum(0); }
static Val Stage_ctor(Val self,int argc,Val *argv){ return self; }
static Val BD_load(Val self,int argc,Val *argv){ Obj *o=newobj(getprop(G,UNDEF,S("BitmapData")).t==T_OBJ?getprop(getprop(G,UNDEF,S("BitmapData")).u.o,UNDEF,S("prototype")).u.o:ObjectProto); setprop(o,S("width"),vnum(1)); setprop(o,S("height"),vnum(1)); return vobj(o); }
static Val BD_clone(Val self,int argc,Val *argv){ return self; }
static Val Mouse_hide(Val self,int argc,Val *argv){ return UNDEF; }

static Val Mouse_add(Val self,int argc,Val *argv){ if(!mlisteners) mlisteners=newarray(); if(ARG(0).t==T_OBJ) arr_push(mlisteners,ARG(0)); return UNDEF; }
static Val Mouse_remove(Val self,int argc,Val *argv){ if(!mlisteners) return vbool(0); for(int i=0;i<mlisteners->alen;i++) if(mlisteners->arr[i].u.o==ARG(0).u.o){ memmove(&mlisteners->arr[i],&mlisteners->arr[i+1],(mlisteners->alen-i-1)*sizeof(Val)); mlisteners->alen--; return vbool(1);} return vbool(0); }
void mouse_listeners_dispatch(int type){ if(!mlisteners) return; Val snap[64]; int n=mlisteners->alen>64?64:mlisteners->alen; for(int i=0;i<n;i++) snap[i]=mlisteners->arr[i]; for(int i=0;i<n;i++) call_method(snap[i],type==1?"onMouseDown":type==2?"onMouseUp":"onMouseMove",0,NULL); }
static Val LV_ctor(Val self,int argc,Val *argv){ return self; }
/* ---------- registration ---------- */
static Obj *mkclass(const char *name,NativeFn ctor,Obj *proto){ Obj *c=newnative(ctor); setprop(c,S("prototype"),vobj(proto)); setprop(proto,S("constructor"),vobj(c)); Prop *p=findprop(proto,S("constructor")); p->hidden=1; p=findprop(c,S("prototype")); p->hidden=1; setprop(G,S(name),vobj(c)); findprop(G,S(name))->hidden=1; return c; }
void builtins_init(void){
  dbg=getenv("ABOBO_DEBUG")!=NULL;
  Obj *C;
  C=mkclass("Object",Object_ctor,ObjectProto); defnative(C,"registerClass",Obj_registerClass);
  defnative(ObjectProto,"hasOwnProperty",Obj_hasOwn); defnative(ObjectProto,"toString",Obj_toString); defnative(ObjectProto,"valueOf",Obj_valueOf); defnative(ObjectProto,"addProperty",Obj_addProperty); defnative(ObjectProto,"isPrototypeOf",Obj_isPrototypeOf);
  defnative(ObjectProto,"registerClass",Obj_registerClass);
  C=mkclass("Function",Object_ctor,FunctionProto); defnative(FunctionProto,"call",Fn_call); defnative(FunctionProto,"apply",Fn_apply);
  C=mkclass("Array",Array_ctor,ArrayProto);
  defnative(ArrayProto,"push",Arr_push); defnative(ArrayProto,"pop",Arr_pop); defnative(ArrayProto,"shift",Arr_shift); defnative(ArrayProto,"unshift",Arr_unshift); defnative(ArrayProto,"splice",Arr_splice); defnative(ArrayProto,"slice",Arr_slice); defnative(ArrayProto,"concat",Arr_concat); defnative(ArrayProto,"join",Arr_join); defnative(ArrayProto,"reverse",Arr_reverse); defnative(ArrayProto,"sort",Arr_sort); defnative(ArrayProto,"sortOn",Arr_sortOn);
  setprop(C,S("NUMERIC"),vnum(16)); setprop(C,S("DESCENDING"),vnum(2));
  C=mkclass("String",g_String,StringProto); defnative(C,"fromCharCode",Str_fromCharCode);
  defnative(StringProto,"charAt",Str_charAt); defnative(StringProto,"charCodeAt",Str_charCodeAt); defnative(StringProto,"substr",Str_substr); defnative(StringProto,"substring",Str_substring); defnative(StringProto,"slice",Str_slice); defnative(StringProto,"indexOf",Str_indexOf); defnative(StringProto,"lastIndexOf",Str_lastIndexOf); defnative(StringProto,"toUpperCase",Str_upper); defnative(StringProto,"toLowerCase",Str_lower); defnative(StringProto,"split",Str_split); defnative(StringProto,"toString",Str_toString); defnative(StringProto,"valueOf",Str_toString); defnative(StringProto,"concat",Str_concat);
  C=mkclass("Number",g_Number,NumberProto); setprop(C,S("MAX_VALUE"),vnum(1.7976931348623157e308)); setprop(C,S("MIN_VALUE"),vnum(5e-324)); setprop(C,S("NaN"),vnum(NAN)); setprop(C,S("POSITIVE_INFINITY"),vnum(INFINITY)); setprop(C,S("NEGATIVE_INFINITY"),vnum(-INFINITY));
  defnative(NumberProto,"toString",Num_toString); defnative(NumberProto,"valueOf",Num_valueOf); defnative(NumberProto,"toFixed",Num_toFixed);
  C=mkclass("Boolean",g_Boolean,BooleanProto); defnative(BooleanProto,"toString",Num_toString); 
  Obj *Mth=newobj(ObjectProto); setprop(G,S("Math"),vobj(Mth));
#define MD(n) defnative(Mth,#n,Math_##n)
  MD(abs);MD(acos);MD(asin);MD(atan);MD(ceil);MD(cos);MD(exp);MD(floor);MD(log);MD(sin);MD(sqrt);MD(tan);MD(round);MD(atan2);MD(pow);MD(max);MD(min);MD(random);
  setprop(Mth,S("PI"),vnum(M_PI)); setprop(Mth,S("E"),vnum(M_E)); setprop(Mth,S("SQRT2"),vnum(M_SQRT2)); setprop(Mth,S("LN2"),vnum(M_LN2));
  defnative(G,"parseInt",g_parseInt); defnative(G,"parseFloat",g_parseFloat); defnative(G,"isNaN",g_isNaN); defnative(G,"int",g_int); defnative(G,"getTimer",g_getTimer); defnative(G,"trace",g_trace); defnative(G,"setInterval",g_setInterval); defnative(G,"clearInterval",g_clearInterval); defnative(G,"ASSetPropFlags",ASSetPropFlags); defnative(G,"eval",g_eval); defnative(G,"stopAllSounds",g_stopAllSounds); defnative(G,"fscommand",Nop);
  setprop(G,S("NaN"),vnum(NAN)); setprop(G,S("Infinity"),vnum(INFINITY));
  keyobj=newobj(ObjectProto); setprop(G,S("Key"),vobj(keyobj));
  defnative(keyobj,"isDown",Key_isDown); defnative(keyobj,"getCode",Key_getCode); defnative(keyobj,"addListener",Key_addListener); defnative(keyobj,"removeListener",Key_removeListener);
  { struct {const char*n;int v;} kc[]={{"LEFT",37},{"RIGHT",39},{"UP",38},{"DOWN",40},{"SPACE",32},{"ENTER",13},{"SHIFT",16},{"CONTROL",17},{"ESCAPE",27},{"BACKSPACE",8},{"TAB",9},{"DELETEKEY",46},{"HOME",36},{"END",35},{"INSERT",45},{"PGUP",33},{"PGDN",34},{"CAPSLOCK",20}}; for(int i=0;i<18;i++) setprop(keyobj,S(kc[i].n),vnum(kc[i].v)); }
  C=mkclass("Sound",Sound_ctor,SoundProto); defnative(SoundProto,"attachSound",Sound_attach); defnative(SoundProto,"start",Sound_start); defnative(SoundProto,"stop",Sound_stop); defnative(SoundProto,"setVolume",Sound_setVolume); defnative(SoundProto,"getVolume",Sound_getVolume); defnative(SoundProto,"getPosition",Sound_getPosition); defnative(SoundProto,"setPan",Nop); defnative(SoundProto,"loadSound",Nop);
  Obj *SO=newobj(ObjectProto); setprop(G,S("SharedObject"),vobj(SO)); defnative(SO,"getLocal",SO_getLocal);
  Obj *colp=newobj(ObjectProto); mkclass("Color",Color_ctor,colp); defnative(colp,"setTransform",Color_setTransform); defnative(colp,"setRGB",Color_setRGB);
  Obj *dp=newobj(ObjectProto); mkclass("Date",Date_ctor,dp); defnative(dp,"getTime",Date_getTime); defnative(dp,"getFullYear",Date_getNum); defnative(dp,"getMonth",Date_getNum); defnative(dp,"getDate",Date_getNum); defnative(dp,"getHours",Date_getNum);
  Obj *st=newobj(ObjectProto); setprop(G,S("Stage"),vobj(st)); setprop(st,S("width"),vnum(256)); setprop(st,S("height"),vnum(254)); setprop(st,S("scaleMode"),vstr(S("noScale"))); setprop(st,S("align"),vstr(S("TL"))); setprop(st,S("displayState"),vstr(S("normal"))); setprop(st,S("showMenu"),vbool(1)); defnative(st,"addListener",Nop);
  Obj *sy=newobj(ObjectProto); setprop(G,S("System"),vobj(sy)); defnative(sy,"setClipboard",Nop); defnative(sy,"showSettings",Nop); Obj *cap=newobj(ObjectProto); setprop(sy,S("capabilities"),vobj(cap)); setprop(cap,S("language"),vstr(S("en"))); setprop(cap,S("os"),vstr(S("Linux")));
  Obj *sec=newobj(ObjectProto); setprop(sy,S("security"),vobj(sec)); defnative(sec,"allowDomain",Nop); defnative(sec,"allowInsecureDomain",Nop); defnative(sy,"security_dummy",Nop);
  { Obj *mp=newobj(ObjectProto); mkclass("MovieClipLoader",LV_ctor,mp); defnative(mp,"addListener",Nop); defnative(mp,"removeListener",Nop); defnative(mp,"unloadClip",Nop); defnative(mp,"loadClip",Nop); }
  { Obj *bp=newobj(ObjectProto); Obj *bc=mkclass("BitmapData",LV_ctor,bp); defnative(bc,"loadBitmap",BD_load); defnative(bp,"dispose",Nop); defnative(bp,"getPixel",Nop); defnative(bp,"draw",Nop); defnative(bp,"fillRect",Nop); defnative(bp,"applyFilter",Nop); defnative(bp,"copyPixels",Nop); defnative(bp,"clone",BD_clone); }
  Obj *ms=newobj(ObjectProto); setprop(G,S("Mouse"),vobj(ms)); defnative(ms,"hide",Mouse_hide); defnative(ms,"show",Mouse_hide); defnative(ms,"addListener",Mouse_add); defnative(ms,"removeListener",Mouse_remove);
  Obj *lvp=newobj(ObjectProto); mkclass("LoadVars",LV_ctor,lvp); defnative(lvp,"load",Nop); defnative(lvp,"sendAndLoad",Nop); defnative(lvp,"send",Nop);
  /* MovieClip */
  C=mkclass("MovieClip",Object_ctor,MCProto);
  defnative(MCProto,"gotoAndPlay",MC_gotoAndPlay); defnative(MCProto,"gotoAndStop",MC_gotoAndStop); defnative(MCProto,"play",MC_play); defnative(MCProto,"stop",MC_stop); defnative(MCProto,"nextFrame",MC_nextFrame); defnative(MCProto,"prevFrame",MC_prevFrame);
  defnative(MCProto,"attachMovie",MC_attachMovie); defnative(MCProto,"createEmptyMovieClip",MC_createEmpty); defnative(MCProto,"duplicateMovieClip",MC_duplicate); defnative(MCProto,"removeMovieClip",MC_remove); defnative(MCProto,"unloadMovie",MC_remove);
  defnative(MCProto,"swapDepths",MC_swapDepths); defnative(MCProto,"getNextHighestDepth",MC_getNextHighestDepth); defnative(MCProto,"getDepth",MC_getDepth); defnative(MCProto,"hitTest",MC_hitTest); defnative(MCProto,"getBounds",MC_getBounds); defnative(MCProto,"localToGlobal",MC_localToGlobal); defnative(MCProto,"globalToLocal",MC_globalToLocal);
  defnative(MCProto,"getBytesLoaded",MC_bytes); defnative(MCProto,"getBytesTotal",MC_bytes);
  const char *nops[]={"startDrag","stopDrag","attachBitmap","beginFill","beginBitmapFill","endFill","lineTo","moveTo","curveTo","lineStyle","clear","loadMovie","attachAudio","setRGB","onEnterFrame_dummy"}; for(int i=0;i<14;i++) defnative(MCProto,nops[i],Nop); defnative(MCProto,"setMask",MC_setMask);
}
void run_ctor_pub(MC *k,Char *c);
