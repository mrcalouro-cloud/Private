"""SWF -> scene.bin converter for the mini AVM1 runtime."""
import sys, struct, json, collections
sys.path.insert(0,'.')
from swflib import *
tags=load('/tmp/abobo.swf')
out=bytearray()
def W(fmt,*a): out.extend(struct.pack('<'+fmt,*a))
def S(s):
    b=s.encode('utf8'); W('H',len(b)); out.extend(b)
def matrix(r):
    r.align(); sx=sy=1.0; r0=r1=0.0
    if r.ub(1): n=r.ub(5); sx=r.sb(n)/65536; sy=r.sb(n)/65536
    if r.ub(1): n=r.ub(5); r0=r.sb(n)/65536; r1=r.sb(n)/65536
    n=r.ub(5); tx=r.sb(n)/20; ty=r.sb(n)/20; r.align()
    return (sx,r0,r1,sy,tx,ty)
def rect(r):
    n=r.ub(5); v=[r.sb(n)/20 for _ in range(4)]; r.align(); return v
def cxform(r,alpha=True):
    r.align(); ha=r.ub(1); hm=r.ub(1); n=r.ub(4)
    m=[256,256,256,256]; a=[0,0,0,0]
    if hm: m=[r.sb(n) for _ in range(4 if alpha else 3)]+([256] if not alpha else [])
    if ha: a=[r.sb(n) for _ in range(4 if alpha else 3)]+([0] if not alpha else [])
    r.align(); return m+a
nshape=0
def parse_fillstyles(r,t,n):
    fills=[]
    for _ in range(n):
        ft=r.u8()
        if ft==0:
            if t in (2,22): c=(r.u8(),r.u8(),r.u8(),255)
            else: c=(r.u8(),r.u8(),r.u8(),r.u8())
            fills.append([0,0xffff,c])
        elif ft in (0x10,0x12,0x13):
            matrix(r); r.align(); r.u8(); ng=r.u8()&15
            for _ in range(ng): r.u8(); r.p+= 3 if t in (2,22) else 4
            if ft==0x13: r.u16()
            fills.append([2,0xffff,None])
        elif ft in (0x40,0x41,0x42,0x43):
            bid=r.u16(); m=matrix(r); fills.append([1,bid,(m,ft)])
        else: raise Exception('fill %x'%ft)
    return fills
def parse_linestyles(r,t):
    n=r.u8()
    if n==255: n=r.u16()
    for _ in range(n):
        if t==83:
            r.u16(); fl=r.u16(); 
            if ((fl>>4)&3)==2: r.u16()
            if fl&8: parse_fillstyles(r,t,1)
            else: r.p+=4
        else:
            r.u16(); r.p+= 3 if t in (2,22) else 4
def flatten(x0,y0,cx,cy,x1,y1):
    pts=[]
    for i in range(1,5):
        u=i/4; px=(1-u)**2*x0+2*(1-u)*u*cx+u*u*x1; py=(1-u)**2*y0+2*(1-u)*u*cy+u*u*y1; pts.append((px,py))
    return pts
def parse_records(r,t,nfill,nline,fills):
    """returns per-fill-index segment lists (twips->px)"""
    segs=collections.defaultdict(list)
    fb=r.ub(4); lb=r.ub(4)
    x=y=0; f0=f1=0; base=0
    while True:
        if r.ub(1)==0:
            fl=r.ub(5)
            if fl==0: break
            if fl&1: n=r.ub(5); x=r.sb(n); y=r.sb(n)
            if fl&2: f0=r.ub(fb)
            if fl&4: f1=r.ub(fb)
            if fl&8: r.ub(lb)
            if fl&16:
                r.align(); nf=r.u8()
                if nf==255 and t!=2: nf=r.u16()
                nfl=parse_fillstyles(r,t,nf); parse_linestyles(r,t); base+=len(fills); fills.extend(nfl)
                fb=r.ub(4); lb=r.ub(4)
                f0=f1=0
            continue
        if r.ub(1):  # straight
            nb=r.ub(4)+2
            if r.ub(1): dx=r.sb(nb); dy=r.sb(nb)
            elif r.ub(1): dx=0; dy=r.sb(nb)
            else: dx=r.sb(nb); dy=0
            pts=[(x+dx,y+dy)]
        else:
            nb=r.ub(4)+2; cx=r.sb(nb); cy=r.sb(nb); ax=r.sb(nb); ay=r.sb(nb)
            c=(x+cx,y+cy); a=(c[0]+ax,c[1]+ay)
            pts=flatten(x,y,c[0],c[1],a[0],a[1])
        px,py=x,y
        for (qx,qy) in pts:
            if f1: segs[base+f1].append((px,py,qx,qy))
            if f0: segs[base+f0].append((qx,qy,px,py))
            px,py=qx,qy
        x,y=px,py
    return segs
def shape(b,t):
    r=R(b,2); cid=struct.unpack_from('<H',b,0)[0]
    bounds=rect(r)
    if t==83: rect(r); r.u8()
    nf=r.u8()
    if nf==255 and t!=2: nf=r.u16()
    segs={}
    try:
        fills=parse_fillstyles(r,t,nf); parse_linestyles(r,t)
        segs=parse_records(r,t,nf,0,fills)
    except Exception as e:
        fills=fills if 'fills' in dir() else []
    return cid,bounds,fills,segs
def cmds(ts):
    frames=[[]]; labels=[]
    for t,b in ts:
        if t==1: frames.append([])
        elif t==43: frames[-1].append(('L',b[:-1].decode('utf8','replace')) if b[-1:]==b'\0' else ('L',b.decode('utf8','replace')))
        elif t in (12,):
            frames[-1].append(('A',b))
        elif t in (26,70):
            r=R(b); f1=r.u8(); f2=r.u8() if t==70 else 0
            depth=r.u16(); d={'depth':depth,'move':f1&1}
            if t==70 and (f2&0x8 or (f2&0x10 and f1&2)): d['cls']=r.cstr()
            if f1&2: d['char']=r.u16()
            if f1&4: d['m']=matrix(r)
            if f1&8: d['cx']=cxform(r)
            if f1&0x10: d['ratio']=r.u16()
            if f1&0x20: d['name']=r.cstr()
            if f1&0x40: d['clipdepth']=r.u16()
            if t==70 and f2&1:
                nfl=r.u8()
                for _ in range(nfl):
                    fid=r.u8()
                    if fid==0: r.p+=23
                    elif fid==1: r.p+=9
                    elif fid==2: r.p+=15
                    elif fid==3: r.p+=27
                    elif fid in (4,7):
                        nc=r.u8(); r.p+=5*nc+4+4+4+4+2+1
                    elif fid==5:
                        mx=r.u8(); my=r.u8(); r.p+=8+4*mx*my+4+1
                    elif fid==6: r.p+=80
                    else: raise Exception('filter %d'%fid)
                d['filters']=nfl
            if t==70 and f2&2: d['blend']=r.u8()
            if t==70 and f2&4: d['cache']=r.u8()
            if f1&0x80:
                r.u16(); r.u32(); ca=[]
                while True:
                    fl=r.u32()
                    if fl==0: break
                    sz=r.u32(); key=0; body=b[r.p:r.p+sz]
                    if fl&0x20000: key=body[0]; body=body[1:]
                    ca.append((fl,key,body)); r.p+=sz
                d['ca']=ca
            frames[-1].append(('P',d))
        elif t in (5,28): 
            d=struct.unpack_from('<H',b,0)[0]; frames[-1].append(('R',d))
        elif t==4:
            pass
    if not frames[-1] and len(frames)>1: frames.pop()
    return frames
def subtags(b):
    r=R(b,4); o=[]
    while r.p<len(b):
        h=r.u16(); t=h>>6; l=h&63
        if l==63: l=r.u32()
        o.append((t,b[r.p:r.p+l])); r.p+=l
        if t==0: break
    return o
def emit_timeline(kind,cid,ts):
    fr=cmds(ts)
    W('BH',kind,cid); W('H',len(fr))
    for f in fr:
        W('H',len(f))
        for k,v in f:
            if k=='A': W('B',3); W('I',len(v)); out.extend(v)
            elif k=='L': W('B',4); S(v)
            elif k=='R': W('B',2); W('H',v)
            elif k=='P':
                fl=0
                for key,bit in (('char',1),('m',2),('cx',4),('name',8),('clipdepth',16),('ca',32),('cls',64)):
                    if key in v: fl|=bit
                W('BHBB',1,v['depth'],fl,v['move'])
                if 'char' in v: W('H',v['char'])
                if 'm' in v: W('6f',*v['m'])
                if 'cx' in v: W('8h',*v['cx'])
                if 'name' in v: S(v['name'])
                if 'clipdepth' in v: W('H',v['clipdepth'])
                if 'cls' in v: S(v['cls'])
                if 'ca' in v:
                    W('B',len(v['ca']))
                    for fl2,key,body in v['ca']: W('IBI',fl2,key,len(body)); out.extend(body)
# --- chars
counts=collections.Counter()
body=bytearray()
W('4s',b'ABOB')
exp={}; inits=[]; btn=0; text=0
for t,b in tags:
    if t in (2,22,32,83):
        cid,bd,fills,segs=shape(b,t); W('BH',1,cid); W('4f',*bd); W('H',len(fills))
        for idx,(ft,bid,mm) in enumerate(fills):
            W('BH',ft,bid)
            if ft==0:
                W('4B',*mm); sg=segs.get(idx+1,[]); W('H',min(len(sg),65535))
                for q in sg[:65535]: W('4f',q[0]/20,q[1]/20,q[2]/20,q[3]/20)
            elif mm: W('6f',*mm[0]); W('B',mm[1])
        counts['shape']+=1
    elif t==39:
        cid=struct.unpack_from('<H',b,0)[0]; emit_timeline(2,cid,subtags(b)); counts['sprite']+=1
    elif t==59:
        cid=struct.unpack_from('<H',b,0)[0]; W('BHI',3,cid,len(b)-2); out.extend(b[2:])
    elif t==56:
        r=R(b); n=r.u16()
        for _ in range(n): i=r.u16(); exp[i]=r.cstr()
    elif t==34:
        counts['button']+=1
        r=R(b); cid=r.u16(); r.u8(); aoff=r.u16(); recs=[]
        while True:
            fl=r.u8()
            if fl==0: break
            ch=r.u16(); dp=r.u16(); m=matrix(r); cx=cxform(r)
            if fl&0x10: 
                nfl=r.u8()
                for _ in range(nfl):
                    fid=r.u8()
                    if fid==0: r.p+=23
                    elif fid==1: r.p+=9
                    elif fid==2: r.p+=15
                    elif fid==3: r.p+=27
                    elif fid in (4,7): nc=r.u8(); r.p+=5*nc+19
                    elif fid==5: mx=r.u8(); my=r.u8(); r.p+=8+4*mx*my+5
                    elif fid==6: r.p+=80
            if fl&0x20: r.u8()
            recs.append((fl&15,ch,dp,m,cx))
        acts=[]
        while r.p<len(b):
            start=r.p; size=r.u16(); cond=r.u16()
            end=len(b) if size==0 else start+size
            acts.append((cond,b[r.p:end])); r.p=end
            if size==0: break
        W('BHH',7,cid,len(recs))
        for st,ch,dp,m,cx in recs: W('BHH',st,ch,dp); W('6f',*m); W('8h',*cx)
        W('H',len(acts))
        for cond,code in acts: W('HI',cond,len(code)); out.extend(code)
    elif t in (37,11,33): counts['text']+=1
    elif t in (20,36,21,35,6): 
        cid=struct.unpack_from('<H',b,0)[0]; W('BH',5,cid)
for i,n in exp.items():
    W('BH',4,i); S(n)
# root timeline: tags after last define? gather top-level placement tags
emit_timeline(6,0,tags)
W('B',0)
open('/home/claude/abobo/assets/scene.bin','wb').write(out)
print(len(out),counts)
