"""Prepare a combined animation preview from a user's installed archives.
Usage: python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY
The output contains timeline metadata and the game's own compressed sprite layer
payloads. It is generated locally and must not be committed.

Supported: version-9 playable-character FOIT frame tables, art banks 4/6
(art-pt) and 5 (main), normal body layers 0/1, authored root animation names,
and repeated timeline frames. Unsupported timelines and malformed references
are reported instead of substituted.
"""
import hashlib,json,os,struct,sys,tempfile
from pathlib import Path

PLAYABLE_CHARACTERS = (
    ("Filia", "filia"), ("Cerebella", "cerebella"), ("Peacock", "peacock"),
    ("Parasoul", "parasoul"), ("MsFortune", "msfortune"), ("Painwheel", "painwheel"),
    ("Valentine", "valentine"), ("Double", "double"), ("Squigly", "squigly"),
    ("BigBand", "bigband"), ("Eliza", "eliza"), ("Fukua", "fukua"),
    ("Beowulf", "beowulf"), ("RoboFortune", "robofortune"), ("Annie", "annie"),
    ("Umbrella", "umbrella"), ("BlackDahlia", "blackdahlia"), ("Marie", "marie"),
)
MAX_CHARACTER_ASSET = 128 * 1024 * 1024
MAX_BUNDLE = 4 * 1024 * 1024 * 1024

def asset(folder,archive,name):
    with (folder/archive).open('rb') as f:
        file_size=f.seek(0,2);f.seek(0)
        header=f.read(51)
        if len(header)!=51:raise ValueError('Truncated GFS header')
        data_offset=int.from_bytes(header[:4],'big')
        count=int.from_bytes(header[43:51],'big')
        if data_offset<51 or data_offset>file_size or count>100000:raise ValueError('Invalid GFS header bounds')
        offset=data_offset;selected=None;entry_names=set()
        for _ in range(count):
            raw_length=f.read(8)
            if len(raw_length)!=8:raise ValueError('Truncated GFS entry length')
            length=int.from_bytes(raw_length,'big')
            if not 1<=length<=4096:raise ValueError('Invalid GFS entry name length')
            raw_name=f.read(length);raw_size=f.read(8);reserved=f.read(4)
            if len(raw_name)!=length or len(raw_size)!=8 or len(reserved)!=4:raise ValueError('Truncated GFS entry')
            try:entry=raw_name.decode('ascii')
            except UnicodeDecodeError as error:raise ValueError('Invalid GFS entry name encoding') from error
            if entry in entry_names:raise ValueError('Duplicate GFS entry name')
            entry_names.add(entry)
            size=int.from_bytes(raw_size,'big')
            if offset>file_size or size>file_size-offset:raise ValueError('GFS asset exceeds archive bounds')
            if entry==name:selected=(offset,size)
            offset+=size
        if f.tell()!=data_offset:raise ValueError('GFS entry table does not match its asset offset')
        if selected is not None:
            asset_offset,size=selected
            if size>512*1024*1024:raise ValueError('GFS asset exceeds preparation bounds')
            f.seek(asset_offset);value=f.read(size)
            if len(value)!=size:raise ValueError('Truncated GFS asset')
            return value
    raise ValueError('Missing '+name)

def sha256_file(path):
    digest=hashlib.sha256()
    with path.open('rb') as source:
        while True:
            block=source.read(1024*1024)
            if not block:break
            digest.update(block)
    return digest.hexdigest()

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
    raise ValueError('No valid version-9 character frame table')

def read_expression(c):
    expressions=[]
    while True:
        a,b,d,flags=[c.num(1) for _ in range(4)]
        opcode=c.num(1)
        operands=[(kind,c.num(1 if kind&1 else 4)) for kind in (a,b,d)]
        expressions.append({'opcode':opcode,'comparison':bool(flags&1),
                            'left':operands[0],'right':operands[1],'destination':operands[2]})
        if not flags&2:return expressions

def skip_expression(c):
    read_expression(c)

def advances_immediately(expressions):
    # Native variable 0x3a asks the move updater to advance within this tick.
    # Only unconditional constant assignment is evaluated; do not guess live state.
    if any(expression["comparison"] for expression in expressions):return False
    result=False
    for expression in expressions:
        if expression['comparison'] or expression['destination']!=(1,0x3a):continue
        kind,bits=expression['left']
        if expression['opcode']==1 and kind in (0,0x24):
            value=struct.unpack('<f',bits.to_bytes(4,'little'))[0]
            result=value!=0
        else:
            result=False
    return result

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

def read_animation_ticks(c,details=None):
    ticks=None
    while True:
        first=c.num(1);second=c.num(1);third=c.num(1)
        expressions=read_expression(c) if third&128 else []
        duration=c.num(2) if first&1 else 1
        if ticks is None:
            ticks=duration
            if details is not None:details["advance_immediately"]=advances_immediately(expressions)
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

def read_move_table(c,file_size,sprite_count,read_children=False,validate_sprite_references=True,move_metadata=None):
    c.take(1);count=c.num(2)
    if count>(file_size-c.pos)//10:raise ValueError('Truncated FOIT animation table')
    animations=[];names=[str(i) for i in range(count)]
    for i in range(count):
        flags=c.num(1)
        if flags&~15:raise ValueError('Invalid FOIT animation flags')
        if flags&12:c.take(4)
        c.take(1)
        tags=[c.num(4) for _ in range(c.num(1))] if flags&2 else []
        transitions=c.num(2);successor=c.num(2)
        if successor>=count:raise ValueError('Invalid FOIT animation successor')
        frames=c.num(2)
        if frames>(file_size-c.pos)//11:raise ValueError('Truncated FOIT animation frames')
        animation=[]
        for _ in range(frames):
            sprites=[c.num(2) for _ in range(3)]
            if validate_sprite_references and any(sprite>sprite_count for sprite in sprites):raise ValueError('Invalid FOIT sprite reference')
            details={}
            ticks=read_animation_ticks(c,details)
            animation.append({'sprites':sprites,'duration':ticks,**details})
        if c.num(1):
            skip_condition(c);read_animation_ticks(c)
        if c.num(1):skip_expression(c)
        targets=[]
        for _ in range(transitions):
            target=c.num(2)
            if target>=count:raise ValueError('Invalid FOIT animation link')
            targets.append(target)
            skip_condition(c)
        animations.append(animation)
        if move_metadata is not None:
            move_metadata.append({'id':i,'tags':tags,'successor':successor,
                                  'conditionalTargets':targets,'sourceFrames':frames})
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
        try:c.take(name_length).decode('utf-8')
        except UnicodeDecodeError as error:raise ValueError('Malformed FOIT child name encoding') from error
        c.take(4)
        if c.num(1):raise ValueError('Unsupported FOIT child entity data')
        c.take(5*c.num(1))
        c.take(0xb1*4)
        expressions=c.num(1)
        for bit in (1,2,4):
            if expressions&bit:skip_expression(c)
        if c.num(1):raise ValueError('FOIT child has its own sprite table')
        read_move_table(c,file_size,65536,True,False)
    for i in range(count):
        length=c.num(1)
        try:names[i]=c.take(length).decode('utf-8')
        except UnicodeDecodeError as error:raise ValueError('Malformed authored animation name encoding') from error
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
        raise ValueError('Unsupported character art bank '+str(bank))
    art=art_banks.get(bank)
    if art is None:
        raise ValueError('Art bank '+str(bank)+' is unavailable')
    return art

def validate_timeline_frame(frame,records):
    for sprite in frame['sprites']:
        if not isinstance(sprite,int) or sprite<0 or sprite>len(records):
            raise ValueError('Invalid FOIT sprite reference')
    sprite=frame.get('body_sprite',max(0,frame['sprites'][1]-1))
    record=records[sprite]
    if record['bank'] not in (4,5,6):
        raise ValueError('Unsupported character art bank '+str(record['bank']))
    if not frame.get('duration',0)>=1:
        raise ValueError('Invalid authored frame duration')

def map_body_timeline(animation,fallback_sprite=1):
    pose=fallback_sprite
    result=[]
    for index,frame in enumerate(animation):
        reference=frame['sprites'][1]
        if reference>0:
            pose=reference-1  # FOIT references are one-based; zero retains the prior pose.
        if frame.get('advance_immediately',False):continue
        mapped=dict(frame)
        mapped['body_sprite']=pose
        mapped['source_record']=index
        result.append(mapped)
    return result

def authored_animation_name(animation_id,name):
    return name.strip() or str(animation_id)

def stored_layer(info,art):
    cols,rows,tiles,flags,offset,sizes=info
    if not tiles:return None
    if offset<0 or offset>len(art):raise ValueError('Sprite layer art offset is outside its bank')
    channels=[]
    for size in sizes:
        if size<=0 or size>len(art)-offset:raise ValueError('Sprite layer channel is outside its art bank')
        channels.append(art[offset:offset+size]);offset+=size
    return cols,rows,flags&64,tiles,channels

def write_section_bytes(stream,data):
    stream.write(data)
    if stream.tell()>MAX_CHARACTER_ASSET:raise ValueError('Prepared character section exceeds 128 MiB')

def frame_support_reason(record,art_banks):
    bank=record['bank']
    if bank not in (4,5,6):return 'unsupported art bank '+str(bank)
    if bank not in art_banks or art_banks[bank] is None:return 'required art bank '+str(bank)+' is unavailable'
    body_layers=[(slot,info) for slot,info in enumerate(record['layers'][:2])
                 if info is not None and info[2]]
    if not body_layers:return 'has no normal body layer 0/1'
    min_x=min_y=max_x=max_y=None
    for slot,info in body_layers:
        cols,rows=info[:2]
        if cols>128 or rows>128:return 'exceeds the normal body layer dimension limit'
        offset=record['offsets'][slot] if record['offsets'][slot] is not None else (0,0)
        left=offset[0]-record['origin'][0];top=offset[1]-record['origin'][1]
        right=left+cols*16;bottom=top+rows*16
        if min(left,top)<-8192 or max(right,bottom)>8192:return 'has an out-of-bounds layer origin'
        min_x=left if min_x is None else min(min_x,left)
        min_y=top if min_y is None else min(min_y,top)
        max_x=right if max_x is None else max(max_x,right)
        max_y=bottom if max_y is None else max(max_y,bottom)
    if max_x-min_x>2048 or max_y-min_y>2048:return 'exceeds the composed preview dimensions'
    return None

def build_character_section(folder,character,output_path):
    foit_name='characters/'+character+'.foit'
    art_name='characters/'+character+'.art'
    foit=first_asset(folder,['characters-foits.gfs','characters-foits-retail.gfs'],foit_name)
    main_art=first_asset(folder,['characters-art.gfs','characters-art-retail.gfs'],art_name)
    art_banks={5:main_art}
    try:
        pt_art=first_asset(folder,['characters-art-pt.gfs','characters-art-pt-retail.gfs'],'characters/'+character+'.art-pt')
        art_banks.update({4:pt_art,6:pt_art})
    except ValueError:
        art_banks.update({4:None,6:None})
    dds=first_asset(folder,['sprites.gfs','sprites-retail.gfs'],'sprites/'+character+'_1p.dds')
    if len(dds)<128 or dds[:4]!=b'DDS ' or struct.unpack_from('<I',dds,16)[0]!=16:
        raise ValueError('Unsupported palette DDS for '+character)
    palette=dds[128:]
    if len(palette)<64 or len(palette)>16384 or len(palette)%64:
        raise ValueError('Unsupported palette data for '+character)

    records,cursor=frame_table(foit,len(main_art))
    raw_animations,names=read_move_table(Cursor(foit,cursor),len(foit),len(records),True)
    if len(names)!=len(raw_animations):raise ValueError('Malformed animation names for '+character)

    fallback=1 if character=='cerebella' and len(records)>1 and frame_support_reason(records[1],art_banks) is None else None
    preferred=sorted(range(len(raw_animations)),key=lambda i:(
        0 if any(word in names[i].lower() for word in ('idle','stand','wait')) else 1,i))
    if fallback is None:
        for animation_id in preferred:
            for frame in raw_animations[animation_id]:
                sprite=frame['sprites'][1]-1
                if 0<=sprite<len(records) and frame_support_reason(records[sprite],art_banks) is None:
                    fallback=sprite
                    break
            if fallback is not None:break
    if fallback is None:
        fallback=next((i for i,record in enumerate(records) if frame_support_reason(record,art_banks) is None),None)
    if fallback is None:raise ValueError('No supported normal body layer found for '+character)

    used={fallback};animations=[];unsupported=[];animations_with_extra_layers=[]
    for animation_id,raw_animation in enumerate(raw_animations):
        if not raw_animation:
            unsupported.append({'id':animation_id,'name':names[animation_id],'reason':'empty timeline'})
            continue
        animation=map_body_timeline(raw_animation,fallback)
        if not animation:
            unsupported.append({"id":animation_id,"name":names[animation_id],"reason":"no timed records after same-tick commands"})
            continue
        reason=None;extra_layer_frames={}
        for frame in animation:
            sprite=frame['body_sprite']
            if sprite<0 or sprite>=len(records):
                raise ValueError('Invalid sprite reference in '+character+' animation '+str(animation_id))
            record=records[sprite]
            extra_slots=[slot for slot,info in enumerate(record['layers'][2:],2) if info is not None and info[2]]
            if extra_slots:extra_layer_frames.setdefault(sprite,set()).update(extra_slots)
            problem=frame_support_reason(record,art_banks)
            if problem:
                reason='sprite record '+str(sprite)+' '+problem
                break
            used.add(sprite)
        if reason:
            unsupported.append({'id':animation_id,'name':names[animation_id],'reason':reason})
        else:
            name=authored_animation_name(animation_id,names[animation_id])
            animations.append((animation_id,name,animation))
            if extra_layer_frames:
                animations_with_extra_layers.append({
                    'id':animation_id,'name':names[animation_id],
                    'spriteLayers':[{'sprite':sprite,'layers':sorted(slots)} for sprite,slots in sorted(extra_layer_frames.items())],
                })

    decoded=[]
    for sprite_id in sorted(used):
        record=records[sprite_id];art=art_banks.get(record['bank'])
        if art is None:raise ValueError('Required art bank is unavailable for '+character+' sprite '+str(sprite_id))
        layers=[]
        for slot,info in enumerate(record['layers'][:2]):
            if info is None or not info[2]:continue
            stored=stored_layer(info,art)
            if stored is None:continue
            cols,rows,flags,tiles,channels=stored
            lw,lh=cols*16,rows*16
            if lw>2048 or lh>2048 or lw*lh*3>16*1024*1024:raise ValueError('Oversized sprite layer')
            offset=record['offsets'][slot] if record['offsets'][slot] is not None else (0,0)
            layers.append((slot,offset[0],offset[1],cols,rows,flags,tiles,channels))
        if not layers:raise ValueError('Sprite '+str(sprite_id)+' has no supported normal body layer')
        decoded.append((sprite_id,record['origin'][0],record['origin'][1],layers))

    with output_path.open('wb') as out:
        write_section_bytes(out,struct.pack('<8s4I',b'SGPOSE6\0',len(palette),fallback,len(decoded),len(animations)))
        write_section_bytes(out,palette)
        for sprite_id,ox,oy,layers in decoded:
            write_section_bytes(out,struct.pack('<IiiI',sprite_id,ox,oy,len(layers)))
            for slot,x,y,cols,rows,flags,tiles,channels in layers:
                write_section_bytes(out,struct.pack('<IiiHHHHIII',slot,x,y,cols,rows,flags,len(tiles),*(len(data) for data in channels)))
                write_section_bytes(out,struct.pack('<'+'H'*len(tiles),*tiles))
                for channel in channels:write_section_bytes(out,channel)
        for animation_id,name_value,animation in animations:
            name=name_value.encode('utf-8','replace')
            if not name or len(name)>1024:raise ValueError('Animation name exceeds supported size')
            write_section_bytes(out,struct.pack('<II',animation_id,len(name))+name+struct.pack('<I',len(animation)))
            for frame in animation:
                if frame['duration']<1 or frame['duration']>65535:raise ValueError('Invalid authored frame duration')
                write_section_bytes(out,struct.pack('<HHHH',frame['body_sprite'],frame['sprites'][1],frame['sprites'][2],frame['duration']))
        out.flush()
        if not out:raise ValueError('Could not write character preview section')
        size=out.tell()
    if size>MAX_CHARACTER_ASSET:raise ValueError('Prepared '+character+' section exceeds 128 MiB')
    return {
        'character':character,'fallbackSprite':fallback,'spriteRecords':len(decoded),
        'animations':len(animations),'timelineFrames':sum(len(item[2]) for item in animations),
        'paletteBytes':len(palette),'bytes':size,
        'sha256':sha256_file(output_path),
        'foitSha256':hashlib.sha256(foit).hexdigest(),
        'timelineMapping':'one-based references; unconditional constant advance variable 0x3a consumes no tick',
        'sameTickRecordsOmitted':sum(bool(frame.get('advance_immediately')) for animation in raw_animations for frame in animation),
        'unsupportedAnimations':unsupported,
        'animationsWithAdditionalUnpreviewedLayers':animations_with_extra_layers,
    }

def write_bundle_index(character_sections):
    if not 1<=len(character_sections)<=18:raise ValueError('Combined preview must contain 1 to 18 characters')
    header_size=16+sum(2+len(name.encode('utf-8'))+16 for name,_path,_meta in character_sections)
    offset=header_size
    entries=[]
    names=set()
    for name,path,meta in character_sections:
        name_bytes=name.encode('utf-8')
        if not name_bytes or len(name_bytes)>64 or name.casefold() in names:raise ValueError('Invalid or duplicate character name in preview bundle')
        names.add(name.casefold())
        size=path.stat().st_size
        if size<24 or size>MAX_CHARACTER_ASSET:raise ValueError('Invalid character section size')
        entries.append(struct.pack('<H',len(name_bytes))+name_bytes+struct.pack('<QQ',offset,size))
        offset+=size
    if offset>MAX_BUNDLE:raise ValueError('Combined preview asset exceeds 4 GiB')
    return struct.pack('<8sII',b'SGMULT1\0',1,len(entries))+b''.join(entries)

def read_bundle_index(stream,file_size):
    if file_size>MAX_BUNDLE:raise ValueError('Combined preview asset exceeds 4 GiB')
    stream.seek(0);header=stream.read(16)
    if len(header)!=16:raise ValueError('Truncated combined preview index')
    magic,version,count=struct.unpack('<8sII',header)
    if magic!=b'SGMULT1\0' or version!=1 or not 1<=count<=18:raise ValueError('Unsupported combined preview index')
    sections=[];names=set()
    for _ in range(count):
        length_bytes=stream.read(2)
        if len(length_bytes)!=2:raise ValueError('Truncated combined preview name length')
        name_length=struct.unpack('<H',length_bytes)[0]
        if not 1<=name_length<=64:raise ValueError('Invalid combined preview character name')
        name_bytes=stream.read(name_length)
        entry=stream.read(16)
        if len(name_bytes)!=name_length or len(entry)!=16:raise ValueError('Truncated combined preview index entry')
        try:name=name_bytes.decode('utf-8')
        except UnicodeDecodeError as error:raise ValueError('Invalid combined preview character name encoding') from error
        offset,size=struct.unpack('<QQ',entry)
        if name.casefold() in names or size<24 or size>MAX_CHARACTER_ASSET:
            raise ValueError('Invalid or duplicate combined preview section')
        names.add(name.casefold());sections.append((name,offset,size))
    end=stream.tell()
    for _name,offset,size in sorted(sections,key=lambda section:section[1]):
        if offset!=end or size>file_size-offset:raise ValueError('Invalid combined preview section bounds')
        end=offset+size
    if end!=file_size:raise ValueError('Truncated or trailing combined preview data')
    return sections

def main():
    if len(sys.argv)!=3:
        raise SystemExit('Usage: python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY')
    game,out=map(Path,sys.argv[1:]);folder=game/'data01'
    if not folder.is_dir():raise ValueError('GAME_DIRECTORY must contain a data01 folder')
    out.mkdir(parents=True,exist_ok=True)
    metadata={'format':'SGMULT1','characters':[],'supportedBanks':[4,5,6],
              'supportedLayers':[0,1],
              'payload':'local version-9 FOIT timelines and original RLE sprite channels',
              'unsupportedEffects':'overlays, lighting, rainbow, redirects, transformations and super visuals'}
    with tempfile.TemporaryDirectory(prefix='sg-preview-') as temporary:
        root=Path(temporary);sections=[]
        for display_name,asset_name in PLAYABLE_CHARACTERS:
            section=root/(asset_name+'.bin')
            info=build_character_section(folder,asset_name,section)
            info['character']=display_name
            sections.append((display_name,section,info))
            metadata['characters'].append(info)
        bundle_index=write_bundle_index(sections)
        handle,temporary_name=tempfile.mkstemp(prefix='.sprite-preview-',suffix='.tmp',dir=out)
        os.close(handle)
        temporary_output=Path(temporary_name)
        try:
            with temporary_output.open('wb') as combined:
                combined.write(bundle_index)
                for _name,path,_info in sections:
                    with path.open('rb') as source:
                        while True:
                            block=source.read(1024*1024)
                            if not block:break
                            combined.write(block)
                combined.flush()
                os.fsync(combined.fileno())
            if temporary_output.stat().st_size>MAX_BUNDLE:raise ValueError('Combined preview asset exceeds 4 GiB')
            os.replace(str(temporary_output),str(out/'sprite-preview.bin'))
        finally:
            if temporary_output.exists():temporary_output.unlink()
    with (out/'sprite-preview.bin').open('rb') as combined:
        read_bundle_index(combined,(out/'sprite-preview.bin').stat().st_size)
    metadata['sha256']=sha256_file(out/'sprite-preview.bin')
    handle,metadata_name=tempfile.mkstemp(prefix='.sprite-preview-meta-',suffix='.tmp',dir=out)
    metadata_temp=Path(metadata_name)
    try:
        with os.fdopen(handle,'w',encoding='utf-8') as target:
            json.dump(metadata,target,indent=2)
        os.replace(str(metadata_temp),str(out/'sprite-preview.json'))
    finally:
        if metadata_temp.exists():metadata_temp.unlink()
    print('Prepared local preview for %d playable characters: %s'%(len(sections),out/'sprite-preview.bin'))
if __name__=='__main__':main()
