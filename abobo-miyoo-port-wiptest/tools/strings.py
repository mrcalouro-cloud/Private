import sys, struct, collections
sys.path.insert(0,'.')
from survey import acts
cnt=collections.Counter()
def walk(b):
    p=0; pool=[]
    while p<len(b):
        c=b[p]; p+=1
        if c==0: break
        if c>=0x80:
            l=struct.unpack_from('<H',b,p)[0]; d=b[p+2:p+2+l]; p+=2+l
            if c==0x88:
                n=struct.unpack_from('<H',d,0)[0]; q=2
                for _ in range(n):
                    e=d.index(0,q); cnt[d[q:e].decode('latin1')]+=1; q=e+1
            elif c==0x96:
                q=0
                while q<len(d):
                    t=d[q]; q+=1
                    if t==0: e=d.index(0,q); cnt[d[q:e].decode('latin1')]+=1; q=e+1
                    elif t in (1,4,5): q+=4 if t==1 else 1
                    elif t in (6,): q+=8
                    elif t in (7,): q+=4
                    elif t in (8,9): q+=1 if t==8 else 2
                    elif t in (2,3): pass
                    else: break
            elif c in (0x8E,0x9B):
                # nested function bodies are inline following; walk continues linearly
                pass
for w,b in acts: walk(b)
open('/home/claude/abobo/assets/meta/as_strings.txt','w').write('\n'.join(f'{n}\t{s}' for s,n in cnt.most_common()))
api="gotoAndPlay gotoAndStop play stop attachMovie duplicateMovieClip removeMovieClip hitTest getBounds createEmptyMovieClip swapDepths getNextHighestDepth localToGlobal globalToLocal Math Key Sound SharedObject Date Color BitmapData setInterval clearInterval Stage System Mouse Selection TextField XML LoadVars loadMovie loadVariables Array String Number Object Boolean getTimer random int parseInt parseFloat isNaN fscommand getURL ContextMenu MovieClipLoader Rectangle Point ColorTransform Matrix attachBitmap beginFill lineTo drawRect filters blendMode startDrag stopDrag setMask _quality _focusrect _global _level0 eval unloadMovie loadSound setTransform getRGB setRGB getBytesLoaded onEnterFrame onKeyDown onMouseDown attachAudio Camera setTimeout apply call prototype __proto__ registerClass".split()
cs=dict(cnt)
print('distinct strings',len(cs))
print({a:cs[a] for a in api if a in cs})
