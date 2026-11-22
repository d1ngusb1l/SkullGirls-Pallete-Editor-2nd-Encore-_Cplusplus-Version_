"""Prepare a Cerebella animation preview from a user's installed archives.
Usage: python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY
The output contains timeline metadata and the game's own compressed sprite layer
payloads. It is generated locally and must not be committed.

Supported: version-9 Cerebella FOIT frame tables, art banks 4/6 (art-pt) and 5
(main), normal body layers 0/1, root authored animation timelines, and repeated
timeline frames. Unsupported timeline payloads and malformed/invalid references
are rejected instead of substituted.
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

def frame_table(foit,main_art_size):
    if foit[4:12]!=b'VRSN\x09\0\0\0':raise ValueError('Expected version 9')
    for start in range(5,min(300011,len(foit)-41)):
        if foit[start-5]!=1 or foit[start+9]!=0 or not (foit[start+10]&1):continue
        c=Cursor(foit,start-4)
        try:
            count=c.num(4)
            if count==0 or count>=20000 or count>(len(foit)-start)//57:continue
            first=Cursor(foit,start+11)
            first_layer=layer(first)
            empty=not first_layer[2] and first_layer[4]==0
            if (not empty and not first_layer[2]) or first_layer[0]>149 or first_layer[1]>149 or first_layer[4]>main_art_size:continue
            frames=[]
            for _ in range(count):
                origin=struct.unpack('<ii',c.take(8));bank=c.num(1)
                c.take(c.num(1)*12);flags=c.num(1)
                layers=[layer(c) if flags&(1<<i) else None for i in range(8)]
                primary=struct.unpack('<ii',c.take(8))
                offset2=struct.unpack('<ii',c.take(8))
                offset4=struct.unpack('<ii',c.take(8))
                offset6=struct.unpack('<ii',c.take(8))
                offset7=struct.unpack('<ii',c.take(8))
                boxed_offsets=[None]*8
                boxed_offsets[0]=boxed_offsets[1]=primary
                boxed_offsets[2]=boxed_offsets[3]=offset2
                boxed_offsets[4]=boxed_offsets[5]=offset4
                boxed_offsets[6]=offset6;boxed_offsets[7]=offset7
                boxes=[]
                for _ in range(3):
                    n=c.num(2);boxes.append([struct.unpack('<iiii',c.take(16)) for _ in range(n)])
                frames.append({'origin':origin,'bank':bank,'layers':layers,'offsets':boxed_offsets,'boxes':boxes})
            return frames,c.pos
        except (ValueError,struct.error):
            continue
    raise ValueError('No valid version-9 Cerebella frame table')

def skip_expression(c):
    while True:
        a=c.num(1);b=c.num(1);d=c.num(1);flags=c.num(1)
        c.take(1+(a&1)+((b&1))*3+(d&1)) if False else None
        c.take(1+(4 if not (a&1) else 1)+(4 if not (b&1) else 1)+(4 if not (d&1) else 1))
        if not (flags&2):return

def skip_condition(c):
    c.take(4)
    count=c.num(2);flags=c.num(1)
    for i in range(count+(1 if flags&2 else 0)):
        c.take(3)
        c.take(2)
        step_flags=c.num(1)
        if step_flags&2:skip_expression(c)
        c.take(15)

def skip_hit(c):
    c.take(2);c.take(4);c.take(1);c.take(2);c.take(2);c.take(1);c.take(4);c.take(2);c.take(9)
    flags=c.num(1);c.take(1)
    for i in range(4):
        if flags&(1<<i):skip_expression(c)
    if flags&16:c.take(2)

def read_animation_ticks(c):
    ticks=None
    while True:
        first=c.num(1);second=c.num(1);third=c.num(1)
        if third&128:skip_expression(c)
        duration=c.num(2) if first&1 else 1
        if ticks is None:ticks=duration
        if third&64:c.take(1)
        payloads=c.num(1)
        if payloads&~7:raise ValueError('Invalid FOIT animation frame payload')
        if payloads&1:skip_hit(c)
        if payloads&2:
            while True:
                c.take(2);c.take(4);c.take(4)
                spawn_flags=c.num(1)
                if spawn_flags&2:skip_expression(c)
                if not c.num(1):break
        if payloads&4:c.take(4*c.num(1))
        if not c.num(1):return ticks
        skip_condition(c)

def read_move_table(c,file_size,sprite_count,read_children=False):
    c.take(1);count=c.num(2)
    if count>(file_size-c.pos)//10:raise ValueError('Truncated FOIT animation table')
    animations=[];names=[str(i) for i in range(count)]
    for i in range(count):
        flags=c.num(1)
        if flags&~15:raise ValueError('Invalid FOIT animation flags')
        if flags&12:c.take(4)
        c.take(1)
        if flags&2:c.take(4*c.num(1))
        transitions=c.num(2);successor=c.num(2)
        if successor>=count:raise ValueError('Invalid FOIT animation successor')
        frames=c.num(2)
        if frames>(file_size-c.pos)//11:raise ValueError('Truncated FOIT animation frames')
        animation=[]
        for _ in range(frames):
            sprites=[c.num(2) for _ in range(3)]
            if any(sprite>=sprite_count for sprite in sprites):raise ValueError('Invalid FOIT sprite reference')
            ticks=read_animation_ticks(c)
            animation.append({'sprites':sprites,'duration':ticks})
        if c.num(1):
            skip_condition(c);read_animation_ticks(c)
        if c.num(1):skip_expression(c)
        for _ in range(transitions):
            target=c.num(2)
            if target>=count:raise ValueError('Invalid FOIT animation link')
            skip_condition(c)
        animations.append(animation)
    if not read_children:
        return animations,names
    c.take(4)
    if c.num(1):c.take(2)
    c.take(18)
    children=c.num(4)
    if children>(file_size-c.pos)//12:raise ValueError('Invalid FOIT child table')
    for _ in range(children):
        data=c.num(4)
        if c.num(4)!=0x4e535256 or c.num(4)!=9 or data<c.pos:raise ValueError('Invalid FOIT child header')
        c.take(data-c.pos)
        name_length=c.num(1)
        c.take(name_length)
        c.take(4)
        if c.num(1):raise ValueError('Unsupported FOIT child entity data')
        c.take(5*c.num(1))
        c.take(0xb1*4)
        expressions=c.num(1)
        for bit in (1,2,4):
            if expressions&bit:skip_expression(c)
        if c.num(1):raise ValueError('FOIT child has its own sprite table')
        child=read_move_table(c,file_size,sprite_count)
    for i in range(count):
        length=c.num(1)
        names[i]=c.take(length).decode('utf-8','replace')
    return animations,names

def unpack(data,expected):
    c=Cursor(data);out=bytearray()
    while len(out)<expected:
        flag=c.num(1);code=flag&127
        length=code+1 if code<125 else max(1,int.from_bytes(c.take(code-124),'big'))
        if length>expected-len(out):raise ValueError('Oversized RLE')
        out.extend(c.take(1)*length if flag&128 else c.take(length))
    if c.pos!=len(data):raise ValueError('Trailing RLE')
    return out

def first_asset(folder,candidates,name):
    for archive in candidates:
        path=folder/archive
        if path.exists():
            try:return asset(folder,archive,name)
            except ValueError:pass
    raise ValueError('Missing local archive asset: '+name)

def resolve_art_bank(bank,art_banks):
    if bank not in (4,5,6):
        raise ValueError('Unsupported Cerebella art bank '+str(bank))
    art=art_banks.get(bank)
    if art is None:
        raise ValueError('Art bank '+str(bank)+' is unavailable')
    return art

def validate_timeline_frame(frame,records):
    for sprite in frame['sprites']:
        if not isinstance(sprite,int) or sprite<0 or sprite>=len(records):
            raise ValueError('Invalid FOIT sprite reference')
    sprite=frame.get('body_sprite',frame['sprites'][1])
    record=records[sprite]
    if record['bank'] not in (4,5,6):
        raise ValueError('Unsupported Cerebella art bank '+str(record['bank']))
    if not frame.get('duration',0)>=1:
        raise ValueError('Invalid authored frame duration')

def map_body_timeline(animation,fallback_sprite=2):
    pose=fallback_sprite
    result=[]
    for frame in animation:
        body_sprite=frame['sprites'][1]
        if body_sprite>1:
            pose=body_sprite
        mapped=dict(frame)
        mapped['body_sprite']=pose
        result.append(mapped)
    return result

def stored_layer(info,art):
    cols,rows,tiles,flags,offset,sizes=info
    if not tiles:return None
    if offset<0 or offset>len(art):raise ValueError('Sprite layer art offset is outside its bank')
    channels=[]
    for size in sizes:
        if size<=0 or size>len(art)-offset:raise ValueError('Sprite layer channel is outside its art bank')
        channels.append(art[offset:offset+size]);offset+=size
    return cols,rows,flags&64,tiles,channels

def main():
    if len(sys.argv)!=3:
        raise SystemExit('Usage: python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY')
    game,out=map(Path,sys.argv[1:]);folder=game/'data01'
    foit=first_asset(folder,['characters-foits.gfs','characters-foits-retail.gfs'],'characters/cerebella.foit')
    main_art=first_asset(folder,['characters-art.gfs','characters-art-retail.gfs'],'characters/cerebella.art')
    art_banks={5:main_art}
    try:
        pt_art=first_asset(folder,['characters-art-pt.gfs','characters-art-pt-retail.gfs'],'characters/cerebella.art-pt')
        art_banks.update({4:pt_art,6:pt_art})
    except ValueError:
        art_banks[6]=None
    dds=first_asset(folder,['sprites.gfs','sprites-retail.gfs'],'sprites/cerebella_1p.dds')
    if dds[:4]!=b'DDS ' or struct.unpack_from('<I',dds,16)[0]!=16 or len(dds)<128:
        raise ValueError('Unsupported palette DDS')
    palette=dds[128:]
    if len(palette)<64 or len(palette)>16384 or len(palette)%64:
        raise ValueError('Unsupported palette data')

    records,cursor=frame_table(foit,len(main_art))
    animations,names=read_move_table(Cursor(foit,cursor),len(foit),len(records))
    if len(names)!=len(animations):raise ValueError('Malformed animation names')
    used=set();supported_animations=[];unsupported_animations=[]
    for animation_id,raw_animation in enumerate(animations):
        animation=map_body_timeline(raw_animation)
        supported=True
        for frame in animation:
            sprite=frame['body_sprite']
            if sprite>=len(records):
                raise ValueError('Invalid FOIT body sprite reference '+str(sprite)+' in animation '+str(animation_id))
            record=records[sprite]
            if record['bank'] not in (4,5,6):
                supported=False
            has_body_layer=False
            for slot in (0,1):
                info=record['layers'][slot]
                if info is not None and info[2]:
                    if info[0]>128 or info[1]>128:
                        supported=False
                    else:
                        has_body_layer=True
            if not has_body_layer:
                supported=False
        if supported:
            supported_animations.append((animation_id,names[animation_id],animation))
            for frame in animation:
                used.add(frame['body_sprite'])
        else:
            unsupported_animations.append({'id':animation_id,'name':names[animation_id],'reason':'unsupported art bank or missing normal body layer 0/1'})
    animations=supported_animations
    if 2>=len(records):raise ValueError('Missing fixed-pose fallback sprite 2')
    used.add(2)

    decoded=[]
    for sprite_id in sorted(used):
        record=records[sprite_id];bank=record['bank']
        if bank not in (4,5,6):raise ValueError('Unsupported Cerebella art bank '+str(bank)+' at sprite '+str(sprite_id))
        art=art_banks.get(bank)
        if art is None:raise ValueError('Art bank '+str(bank)+' is required for sprite '+str(sprite_id)+' but is unavailable')
        layers=[]
        for slot,info in enumerate(record['layers']):
            if info is None:continue
            if slot not in (0,1):continue
            stored=stored_layer(info,art)
            if stored is None:continue
            cols,rows,flags,tiles,channels=stored
            lw,lh=cols*16,rows*16
            if lw>2048 or lh>2048 or lw*lh*3>16*1024*1024:raise ValueError('Oversized sprite layer')
            offset=record['offsets'][slot] if record['offsets'][slot] is not None else (0,0)
            layers.append((slot,offset[0],offset[1],cols,rows,flags,tiles,channels))
        if not layers:raise ValueError('Sprite '+str(sprite_id)+' has no body layers')
        decoded.append((sprite_id,record['origin'][0],record['origin'][1],layers))

    fallback_record_id=sorted(used).index(2)
    parts=[bytearray(struct.pack('<8s4I',b'SGPOSE5\0',len(palette),2,len(decoded),len(animations))),bytearray(palette)]
    for sprite_id,ox,oy,layers in decoded:
        parts.append(struct.pack('<IiiI',sprite_id,ox,oy,len(layers)))
        for slot,x,y,cols,rows,flags,tiles,channels in layers:
            parts.append(struct.pack('<IiiHHHHIII',slot,x,y,cols,rows,flags,len(tiles),*(len(data) for data in channels)))
            parts.append(struct.pack('<'+'H'*len(tiles),*tiles))
            parts.extend(channels)
    for animation_id,name_value,animation in animations:
        name=name_value.encode('utf-8','replace')
        if len(name)>1024:raise ValueError('Animation name exceeds supported size')
        parts.append(struct.pack('<II',animation_id,len(name))+name+struct.pack('<I',len(animation)))
        for frame in animation:
            if frame['duration']<1 or frame['duration']>65535:raise ValueError('Invalid authored frame duration')
            parts.append(struct.pack('<HHH H',frame['body_sprite'],frame['sprites'][1],frame['sprites'][2],frame['duration']))
    payload=b''.join(parts)
    if len(payload)>128*1024*1024:raise ValueError('Prepared preview exceeds 128 MiB explicit bound')
    out.mkdir(parents=True,exist_ok=True)
    (out/'cerebella-pose.bin').write_bytes(payload)
    meta={'format':'SGPOSE5','frame':2,'bankFallback':5,'fallbackSprite':2,'fallbackRecord':fallback_record_id,
          'spriteRecords':len(decoded),'animations':len(animations),'timelineFrames':sum(len(a[2]) for a in animations),
          'paletteBytes':len(palette),'sha256':hashlib.sha256(payload).hexdigest(),'foitSha256':hashlib.sha256(foit).hexdigest(),
          'supportedBanks':[4,5,6],'supportedLayers':[0,1],'unsupportedAnimations':unsupported_animations,
          'payload':'original FOIT RLE channels, decoded on selected frame','unsupportedEffects':'overlays, lighting, rainbow, redirects, transformations and super visuals','unsupportedOtherLayers':'2-7'}
    (out/'cerebella-pose.json').write_text(json.dumps(meta,indent=2))
    print(out/'cerebella-pose.json')
if __name__=='__main__':main()

