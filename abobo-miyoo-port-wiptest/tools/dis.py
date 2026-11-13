import sys,struct
sys.path.insert(0,'.')
OPS={0x04:'NextFrame',0x05:'PrevFrame',0x06:'Play',0x07:'Stop',0x0a:'Add',0x0b:'Sub',0x0c:'Mul',0x0d:'Div',0x0e:'Eq',0x0f:'Lt',0x10:'And',0x11:'Or',0x12:'Not',0x17:'Pop',0x1c:'GetVar',0x1d:'SetVar',0x21:'StrAdd',0x22:'GetProp',0x23:'SetProp',0x26:'Trace',0x30:'Random',0x34:'GetTime',0x3a:'Delete',0x3c:'DefLocal',0x3d:'CallFn',0x3e:'Return',0x3f:'Mod',0x40:'NewObj',0x41:'DefLocal2',0x42:'InitArr',0x43:'InitObj',0x44:'TypeOf',0x47:'Add2',0x48:'Lt2',0x49:'Eq2',0x4a:'ToNum',0x4b:'ToStr',0x4c:'Dup',0x4d:'Swap',0x4e:'GetMem',0x4f:'SetMem',0x50:'Inc',0x51:'Dec',0x52:'CallMeth',0x53:'NewMeth',0x60:'BitAnd',0x61:'BitOr',0x66:'StrictEq',0x67:'Gt',0x69:'Extends'}
def pushvals(d,pool):
    out=[];q=0
    while q<len(d):
        t=d[q];q+=1
        if t==0: e=d.index(0,q); out.append(repr(d[q:e].decode('latin1'))); q=e+1
        elif t==1: out.append(repr(round(struct.unpack_from('<f',d,q)[0],3))); q+=4
        elif t==2: out.append('null')
        elif t==3: out.append('undef')
        elif t==4: out.append('r%d'%d[q]); q+=1
        elif t==5: out.append('true' if d[q] else 'false'); q+=1
        elif t==6: hi,lo=struct.unpack_from('<II',d,q); out.append(repr(struct.unpack('<d',struct.pack('<II',lo,hi))[0])); q+=8
        elif t==7: out.append(str(struct.unpack_from('<i',d,q)[0])); q+=4
        elif t==8: out.append(repr(pool[d[q]]) if d[q]<len(pool) else '?'); q+=1
        elif t==9: out.append(repr(pool[struct.unpack_from('<H',d,q)[0]])); q+=2
    return out
def dis(b,pool=None,base=0,indent=0):
    pool=pool or []; p=0; out=[]
    while p<len(b):
        op=b[p]; pos=p; p+=1
        if op==0: break
        if op>=0x80:
            l=struct.unpack_from('<H',b,p)[0]; d=b[p+2:p+2+l]; p+=2+l
            if op==0x88:
                k=struct.unpack_from('<H',d,0)[0]; q=2; pool=[]
                for _ in range(k):
                    e=d.index(0,q); pool.append(d[q:e].decode('latin1')); q=e+1
                out.append(' '*indent+'%d: ConstPool %d'%(pos,k))
            elif op==0x96: out.append(' '*indent+'%d: Push %s'%(pos,', '.join(pushvals(d,pool))))
            elif op in (0x8e,0x9b):
                q=0; e=d.index(0,q); name=d[q:e].decode(); q=e+1; np=struct.unpack_from('<H',d,q)[0]; q+=2
                if op==0x8e: nr=d[q]; fl=struct.unpack_from('<H',d,q+1)[0]; q+=3
                params=[]
                for _ in range(np):
                    if op==0x8e: r=d[q]; q+=1
                    e=d.index(0,q); params.append(d[q:e].decode()); q=e+1
                bl=struct.unpack_from('<H',d,q)[0]; body=b[p:p+bl]; p+=bl
                out.append(' '*indent+'%d: Function %s(%s) {'%(pos,name,','.join(params)))
                out+=dis(body,pool,0,indent+2); out.append(' '*indent+'}')
            elif op in (0x9d,0x99): out.append(' '*indent+'%d: %s %+d -> %d'%(pos,'If' if op==0x9d else 'Jump',struct.unpack_from('<h',d,0)[0],p+struct.unpack_from('<h',d,0)[0]))
            elif op==0x87: out.append(' '*indent+'%d: StoreReg %d'%(pos,d[0]))
            elif op==0x8c: out.append(' '*indent+'%d: GotoLabel %r'%(pos,d[:-1].decode()))
            elif op==0x81: out.append(' '*indent+'%d: GotoFrame %d'%(pos,struct.unpack_from('<H',d,0)[0]))
            elif op==0x9f: out.append(' '*indent+'%d: GotoFrame2 flags=%d'%(pos,d[0]))
            else: out.append(' '*indent+'%d: op%02x'%(pos,op))
        else: out.append(' '*indent+'%d: %s'%(pos,OPS.get(op,'op%02x'%op)))
    return out
