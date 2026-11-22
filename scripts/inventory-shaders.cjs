// Read-only inventory of the installed main game's GFS shader and effect assets.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const game=path.resolve(process.argv[2]||'.');
const out=path.resolve(process.argv[3]||'build/shader-inventory');fs.mkdirSync(out,{recursive:true});
const assets=[],textures=[],archiveStats=[];let paletteEffectsText;
for(const dataDir of fs.readdirSync(game).filter(n=>/^data\d+$/.test(n)).sort()) for(const archive of fs.readdirSync(path.join(game,dataDir)).filter(n=>n.endsWith('.gfs')).sort()){
 const archivePath=path.join(game,dataDir,archive),fd=fs.openSync(archivePath,'r');
 try{
  const header=Buffer.alloc(51);if(fs.readSync(fd,header,0,51,0)!==51)throw Error('Truncated header');
  const dataStart=header.readUInt32BE(),count=Number(header.readBigUInt64BE(43));
  if(dataStart<51||dataStart>64*1024*1024)throw Error('Invalid GFS metadata size');
  const metadata=Buffer.alloc(dataStart-51);fs.readSync(fd,metadata,0,metadata.length,51);
  let p=0,relative=0,found=0;
  for(let i=0;i<count;i++){
   const nameLength=Number(metadata.readBigUInt64BE(p));p+=8;
   const name=metadata.toString('utf8',p,p+nameLength);p+=nameLength;
   const size=Number(metadata.readBigUInt64BE(p));p+=12;
   if(/shader/i.test(name)||/\.(?:fx|glsl|hlsl|cg|vso|pso|fxo)$/i.test(name)){
    const bytes=Buffer.alloc(size);if(fs.readSync(fd,bytes,0,size,dataStart+relative)!==size)throw Error('Truncated shader');
    const ext=path.extname(name).toLowerCase(),source=['.fx','.glsl','.hlsl','.cg'].includes(ext);
    const item={archive:dataDir+'/'+archive,path:name,size,sha256:crypto.createHash('sha256').update(bytes).digest('hex'),format:source?'source':'binary'};
    if(source){
     const text=bytes.toString('utf8').replace(/\/\*[\s\S]*?\*\//g,'').replace(/\/\/[^\r\n]*/g,'');item.uniforms=[...text.matchAll(/^\s*(?:uniform\s+)?(?:row_major\s+)?(?:float[1-4]?(?:x[1-4])?|half[1-4]?(?:x[1-4])?|int[1-4]?|bool|sampler\w*|texture\w*)\s+(g\w+)\b/gm)].map(m=>m[1]);
     item.techniques=[...text.matchAll(/^\s*technique\w*\s+(\w+)/gm)].map(m=>m[1]);
     item.entryPoints=[...text.matchAll(/^\s*(?:float[1-4]?|half[1-4]?|FPOutput|VPOutput|VS_OUTPUT|PS_OUTPUT|void)\s+(\w+)\s*\(/gm)].map(m=>m[1]);
     // Metadata inventory only: do not redistribute game shader source.
    }
    assets.push(item);found++;
   }
   if(/(?:chowder|shaderfeature)/i.test(name)&&/\.dds$/i.test(name)){
    const bytes=Buffer.alloc(Math.min(size,128));fs.readSync(fd,bytes,0,bytes.length,dataStart+relative);
    textures.push({archive:dataDir+'/'+archive,path:name,size,width:bytes.length>=128?bytes.readUInt32LE(16):null,height:bytes.length>=128?bytes.readUInt32LE(12):null});
   }
   if(name==='characters/_palette_effects.ini'){
    const bytes=Buffer.alloc(size);fs.readSync(fd,bytes,0,size,dataStart+relative);paletteEffectsText=bytes.toString('utf8');fs.writeFileSync(path.join(out,'palette-effects.ini'),bytes);
   }
   relative+=size;
  }
  if(dataStart+relative!==fs.fstatSync(fd).size)throw Error('GFS payload size mismatch: '+archive);
  archiveStats.push({archive:dataDir+'/'+archive,entries:count,shaderAssets:found});
 }finally{fs.closeSync(fd);}
}
const exe=fs.readFileSync(path.join(game,'SkullGirls.exe'));
const exeHash=crypto.createHash('sha256').update(exe).digest('hex');
// D3D9 version, instruction-length, comment-size and END token definitions are
// taken from the installed Windows SDK shared/d3d9types.h. Require a complete
// token stream and a CTAB comment to reject incidental version-like constants.
const embedded=[];
for(let off=0;off+16<exe.length;off+=4){
 const version=exe.readUInt32LE(off),stage=version>>>16,major=(version>>>8)&255,minor=version&255;
 if(![0xfffe,0xffff].includes(stage)||![2,3].includes(major)||minor!==0)continue;
 let p=off+4,instructions=0,ctab,end;
 while(p+4<=Math.min(exe.length,off+65536)&&instructions<4096){
  const token=exe.readUInt32LE(p),op=token&0xffff;
  if(token===0xffff){end=p+4;break;}
  if(op===0xfffe){const n=(token>>>16)&0x7fff;if(!n||p+4+4*n>exe.length)break;const comment=exe.subarray(p+4,p+4+4*n);if(comment.toString('ascii',0,4)==='CTAB')ctab=comment;p+=4+4*n;continue;}
  if(op>96)break;
  const n=(token>>>24)&15;p+=4+4*n;instructions++;
 }
 if(!end||!ctab||!instructions)continue;
 const constants=[];let creator,target;
 try{
  const base=4,readString=offset=>{const start=base+offset;if(start<base||start>=ctab.length)throw Error('Invalid CTAB string');const end=ctab.indexOf(0,start);if(end<0)throw Error('Invalid CTAB termination');return ctab.toString('utf8',start,end);};
  creator=readString(ctab.readUInt32LE(base+4));target=readString(ctab.readUInt32LE(base+24));
  const count=ctab.readUInt32LE(base+12),start=base+ctab.readUInt32LE(base+16);if(count>1000||start+20*count>ctab.length)continue;
  for(let i=0;i<count;i++){const at=start+20*i;constants.push({name:readString(ctab.readUInt32LE(at)),registerSet:ctab.readUInt16LE(at+4),registerIndex:ctab.readUInt16LE(at+6),registerCount:ctab.readUInt16LE(at+8)});}
 }catch{continue;}
 embedded.push({fileOffset:'0x'+off.toString(16),stage:stage===0xfffe?'vertex':'pixel',model:`${major}.${minor}`,target,creator,byteLength:end-off,instructions,sha256:crypto.createHash('sha256').update(exe.subarray(off,end)).digest('hex'),constants});
 off=end-4;
}
const formats={};for(const a of assets){const ext=path.extname(a.path);formats[ext]=(formats[ext]||0)+1;}
const programs=new Map();for(const a of assets){const m=a.path.match(/\/(\w+)_(fp|vp)-([^.]+)\.(glsl|sb)$/);if(m){const key=m[1]+':'+m[3];if(!programs.has(key))programs.set(key,{family:m[1],variant:m[3],assets:[]});programs.get(key).assets.push(a.path);}}
const paletteEffectPresets=[];let preset;
for(const raw of (paletteEffectsText||'').split(/\r?\n/)){
 const line=raw.split('#')[0].trim();if(!line)continue;
 if(!preset){preset={name:line,frameRecords:0,noLight:false};continue;}
 if(line==='END'){paletteEffectPresets.push(preset);preset=undefined;continue;}
 if(line==='NoLight')preset.noLight=true;
 if(line.startsWith('[')||line==='NORMAL')preset.frameRecords++;
}
const report={date:new Date().toISOString().slice(0,10),scope:'Specified game root dataNN GFS archives and embedded D3D9 bytecode in SkullGirls.exe; excludes separate Salmon and SGBeta installs. Packaged and embedded availability does not establish that a shader is currently bound by the renderer.',executableSha256:exeHash,archiveStats,counts:{archives:archiveStats.length,shaderAssets:assets.length,uniqueShaderPaths:new Set(assets.map(x=>x.path)).size,uniqueShaderContents:new Set(assets.map(x=>x.sha256)).size,formats,chowderTextures:textures.length,namedPackagedProgramVariants:programs.size,embeddedD3D9Programs:embedded.length,uniqueEmbeddedBytecode:new Set(embedded.map(x=>x.sha256)).size,paletteEffectPresets:paletteEffectPresets.length},assets,textures,namedPrograms:[...programs.values()],embeddedD3D9Programs:embedded,paletteEffectPresets};
fs.writeFileSync(path.join(out,'inventory.json'),JSON.stringify(report,null,2));
const quote=value=>'"'+String(value??'').replaceAll('"','""')+'"';
const rows=[['archive','path','format','bytes','sha256','declared entry points and helpers','uniforms'],...assets.map(a=>[a.archive,a.path,a.format,a.size,a.sha256,(a.entryPoints||[]).join('; '),(a.uniforms||[]).join('; ')])];
fs.writeFileSync(path.join(out,'inventory.csv'),rows.map(r=>r.map(quote).join(',')).join('\r\n')+'\r\n');
console.log(JSON.stringify({counts:report.counts,archivesWithShaders:archiveStats.filter(x=>x.shaderAssets),namedPrograms:report.namedPrograms.map(x=>({family:x.family,variant:x.variant})),embedded:embedded.map(x=>({stage:x.stage,target:x.target,instructions:x.instructions,constants:x.constants.map(c=>c.name)})),textures},null,2));
