#ifndef RT_H
#define RT_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct Obj Obj;
typedef struct MC MC;
enum { T_UNDEF, T_NULL, T_NUM, T_BOOL, T_STR, T_OBJ };
typedef struct { uint8_t t; union { double n; int b; const char *s; Obj *o; } u; } Val;
#define UNDEF ((Val){T_UNDEF,{0}})
#define NUL ((Val){T_NULL,{0}})
static inline Val vnum(double n){ Val v; v.t=T_NUM; v.u.n=n; return v; }
static inline Val vbool(int b){ Val v; v.t=T_BOOL; v.u.b=!!b; return v; }
static inline Val vstr(const char*s){ Val v; v.t=T_STR; v.u.s=s; return v; }
static inline Val vobj(Obj*o){ Val v; if(!o) return NUL; v.t=T_OBJ; v.u.o=o; return v; }

typedef Val (*NativeFn)(Val self, int argc, Val *argv);
typedef struct { const char *name; Val v; Obj *get,*set; uint8_t hidden; } Prop;
typedef struct Scope { Obj *obj; struct Scope *next; Obj *holder; } Scope;
typedef struct { const uint8_t *data; int len; } Pool_; 
enum { K_OBJ, K_ARRAY, K_FUNC, K_NATIVE, K_MC, K_SOUND, K_SUPER };
typedef struct FuncInfo {
  const uint8_t *code; int len; int nparams; const char *pnames[32]; uint8_t pregs[32];
  int nregs; uint16_t flags; int v2; const char **pool; int npool;
} FuncInfo;
struct Obj {
  uint8_t kind, mark; Obj *proto; Obj *gcnext;
  Prop *props; int nprops, cprops; int *hash; int hcap;
  Val *arr; int alen, acap;           /* K_ARRAY */
  FuncInfo *fi; Scope *scope;         /* K_FUNC */
  NativeFn nat;                       /* K_NATIVE */
  MC *mc;                             /* K_MC */
  void *ext; int extn;                /* misc (sound etc) */
};

/* strings */
const char *intern(const char *s);
const char *intern_n(const char *s,int n);
/* values */
double tonum(Val v); const char *tostr(Val v); int tobool(Val v);
Val num2str(double d);
const char *typeof_(Val v);
int equals2(Val a, Val b); int strict_eq(Val a, Val b);
/* objects */
Obj *newobj(Obj *proto); Obj *newarray(void); Obj *newnative(NativeFn f);
Prop *findprop(Obj *o,const char *name);
Val getprop(Obj *o, Val self, const char *name);
Val getmember(Val base,const char *name);
void setprop(Obj *o, const char *name, Val v);
void setmember(Val base,const char *name, Val v);
void delprop(Obj *o,const char *name);
void arr_push(Obj *a, Val v);
Val arr_get(Obj *a,int i);
void gc_collect(void);
void addroot(Val *v);
extern Obj *G, *ObjectProto, *ArrayProto, *FunctionProto, *StringProto, *NumberProto, *BooleanProto, *MCProto, *SoundProto;
/* calling */
Val call_func(Val fn, Val thisv, int argc, Val *argv);
Val call_method(Val obj,const char *name,int argc,Val *argv);
int exec_block(MC *target,const uint8_t *code,int len, const char **pool);
void rt_init(void);
Scope *newscope(Obj *o,Scope *next);
int has_lookup(Obj *o,const char *name,Val *out);
Val tostr_mc_target(MC *m);
Obj *this_proto_super(Obj *t);
void register_class(const char *name,Val ctor);
int mc_pixel_hit(MC *m,double x,double y);
void bitmaps_open(const char *dir);
void intervals_tick(void); void key_listeners_dispatch(int down);
void builtins_init(void);
extern Val g_throw; 
void defnative(Obj *o,const char*name,NativeFn f);
Val newfunc_native(NativeFn f);

/* ---- movieclips ---- */
typedef struct { float a,b,c,d,tx,ty; } Mat;
typedef struct { int16_t m[4], a[4]; } CX;
typedef struct { uint8_t type; uint16_t bid; Mat m; uint8_t ft; uint8_t rgba[4]; int nseg; float *seg; } Fill;
typedef struct { uint32_t flags; uint8_t key; uint32_t len; const uint8_t *code; } ClipAct;
typedef struct { uint8_t type; uint16_t depth; uint8_t fl, move; uint16_t ch, clipdepth; Mat m; CX cx; const char *name, *cls; int nca; ClipAct *ca; const uint8_t *act; uint32_t alen; const char *label; } Cmd;
typedef struct { int ncmd; Cmd *cmd; const char *label; } Frame;
typedef struct { uint8_t st; uint16_t ch,depth; Mat m; CX cx; } BRec;
typedef struct { uint16_t cond; uint32_t len; const uint8_t *code; } BAct;
typedef struct { uint8_t kind; uint16_t id; int nbrec; BRec *brec; int nbact; BAct *bact; float bounds[4]; int nfills; Fill *fills; int nframes; Frame *frames; const char *name; int has; } Char;
extern Char *chars[65536];
typedef struct { uint16_t sid; const uint8_t *code; uint32_t len; } InitAct;
extern InitAct *inits; extern int ninits;
struct MC {
  Obj *obj; Char *def; int id; MC *parent; const char *name;
  MC **kids; int nkids, ckids; int *kdepth; int *ktl;  /* depth, placed-by-timeline flag */
  int cur, playing; Mat m; CX cx; int visible; float alpha; int depth;
  int dead; int clipdepth; int onload_done; int isroot;
  uint16_t clipdef; ClipAct *ca; int nca;
  float rot, xs, ys; int ownmat;
  int hitonly, isbutton, btnpress, ismask; struct MC *maskmc; struct Scope *scope; Val handlers_dummy;
};
extern MC *ROOT;
MC *mc_create(Char *def, MC *parent, int depth, const char *name, int timeline);
void mc_remove(MC *m);
void mc_goto(MC *m,int frame,int play);
void mc_tick(MC *m);
int mc_findlabel(MC *m,const char *l);
MC *mc_child(MC *m,const char *name);
MC *mc_at_depth(MC *m,int d);
void scene_load(const char *dir);
void run_inits(void);
void world_tick(void);
Val mc_getprop(MC *m,const char *name,int *found);
int mc_setprop(MC *m,const char *name,Val v);
MC *mc_resolve_path(MC *base,const char *path);
extern const char *Sname_exports[65536];
Char *find_export(const char *name);
void mc_event(MC *m,uint32_t evflag,const char *fname);
/* render */
void render_frame(uint32_t *fb,int w,int h);
void mc_worldmat(MC *m, Mat *out);
void mc_bbox(MC *m,float *x0,float *y0,float *x1,float *y1,Mat *pm,int *has);
void mat_mul(Mat *r,const Mat *a,const Mat *b); /* r = a * b (apply b first then a) */
/* input */
extern uint8_t keys_down[256]; extern uint8_t keys_pressed[256];
void input_mouse(int type,double x,double y);
extern int g_frame; extern double g_mouse_x,g_mouse_y; 
double now_ms(void);
#endif
