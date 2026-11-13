import sys, struct, os, collections, wave, json
sys.path.insert(0,'.')
from swflib import *
tags=load('/tmp/abobo.swf')
out='/home/claude/abobo/assets/sounds'
exp=json.load(open('/home/claude/abobo/assets/meta/exports.json'))
fm=collections.Counter(); n=0
rates=[5512,11025,22050,44100]
for t,b in tags:
    if t!=14: continue
    sid=struct.unpack_from('<H',b,0)[0]; fl=b[2]
    f=fl>>4; rate=rates[(fl>>2)&3]; sz=(fl>>1)&1; st=fl&1
    cnt=struct.unpack_from('<I',b,3)[0]; data=b[7:]
    fm[(f,rate,sz,st)]+=1
    name=exp.get(str(sid),f'snd{sid}')
    if f==2: open(f'{out}/{sid}_{name}.mp3','wb').write(data[2:]); n+=1
    elif f in (0,3):
        w=wave.open(f'{out}/{sid}_{name}.wav','wb'); w.setnchannels(1+st); w.setsampwidth(2 if sz else 1); w.setframerate(rate)
        if f==0 and sz: # native-endian (assume LE)
            pass
        w.writeframes(data); w.close(); n+=1
print(n,fm)
