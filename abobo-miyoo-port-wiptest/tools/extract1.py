import sys, struct, zlib, os, collections, json
sys.path.insert(0,'.')
from swflib import *
from PIL import Image
tags=load('/tmp/abobo.swf')
out='/home/claude/abobo/assets'
for d in ('bitmaps','sounds','meta'): os.makedirs(f'{out}/{d}',exist_ok=True)
nb=0
for t,b in tags:
    if t in (20,36):
        cid,fmt,w,h=struct.unpack_from('<HBHH',b,0); p=7
        ncol=0
        if fmt==3: ncol=b[p]+1; p+=1
        raw=zlib.decompress(b[p:]); alpha=(t==36)
        if fmt==3:
            esz=4 if alpha else 3; pal=raw[:ncol*esz]; rs=(w+3)&~3
            img=Image.new('RGBA',(w,h)); px=img.load()
            for y in range(h):
                for x in range(w):
                    i=raw[ncol*esz+y*rs+x]
                    if i>=ncol: px[x,y]=(0,0,0,0); continue
                    if alpha: r,g,bb,a=pal[i*4:i*4+4]
                    else: r,g,bb=pal[i*3:i*3+3]; a=255
                    px[x,y]=(r,g,bb,a)
        elif fmt==5:
            import numpy as np
            a=np.frombuffer(raw,np.uint8).reshape(h,w,4)
            if alpha:  # ARGB premultiplied
                A=a[:,:,0].astype(float); rgb=a[:,:,1:4].astype(float)
                m=A>0; rgb[m]=np.minimum(255,rgb[m]*255/A[m][:,None])
                img=Image.fromarray(np.dstack([rgb.astype(np.uint8),A.astype(np.uint8)]),'RGBA')
            else: img=Image.fromarray(np.dstack([a[:,:,1:4],np.full((h,w),255,np.uint8)]),'RGBA')
        else: continue
        img.save(f'{out}/bitmaps/{cid}.png'); nb+=1
print('bitmaps',nb)
# exports
exp={}
for t,b in tags:
    if t==56:
        r=R(b); n=r.u16()
        for _ in range(n): i=r.u16(); exp[i]=r.cstr()
json.dump(exp,open(f'{out}/meta/exports.json','w'),indent=0)
print('exports',len(exp)); print(list(exp.items())[:40])
