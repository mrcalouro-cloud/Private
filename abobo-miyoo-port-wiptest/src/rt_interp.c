#include "rt.h"
#include <math.h>
#include <time.h>
#include <ctype.h>
#define STK 16384
static Val stack[STK]; static int sp;
extern void intervals_roots(void (*cb)(Val));
void gc_extra_roots(void (*cb)(Val)){ for(int i=0;i<sp;i++) cb(stack[i]); intervals_roots(cb); }
typedef struct Fr { Val *regs; int nregs; Obj *this_; Scope *scope; const char **pool; int npool; MC *target; Obj *locals; int isfunc; Val ret; Obj *func; Obj *superobj; } Fr;
static int depth_; Obj *this_proto_super(Obj *t);
int has_lookup(Obj *o,const char*name,Val *out);
Scope *newscope(Obj *o,Scope *next){
  Obj *h=newobj(NULL); h->extn=1; Scope *s=malloc(sizeof(Scope)); s->obj=o; s->next=next; s->holder=h; h->ext=s; return s;
}
static int exec(Fr *f,const uint8_t *code,int len);
static Val getvar(Fr *f,const char *name);
/* ---------- path helpers ---------- */
static int haspath(const char *s){ for(;*s;s++) if(*s=='.'||*s=='/'||*s==':') return 1; return 0; }
static Val getvar_scope(Fr *f,const char *name,int *found){
  Val v; *found=1;
  for(Scope *s=f->scope;s;s=s->next){ if(has_lookup(s->obj,name,&v)) return v; }
  if(has_lookup(G,name,&v)) return v;
  *found=0; return UNDEF;
}
static Val getvar(Fr *f,const char *name){
  if(name[0]=='t'&&!strcmp(name,"this")) return vobj(f->this_);
  if(name[0]=='_'){
    if(!strcmp(name,"_root")) return vobj(ROOT->obj);
    if(!strcmp(name,"_global")) return vobj(G);
    if(!strcmp(name,"_level0")) return vobj(ROOT->obj);
  }
  if(haspath(name)){
    char buf[512]; strncpy(buf,name,511); buf[511]=0; Val cur=UNDEF; int first=1; char *p=buf;
    if(*p=='/'){ cur=vobj(ROOT->obj); first=0; p++; }
    while(*p){ char *e=p; while(*e&&*e!='.'&&*e!='/'&&*e!=':') e++; char sv=*e; *e=0;
      if(*p){ const char *nm=intern(p);
        if(first){ int fd; cur=getvar_scope(f,nm,&fd); if(!strcmp(nm,"_root")) cur=vobj(ROOT->obj); first=0; }
        else if(!strcmp(nm,"..")||!strcmp(nm,"_parent")){ if(cur.t==T_OBJ&&cur.u.o->kind==K_MC&&cur.u.o->mc&&cur.u.o->mc->parent) cur=vobj(cur.u.o->mc->parent->obj); }
        else cur=getmember(cur,nm); }
      if(!sv) break; p=e+1; }
    return cur;
  }
  int fd; Val v=getvar_scope(f,intern(name),&fd);
  if(!fd){ /* special timeline names */
    if(f->target && f->target->obj){ int ff; Val t=mc_getprop(f->target,intern(name),&ff); if(ff) return t; }
  }
  return v;
}
static void setvar(Fr *f,const char *name,Val v){
  if(haspath(name)){
    char buf[512]; strncpy(buf,name,511); buf[511]=0; char *last=NULL; for(char *c=buf;*c;c++) if(*c=='.'||*c==':') last=c;
    if(last){ *last=0; Val base=getvar(f,buf); setmember(base,intern(last+1),v); return; }
  }
  const char *nm=intern(name); Val t;
  for(Scope *s=f->scope;s;s=s->next){ if(has_lookup(s->obj,nm,&t)){ setprop(s->obj,nm,v); return; } }
  Scope *s=f->scope; if(!s) { setprop(G,nm,v); return; }
  while(s->next) s=s->next; setprop(s->obj,nm,v);
}
/* ---------- calling ---------- */
Val call_func(Val fn,Val thisv,int argc,Val *argv){
  if(fn.t!=T_OBJ) return UNDEF; Obj *o=fn.u.o;
  if(o->kind==K_NATIVE) return o->nat(thisv,argc,argv);
  if(o->kind!=K_FUNC||!o->fi) return UNDEF;
  if(depth_>120) return UNDEF;
  FuncInfo *fi=o->fi; Fr f; memset(&f,0,sizeof f); Val regs[256]; 
  Obj *loc=newobj(NULL); f.locals=loc; f.isfunc=1; f.func=o;
  f.scope=newscope(loc,o->scope); f.pool=fi->pool; f.npool=fi->npool;
  f.this_= thisv.t==T_OBJ?thisv.u.o:NULL; f.target=NULL;
  /* find target MC from scope chain */
  for(Scope *s=o->scope;s;s=s->next) if(s->obj->kind==K_MC&&s->obj->mc){ f.target=s->obj->mc; break; }
  Obj *argsobj=NULL;
  if(fi->v2){
    f.regs=regs; f.nregs=fi->nregs; for(int i=0;i<fi->nregs;i++) regs[i]=UNDEF;
    int r=1; uint16_t fl=fi->flags;
    if(fl&1){ regs[r++]=thisv; }
    if(fl&4){ argsobj=newarray(); for(int i=0;i<argc;i++) arr_push(argsobj,argv[i]); setprop(argsobj,intern("callee"),fn); regs[r++]=vobj(argsobj); }
    else if(!(fl&8)){ argsobj=newarray(); for(int i=0;i<argc;i++) arr_push(argsobj,argv[i]); setprop(argsobj,intern("callee"),fn); setprop(loc,intern("arguments"),vobj(argsobj)); }
    if(fl&16){ Obj *so=newobj(this_proto_super(f.this_)); so->kind=K_SUPER; so->ext=f.this_; f.superobj=so; regs[r++]=vobj(so); }
    else if(!(fl&32)){ Obj *so=newobj(this_proto_super(f.this_)); so->kind=K_SUPER; so->ext=f.this_; f.superobj=so; setprop(loc,intern("super"),vobj(so)); }
    if(fl&64){ regs[r++]=vobj(ROOT->obj); }
    if(fl&128){ regs[r++]= f.target&&f.target->parent? vobj(f.target->parent->obj):UNDEF; }
    if(fl&256){ regs[r++]=vobj(G); }
    for(int i=0;i<fi->nparams;i++){ Val a= i<argc?argv[i]:UNDEF; if(fi->pregs[i]) regs[fi->pregs[i]]=a; else setprop(loc,intern(fi->pnames[i]),a); }
  } else {
    f.regs=regs; f.nregs=4; for(int i=0;i<4;i++) regs[i]=UNDEF;
    for(int i=0;i<fi->nparams;i++) setprop(loc,intern(fi->pnames[i]),i<argc?argv[i]:UNDEF);
    argsobj=newarray(); for(int i=0;i<argc;i++) arr_push(argsobj,argv[i]); setprop(loc,intern("arguments"),vobj(argsobj));
  }
  f.ret=UNDEF; depth_++; int st=exec(&f,fi->code,fi->len); depth_--;
  (void)st; return f.ret;
}
Obj *this_proto_super(Obj *t){ if(t&&t->proto&&t->proto->proto) return t->proto->proto; return ObjectProto; }
Val call_method(Val obj,const char *name,int argc,Val *argv){ Val fn=getmember(obj,intern(name)); return call_func(fn,obj,argc,argv); }
static Val construct(Val ctor,int argc,Val *argv){
  if(ctor.t!=T_OBJ) return UNDEF;
  Val pr=getmember(ctor,intern("prototype"));
  Obj *o=newobj(pr.t==T_OBJ?pr.u.o:ObjectProto);
  setprop(o,intern("__constructor__"),ctor); { Prop*p=findprop(o,intern("__constructor__")); if(p) p->hidden=1; }
  Val r=call_func(ctor,vobj(o),argc,argv);
  if(ctor.u.o->kind==K_NATIVE && r.t==T_OBJ) return r;
  return vobj(o);
}
/* ---------- bytecode ---------- */
#define PUSH(v) (stack[sp++]=(v))
static int dbgop_; 
#define POP() (sp>0?stack[--sp]:UNDEF)
static inline uint16_t U16(const uint8_t *p){ return p[0]|(p[1]<<8); }
static inline int16_t S16(const uint8_t *p){ return (int16_t)(p[0]|(p[1]<<8)); }
static inline uint32_t U32(const uint8_t *p){ return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24); }
static const char *rdstr(const uint8_t *p,int *adv){ int n=strlen((const char*)p); *adv=n+1; return intern_n((const char*)p,n); }
static Val add2(Val a,Val b){
  if(a.t==T_STR||b.t==T_STR || a.t==T_OBJ||b.t==T_OBJ){
    const char *x=tostr(a),*y=tostr(b); size_t lx=strlen(x),ly=strlen(y); char *buf=malloc(lx+ly+1); memcpy(buf,x,lx); memcpy(buf+lx,y,ly+1); const char *r=intern_n(buf,lx+ly); free(buf); return vstr(r);
  }
  return vnum(tonum(a)+tonum(b));
}
static int less(Val a,Val b,int *undef){ /* a<b */
  *undef=0;
  if(a.t==T_OBJ) a=vstr(tostr(a)); if(b.t==T_OBJ) b=vstr(tostr(b));
  if(a.t==T_STR&&b.t==T_STR) return strcmp(a.u.s,b.u.s)<0;
  double x=tonum(a),y=tonum(b); if(isnan(x)||isnan(y)){ *undef=1; return 0; } return x<y;
}
static Val getmember_v(Val base,Val key){
  if(base.t==T_OBJ && base.u.o->kind==K_ARRAY && key.t==T_NUM){ double d=key.u.n; int i=(int)d; if(d==i&&i>=0&&i<base.u.o->alen) return base.u.o->arr[i]; }
  return getmember(base,intern(tostr(key)));
}
static void setmember_v(Val base,Val key,Val v){
  if(base.t==T_OBJ && base.u.o->kind==K_ARRAY && key.t==T_NUM){ double d=key.u.n; int i=(int)d; if(d==i&&i>=0&&i<base.u.o->alen){ base.u.o->arr[i]=v; return; } }
  setmember(base,intern(tostr(key)),v);
}
static MC *target_from_val(Fr *f,Val v){
  if(v.t==T_OBJ&&v.u.o->kind==K_MC) return v.u.o->mc;
  if(v.t==T_STR){ return mc_resolve_path(f->target?f->target:ROOT,v.u.s); }
  return NULL;
}
static Val getprop_idx(MC *m,int idx){
  if(!m) return UNDEF;
  static const char *names[]={"_x","_y","_xscale","_yscale","_currentframe","_totalframes","_alpha","_visible","_width","_height","_rotation","_target","_framesloaded","_name","_droptarget","_url","_highquality","_focusrect","_soundbuftime","_quality","_xmouse","_ymouse"};
  if(idx<0||idx>21) return UNDEF; int f; return mc_getprop(m,intern(names[idx]),&f);
}
static Val do_newobj(Val ctor,int argc,Val *argv){ return construct(ctor,argc,argv); }
static void defun(Fr *f,const uint8_t *code,int *pc,int v2,const uint8_t *hdr_start){
  const uint8_t *p=code+*pc; /* points after opcode & length (header) */
  (void)hdr_start; int adv;
  FuncInfo *fi=calloc(1,sizeof(FuncInfo)); fi->v2=v2; const char *name=rdstr(p,&adv); p+=adv;
  int np=U16(p); p+=2;
  if(v2){ fi->nregs=*p++; fi->flags=U16(p); p+=2; }
  if(np>32) np=32; fi->nparams=np;
  for(int i=0;i<np;i++){ if(v2){ fi->pregs[i]=*p++; } fi->pnames[i]=rdstr(p,&adv); p+=adv; }
  int bl=U16(p); p+=2; fi->code=p; fi->len=bl; fi->pool=f->pool; fi->npool=f->npool;
  Obj *fo=newobj(FunctionProto); fo->kind=K_FUNC; fo->fi=fi; fo->scope=f->scope;
  Obj *pr=newobj(ObjectProto); setprop(fo,intern("prototype"),vobj(pr)); { Prop*q=findprop(fo,intern("prototype")); q->hidden=1; }
  setprop(pr,intern("constructor"),vobj(fo)); { Prop*q=findprop(pr,intern("constructor")); q->hidden=1; }
  *pc=(int)((p+bl)-code);
  if(name[0]){ /* declare */
    if(f->isfunc) setprop(f->locals,name,vobj(fo)); else setvar(f,name,vobj(fo));
  } else PUSH(vobj(fo));
}
/* returns 0 end, 1 return, 2 throw */
static int exec(Fr *f,const uint8_t *code,int len){
  int pc=0; int base_sp=sp;
  while(pc<len){
    uint8_t op=code[pc++]; if(op==0) break; dbgop_=op;
    int dl=0; const uint8_t *d=NULL;
    if(op>=0x80){ dl=U16(code+pc); d=code+pc+2; pc+=2+dl; }
    switch(op){
    case 0x04: if(f->target) mc_goto(f->target,f->target->cur+1,f->target->playing); break; /* NextFrame */
    case 0x05: if(f->target) mc_goto(f->target,f->target->cur-1,f->target->playing); break;
    case 0x06: if(f->target) f->target->playing=1; break;
    case 0x07: if(f->target) f->target->playing=0; break;
    case 0x08: case 0x09: case 0x2D: break;
    case 0x0A: { Val b=POP(),a=POP(); PUSH(vnum(tonum(a)+tonum(b))); break; }
    case 0x0B: { Val b=POP(),a=POP(); PUSH(vnum(tonum(a)-tonum(b))); break; }
    case 0x0C: { Val b=POP(),a=POP(); PUSH(vnum(tonum(a)*tonum(b))); break; }
    case 0x0D: { Val b=POP(),a=POP(); PUSH(vnum(tonum(a)/tonum(b))); break; }
    case 0x0E: { Val b=POP(),a=POP(); PUSH(vbool(tonum(a)==tonum(b))); break; }
    case 0x0F: { Val b=POP(),a=POP(); PUSH(vbool(tonum(a)<tonum(b))); break; }
    case 0x10: { Val b=POP(),a=POP(); PUSH(vbool(tobool(a)&&tobool(b))); break; }
    case 0x11: { Val b=POP(),a=POP(); PUSH(vbool(tobool(a)||tobool(b))); break; }
    case 0x12: { Val a=POP(); PUSH(vbool(!tobool(a))); break; }
    case 0x13: { Val b=POP(),a=POP(); PUSH(vbool(!strcmp(tostr(a),tostr(b)))); break; }
    case 0x14: case 0x31: { Val a=POP(); PUSH(vnum(strlen(tostr(a)))); break; }
    case 0x15: case 0x35: { Val c=POP(),i=POP(),s=POP(); const char *str=tostr(s); int n=strlen(str); int st=(int)tonum(i)-1; int ln=(int)tonum(c); if(st<0)st=0; if(st>n)st=n; if(ln<0||st+ln>n) ln=n-st; PUSH(vstr(intern_n(str+st,ln))); break; }
    case 0x17: if(sp>0) sp--; break;
    case 0x18: { Val a=POP(); double x=tonum(a); PUSH(vnum(isnan(x)?0:(x<0?ceil(x):floor(x)))); break; }
    case 0x1C: { Val n=POP(); PUSH(getvar(f,tostr(n))); break; }
    case 0x1D: { Val v=POP(),n=POP(); setvar(f,tostr(n),v); break; }
    case 0x20: { Val t=POP(); MC *m=target_from_val(f,t); f->target=m?m:f->target; break; }
    case 0x21: { Val b=POP(),a=POP(); PUSH(add2(vstr(tostr(a)),vstr(tostr(b)))); break; }
    case 0x22: { Val i=POP(),t=POP(); MC *m=target_from_val(f,t); PUSH(getprop_idx(m,(int)tonum(i))); break; }
    case 0x23: { Val v=POP(),i=POP(),t=POP(); MC *m=target_from_val(f,t); if(m){ static const char *names[]={"_x","_y","_xscale","_yscale","_currentframe","_totalframes","_alpha","_visible","_width","_height","_rotation","_target","_framesloaded","_name","_droptarget","_url","_highquality","_focusrect","_soundbuftime","_quality","_xmouse","_ymouse"}; int ix=(int)tonum(i); if(ix>=0&&ix<22) mc_setprop(m,intern(names[ix]),v); } break; }
    case 0x24: { Val dep=POP(),nn=POP(),src=POP(); MC *s=target_from_val(f,src); if(s&&s->parent){ MC *c=mc_create(s->def,s->parent,(int)tonum(dep),intern(tostr(nn)),0); if(c){ c->m=s->m; c->cx=s->cx; } } break; }
    case 0x25: { Val t=POP(); MC *m=target_from_val(f,t); if(m) mc_remove(m); break; }
    case 0x26: if(sp>0) sp--; break;
    case 0x27: sp-=(tobool(stack[sp-1])? 1:0)+4; if(sp<base_sp) sp=base_sp; break;
    case 0x28: break;
    case 0x29: { Val b=POP(),a=POP(); PUSH(vbool(strcmp(tostr(a),tostr(b))<0)); break; }
    case 0x2A: { g_throw=POP(); return 2; }
    case 0x2B: { Val c=POP(),o=POP(); int ok=0; if(o.t==T_OBJ&&c.t==T_OBJ){ Val pr=getmember(c,intern("prototype")); for(Obj*q=o.u.o->proto;q;q=q->proto) if(pr.t==T_OBJ&&q==pr.u.o){ ok=1; break; } } PUSH(ok?o:NUL); break; }
    case 0x2C: { Val n=POP(); int k=(int)tonum(n); sp-=k; sp--; sp--; break; }
    case 0x30: { Val a=POP(); int n=(int)tonum(a); PUSH(vnum(n>0?rand()%n:0)); break; }
    case 0x32: case 0x36: { Val a=POP(); const char *s=tostr(a); PUSH(vnum((uint8_t)s[0])); break; }
    case 0x33: case 0x37: { Val a=POP(); char b[2]={(char)(int)tonum(a),0}; PUSH(vstr(intern(b))); break; }
    case 0x34: PUSH(vnum(now_ms())); break;
    case 0x3A: { Val n=POP(),o=POP(); if(o.t==T_OBJ){ delprop(o.u.o,intern(tostr(n))); } PUSH(vbool(1)); break; }
    case 0x3B: { Val n=POP(); const char *nm=intern(tostr(n)); for(Scope*s=f->scope;s;s=s->next){ if(findprop(s->obj,nm)){ delprop(s->obj,nm); break; } } PUSH(vbool(1)); break; }
    case 0x3C: { Val v=POP(),n=POP(); if(f->isfunc) setprop(f->locals,intern(tostr(n)),v); else setprop(f->scope->obj,intern(tostr(n)),v); break; }
    case 0x41: { Val n=POP(); const char *nm=intern(tostr(n)); Obj *tgt=f->isfunc?f->locals:f->scope->obj; if(!findprop(tgt,nm)) setprop(tgt,nm,UNDEF); break; }
    case 0x3D: { Val n=POP(); int ac=(int)tonum(POP()); Val av[64]; if(ac>64) ac=64; for(int i=0;i<ac;i++) av[i]=POP();
      const char *nm=tostr(n); Val fn=getvar(f,nm);
      if(fn.t!=T_OBJ&&getenv("ABOBO_DEBUG")) fprintf(stderr,"[undef fn] %s()\n",nm);
      if(getenv("ABOBO_TRACEM")) fprintf(stderr,"[f%d] call %s(%d)\n",g_frame,nm,ac);
      Val r=call_func(fn,vobj(f->this_),ac,av); PUSH(r); break; }
    case 0x3E: f->ret=POP(); return 1;
    case 0x3F: { Val b=POP(),a=POP(); PUSH(vnum(fmod(tonum(a),tonum(b)))); break; }
    case 0x40: { Val n=POP(); int ac=(int)tonum(POP()); Val av[64]; if(ac>64) ac=64; for(int i=0;i<ac;i++) av[i]=POP();
      Val ctor=getvar(f,tostr(n)); PUSH(do_newobj(ctor,ac,av)); break; }
    case 0x42: { int ac=(int)tonum(POP()); Obj *a=newarray(); for(int i=0;i<ac;i++) arr_push(a,POP()); PUSH(vobj(a)); break; }
    case 0x43: { int ac=(int)tonum(POP()); Obj *o=newobj(ObjectProto); for(int i=0;i<ac;i++){ Val v=POP(); Val k=POP(); setprop(o,intern(tostr(k)),v); } PUSH(vobj(o)); break; }
    case 0x44: { Val a=POP(); PUSH(vstr(intern(typeof_(a)))); break; }
    case 0x45: { Val a=POP(); PUSH(vstr(tostr(a))); break; }
    case 0x46: case 0x55: { Val a=POP(); Val ob=a; if(op==0x46) ob=getvar(f,tostr(a));
      PUSH(NUL);
      if(ob.t==T_OBJ){ Obj *o=ob.u.o; /* collect names */
        const char *names[2048]; int nn=0;
        for(Obj *q=o;q&&nn<2000;q=q->proto){
          if(q->kind==K_ARRAY){ for(int i=0;i<q->alen&&nn<2000;i++){ char b[16]; snprintf(b,16,"%d",i); names[nn++]=intern(b);} }
          if(q->kind==K_MC&&q->mc){ MC *m=q->mc; for(int i=0;i<m->nkids&&nn<2000;i++) if(m->kids[i]->name) names[nn++]=m->kids[i]->name; }
          for(int i=0;i<q->nprops&&nn<2000;i++) if(!q->props[i].hidden){ int dup=0; for(int j=0;j<nn;j++) if(names[j]==q->props[i].name){dup=1;break;} if(!dup) names[nn++]=q->props[i].name; }
        }
        for(int i=nn-1;i>=0;i--) PUSH(vstr(names[i])); }
      break; }
    case 0x47: { Val b=POP(),a=POP(); PUSH(add2(a,b)); break; }
    case 0x48: { Val b=POP(),a=POP(); int u; int r=less(a,b,&u); PUSH(u?UNDEF:vbool(r)); break; }
    case 0x49: { Val b=POP(),a=POP(); PUSH(vbool(equals2(a,b))); break; }
    case 0x4A: { Val a=POP(); PUSH(vnum(tonum(a))); break; }
    case 0x4B: { Val a=POP(); PUSH(vstr(tostr(a))); break; }
    case 0x4C: { Val a=stack[sp-1]; PUSH(a); break; }
    case 0x4D: { Val a=stack[sp-1]; stack[sp-1]=stack[sp-2]; stack[sp-2]=a; break; }
    case 0x4E: { Val n=POP(),b=POP(); PUSH(getmember_v(b,n)); break; }
    case 0x4F: { Val v=POP(),n=POP(),b=POP(); setmember_v(b,n,v); break; }
    case 0x50: { Val a=POP(); PUSH(vnum(tonum(a)+1)); break; }
    case 0x51: { Val a=POP(); PUSH(vnum(tonum(a)-1)); break; }
    case 0x52: { Val n=POP(); Val ob=POP(); int ac=(int)tonum(POP()); Val av[64]; if(ac>64) ac=64; for(int i=0;i<ac;i++) av[i]=POP();
      Val r=UNDEF;
      if(n.t==T_UNDEF||(n.t==T_STR&&!n.u.s[0])){
        if(ob.t==T_OBJ&&ob.u.o->kind==K_SUPER){ Val c=getprop(ob.u.o,vobj((Obj*)ob.u.o->ext),intern("__constructor__")); r=call_func(c,vobj((Obj*)ob.u.o->ext),ac,av); }
        else r=call_func(ob,vobj(f->this_),ac,av);
      } else { const char *nm=tostr(n); Val fn=getmember(ob,intern(nm)); Val th=ob;
        if(ob.t==T_OBJ&&ob.u.o->kind==K_SUPER) th=vobj((Obj*)ob.u.o->ext);
        if(fn.t!=T_OBJ&&getenv("ABOBO_DEBUG")) fprintf(stderr,"[undef method] %s.%s()\n",tostr(ob),nm);
        if(getenv("ABOBO_TRACEM")) fprintf(stderr,"[f%d] method %s.%s(%d)\n",g_frame,tostr(ob),nm,ac);
        r=call_func(fn,th,ac,av); }
      PUSH(r); break; }
    case 0x53: { Val n=POP(); Val ob=POP(); int ac=(int)tonum(POP()); Val av[64]; if(ac>64) ac=64; for(int i=0;i<ac;i++) av[i]=POP();
      Val ctor= (n.t==T_STR&&!n.u.s[0])||n.t==T_UNDEF? ob : getmember(ob,intern(tostr(n))); PUSH(do_newobj(ctor,ac,av)); break; }
    case 0x54: { Val c=POP(),o=POP(); int ok=0; if(o.t==T_OBJ&&c.t==T_OBJ){ Val pr=getmember(c,intern("prototype")); for(Obj*q=o.u.o->proto;q;q=q->proto) if(pr.t==T_OBJ&&q==pr.u.o){ ok=1; break; } } PUSH(vbool(ok)); break; }
    case 0x60: { Val b=POP(),a=POP(); PUSH(vnum((double)((int32_t)(int64_t)tonum(a)&(int32_t)(int64_t)tonum(b)))); break; }
    case 0x61: { Val b=POP(),a=POP(); PUSH(vnum((double)((int32_t)(int64_t)tonum(a)|(int32_t)(int64_t)tonum(b)))); break; }
    case 0x62: { Val b=POP(),a=POP(); PUSH(vnum((double)((int32_t)(int64_t)tonum(a)^(int32_t)(int64_t)tonum(b)))); break; }
    case 0x63: { Val b=POP(),a=POP(); PUSH(vnum((double)((int32_t)((uint32_t)(int64_t)tonum(a)<<((int)tonum(b)&31))))); break; }
    case 0x64: { Val b=POP(),a=POP(); PUSH(vnum((double)((int32_t)(int64_t)tonum(a)>>((int)tonum(b)&31)))); break; }
    case 0x65: { Val b=POP(),a=POP(); PUSH(vnum((double)((uint32_t)(int64_t)tonum(a)>>((int)tonum(b)&31)))); break; }
    case 0x66: { Val b=POP(),a=POP(); PUSH(vbool(strict_eq(a,b))); break; }
    case 0x67: { Val b=POP(),a=POP(); int u; int r=less(b,a,&u); PUSH(u?UNDEF:vbool(r)); break; }
    case 0x68: { Val b=POP(),a=POP(); PUSH(vbool(strcmp(tostr(a),tostr(b))>0)); break; }
    case 0x69: { Val sup=POP(),sub=POP(); if(sub.t==T_OBJ&&sup.t==T_OBJ){ Obj *pr=newobj(ObjectProto); Val sp_=getmember(sup,intern("prototype")); pr->proto= sp_.t==T_OBJ?sp_.u.o:ObjectProto; setprop(pr,intern("__constructor__"),sup); findprop(pr,intern("__constructor__"))->hidden=1; setprop(pr,intern("constructor"),sup); findprop(pr,intern("constructor"))->hidden=1; setprop(sub.u.o,intern("prototype"),vobj(pr)); findprop(sub.u.o,intern("prototype"))->hidden=1; } break; }
    case 0x81: if(f->target){ int fr=U16(d)+1; mc_goto(f->target,fr,f->target->playing); } break;
    case 0x83: break;
    case 0x87: { Val v=stack[sp-1]; int r=d[0]; if(r<f->nregs) f->regs[r]=v; break; }
    case 0x88: { int n=U16(d); const char **pl=malloc(sizeof(char*)*(n?n:1)); const uint8_t *p=d+2; for(int i=0;i<n;i++){ int adv; pl[i]=rdstr(p,&adv); p+=adv; } f->pool=pl; f->npool=n; break; }
    case 0x8A: break;
    case 0x8B: { int adv; const char *s=rdstr(d,&adv); if(!*s) f->target=f->scope?(f->scope->obj->kind==K_MC?f->scope->obj->mc:f->target):f->target; else { MC *m=mc_resolve_path(f->target?f->target:ROOT,s); if(m) f->target=m; } break; }
    case 0x8C: { int adv; const char *s=rdstr(d,&adv); if(f->target){ int fr=mc_findlabel(f->target,s); if(fr>0) mc_goto(f->target,fr,f->target->playing); } break; }
    case 0x8D: { Val a=POP(); (void)a; break; }
    case 0x8E: { pc-=2+dl; int q=pc+2; defun(f,code,&q,1,NULL); pc=q; break; }
    case 0x9B: { pc-=2+dl; int q=pc+2; defun(f,code,&q,0,NULL); pc=q; break; }
    case 0x8F: { /* try */
      uint8_t fl=d[0]; int ts=U16(d+1),cs=U16(d+3),fs=U16(d+5); const uint8_t *q=d+7; const char *cname=NULL; int creg=-1;
      if(fl&4){ creg=*q++; } else { int adv; cname=rdstr(q,&adv); q+=adv; }
      const uint8_t *tb=q,*cb=q+ts,*fb=q+ts+cs;
      int st=exec(f,tb,ts); int tsp=sp;
      if(st==2 && (fl&1)){ Val ex=g_throw; sp=tsp; if(creg>=0&&creg<f->nregs) f->regs[creg]=ex; else if(cname) setprop(f->isfunc?f->locals:f->scope->obj,cname,ex); st=exec(f,cb,cs); }
      if(fl&2){ int st2=exec(f,fb,fs); if(st2) st=st2; }
      pc=(int)((fb+fs)-code); if(st==1) return 1; if(st==2) return 2; break; }
    case 0x94: { Val o=POP(); int sz=U16(d); if(o.t==T_OBJ){ Scope *sc=newscope(o.u.o,f->scope); Scope *old=f->scope; f->scope=sc; int st=exec(f,code+pc,sz); f->scope=old; if(st) return st; } pc+=sz; break; }
    case 0x96: { const uint8_t *p=d,*e=d+dl; while(p<e){ uint8_t t=*p++; switch(t){
        case 0: { int adv; PUSH(vstr(rdstr(p,&adv))); p+=adv; break; }
        case 1: { union{uint32_t u;float f;} x; x.u=U32(p); p+=4; PUSH(vnum(x.f)); break; }
        case 2: PUSH(NUL); break; case 3: PUSH(UNDEF); break;
        case 4: { int r=*p++; PUSH(r<f->nregs?f->regs[r]:UNDEF); break; }
        case 5: PUSH(vbool(*p++)); break;
        case 6: { uint32_t lo=U32(p),hi=U32(p+4); p+=8; union{uint64_t u;double d;} x; x.u=((uint64_t)lo<<32)|hi; PUSH(vnum(x.d)); break; }
        case 7: PUSH(vnum((double)(int32_t)U32(p))); p+=4; break;
        case 8: { int i=*p++; PUSH(i<f->npool?vstr(f->pool[i]):UNDEF); break; }
        case 9: { int i=U16(p); p+=2; PUSH(i<f->npool?vstr(f->pool[i]):UNDEF); break; }
        default: p=e; } } break; }
    case 0x99: pc+=S16(d); break;
    case 0x9A: { Val tg=POP(); (void)tg; POP(); break; }
    case 0x9D: { Val c=POP(); if(tobool(c)) pc+=S16(d); break; }
    case 0x9E: { Val fr=POP(); (void)fr; break; }
    case 0x9F: { Val fr=POP(); int play=d[0]&1; if(f->target){ int n; if(fr.t==T_NUM) n=(int)fr.u.n; else n=mc_findlabel(f->target,tostr(fr)); if(d[0]&2) n+=U16(d+1); if(n>0) mc_goto(f->target,n,play); } break; }
    default: break;
    }
    if(sp>STK-300){ sp=base_sp; return 2; }
  }
  return 0;
}
int exec_block(MC *target,const uint8_t *code,int len,const char **pool){
  if(!target||!target->obj) return 0;
  Val regs[4]; for(int i=0;i<4;i++) regs[i]=UNDEF;
  Fr f; memset(&f,0,sizeof f); f.regs=regs; f.nregs=4; f.this_=target->obj; f.target=target;
  if(!target->scope) target->scope=newscope(target->obj,NULL);
  f.scope=target->scope; f.pool=NULL; f.npool=0; int saved=sp;
  int st=exec(&f,code,len); sp=saved; return st;
}
