"""Prepare the fixed Cerebella frame from a user's installed archives (no game art in git).
Usage: python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY
Format investigation reference: Anonnie04/skullgirls-rough-cut, src/sprite_{rle,layer}.cpp.
This intentionally supports only the tested version-9 Cerebella frame 2, bank 5.
"""
import hashlib,json,struct,sys
from pathlib import Path

def asset(folder,archive,name):
    with (folder/archive).open('rb') as f:
        header=f.read(51)
        offset=int.from_bytes(header[:4],'big')
        for _ in range(int.from_bytes(header[43:51],'big')):
            length=int.from_bytes(f.read(8),'big')
            entry=f.read(length).decode('ascii')
            size=int.from_bytes(f.read(8),'big'); f.read(4)
            if entry==name:
                f.seek(offset);value=f.read(size)
                if len(value)!=size:raise ValueError('Truncated archive')
                return value
            offset+=size
    raise ValueError('Missing '+name)

class Cursor:
    def __init__(self,data,pos=0):self.data,self.pos=data,pos
    def take(self,n):
        if n<0 or self.pos+n>len(self.data):raise ValueError('Truncated FOIT')
        result=self.data[self.pos:self.pos+n];self.pos+=n;return result
    def num(self,n):return int.from_bytes(self.take(n),'little')

def layer(c):
    total,cols,rows,used=struct.unpack('<4H',c.take(8));flags=c.num(1)
    if total!=cols*rows or cols>149 or rows>149 or flags&40!=40:raise ValueError('Invalid tiles')
    if flags&4:c.take(8)
    mask=c.take((total+7)//8) if flags&16 else bytes((total+7)//8)
    occupied=[i for i in range(total) if mask[i//8]&(1<<(i%8))]
    if len(occupied)!=used:raise ValueError('Invalid mask')
    return cols,rows,occupied,flags,c.num(4),[c.num(4) for _ in range(3)]

def frames(foit):
    if foit[4:12]!=b'VRSN\x09\0\0\0':raise ValueError('Expected version 9')
    # This one supported game's frame table begins at this audited offset.
    c=Cursor(foit,35714);count=c.num(4)
    if count!=1445:raise ValueError('Unsupported Cerebella frame table')
    for frame in range(3):
        origin=struct.unpack('<ii',c.take(8));bank=c.num(1)
        c.take(c.num(1)*12);flags=c.num(1)
        layers=[layer(c) if flags&(1<<i) else None for i in range(8)]
        offsets=c.take(40)
        for _ in range(3):c.take(c.num(2)*16)
    if bank!=5:raise ValueError('Expected main art bank')
    return [x for x in layers[:2] if x and x[2]]

def unpack(data,expected):
    c=Cursor(data);out=bytearray()
    while len(out)<expected:
        flag=c.num(1);code=flag&127
        length=code+1 if code<125 else max(1,int.from_bytes(c.take(code-124),'big'))
        if length>expected-len(out):raise ValueError('Oversized RLE')
        out.extend(c.take(1)*length if flag&128 else c.take(length))
    if c.pos!=len(data):raise ValueError('Trailing RLE')
    return out

def pixels(info,art):
    cols,rows,tiles,flags,offset,sizes=info
    width,height=cols*16,rows*16
    result=bytearray(width*height*3);stride=len(tiles)*16
    for ch,size in enumerate(sizes):
        plane=unpack(art[offset:offset+size],stride*16);offset+=size
        if ch==0:
            for y in list(range(2,16,2))+[15]:
                prev=y-2 if y!=15 else 14
                for x in range(stride):plane[y*stride+x]=(plane[y*stride+x]+plane[prev*stride+x])&255
        if ch==0 or (ch==1 and flags&64):
            for y in range(1,15,2):
                for x in range(stride):
                    a,b=plane[(y-1)*stride+x],plane[(y+1)*stride+x]
                    plane[y*stride+x]=(((a>>4)+(b>>4))//2*16+((a&15)+(b&15))//2) if ch==0 else (plane[y*stride+x]&224)|(((a&31)+(b&31))//2)
        for n,tile in enumerate(tiles):
            for y in range(16):
                for x in range(16):result[(((tile//cols)*16+y)*width+(tile%cols)*16+x)*3+ch]=plane[y*stride+n*16+x]
    return width,height,result

def main():
    game,out=map(Path,sys.argv[1:]);folder=game/'data01'
    foit=asset(folder,'characters-foits.gfs','characters/cerebella.foit')
    art=asset(folder,'characters-art.gfs','characters/cerebella.art')
    dds=asset(folder,'sprites.gfs','sprites/cerebella_1p.dds')
    if dds[:4]!=b'DDS ' or struct.unpack_from('<I',dds,16)[0]!=16:raise ValueError('Unsupported palette')
    palette=dds[128:];decoded=[pixels(x,art) for x in frames(foit)]
    w,h,_=decoded[0]
    if any((a,b)!=(w,h) for a,b,_ in decoded):raise ValueError('Unequal layers')
    out.mkdir(parents=True,exist_ok=True)
    payload=struct.pack('<8s4I',b'SGPOSE1\0',w,h,len(decoded),len(palette))+palette+b''.join(x[2] for x in decoded)
    (out/'cerebella-pose.bin').write_bytes(payload)
    (out/'cerebella-pose.json').write_text(json.dumps({'frame':2,'bank':5,'width':w,'height':h,'layers':len(decoded),'sha256':hashlib.sha256(payload).hexdigest(),'foitSha256':hashlib.sha256(foit).hexdigest()},indent=2))
    print(out/'cerebella-pose.json')
if __name__=='__main__':main()

