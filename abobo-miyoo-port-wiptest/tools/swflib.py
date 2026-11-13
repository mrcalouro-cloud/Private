import zlib, struct
class R:
    def __init__(s,b,p=0): s.b=b; s.p=p; s.bit=0
    def u8(s): s.align(); v=s.b[s.p]; s.p+=1; return v
    def u16(s): s.align(); v=struct.unpack_from('<H',s.b,s.p)[0]; s.p+=2; return v
    def s16(s): s.align(); v=struct.unpack_from('<h',s.b,s.p)[0]; s.p+=2; return v
    def u32(s): s.align(); v=struct.unpack_from('<I',s.b,s.p)[0]; s.p+=4; return v
    def align(s):
        if s.bit: s.bit=0; s.p+=1
    def ub(s,n):
        v=0
        for _ in range(n):
            byte=s.b[s.p]; v=(v<<1)|((byte>>(7-s.bit))&1); s.bit+=1
            if s.bit==8: s.bit=0; s.p+=1
        return v
    def sb(s,n):
        v=s.ub(n)
        if n and v>>(n-1): v-=1<<n
        return v
    def cstr(s):
        s.align(); e=s.b.index(0,s.p); v=s.b[s.p:e].decode('utf8','replace'); s.p=e+1; return v
def load(path):
    d=open(path,'rb').read()
    body=zlib.decompress(d[8:]) if d[:3]==b'CWS' else d[8:]
    r=R(body); n=r.ub(5); [r.sb(n) for _ in range(4)]; r.align()
    r.p+=4
    tags=[]
    while r.p<len(body):
        h=r.u16(); t=h>>6; l=h&63
        if l==63: l=r.u32()
        tags.append((t,body[r.p:r.p+l])); r.p+=l
    return tags
