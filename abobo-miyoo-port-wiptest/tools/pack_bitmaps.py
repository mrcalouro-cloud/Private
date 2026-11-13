import os, struct, zlib
from PIL import Image
src='/home/claude/abobo/assets/bitmaps'
ids=sorted(int(f[:-4]) for f in os.listdir(src))
blobs=[]; 
for i in ids:
    im=Image.open(f'{src}/{i}.png').convert('RGBA')
    blobs.append((i,im.size[0],im.size[1],zlib.compress(im.tobytes(),6)))
hdr=8+len(blobs)*20
off=hdr; idx=b''
for i,w,h,c in blobs:
    idx+=struct.pack('<HHIIII',i,0,w,h,off,len(c)); off+=len(c)
with open('/home/claude/abobo/assets/bitmaps.pak','wb') as f:
    f.write(b'APAK'+struct.pack('<I',len(blobs))); f.write(idx)
    for b in blobs: f.write(b[3])
print(len(blobs),off/1e6,'MB')
