import sys, struct, collections
sys.path.insert(0,'.')
from swflib import *
tags=load('/tmp/abobo.swf')
def subtags(b):
    r=R(b,4); out=[]
    while r.p<len(b):
        h=r.u16(); t=h>>6; l=h&63
        if l==63: l=r.u32()
        out.append((t,b[r.p:r.p+l])); r.p+=l
        if t==0: break
    return out
acts=[]  # (where, bytes)
def scan(ts,where):
    for t,b in ts:
        if t==12: acts.append((where,b))
        if t==59: acts.append((where+'/init%d'%struct.unpack_from('<H',b,0)[0],b[2:]))
        if t==39:
            sid=struct.unpack_from('<H',b,0)[0]
            scan(subtags(b),f'sprite{sid}')
        if t in (26,70):
            pass
scan(tags,'main')
print('action blocks',len(acts),'bytes',sum(len(b) for _,b in acts))
big=sorted(acts,key=lambda x:-len(x[1]))[:15]
for w,b in big: print(w,len(b))
inner=collections.Counter(w.split('/')[0][:6] for w,b in acts)
print(inner.most_common(5))
# opcode histogram
op=collections.Counter()
for w,b in acts:
    p=0
    while p<len(b):
        c=b[p]; p+=1
        if c==0: break
        op[c]+=1
        if c>=0x80: l=struct.unpack_from('<H',b,p)[0]; p+=2+l
print(sorted(op.items(),key=lambda x:-x[1])[:60])
