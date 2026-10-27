import fs from 'node:fs';import {decodePng} from './assets.mjs';
const root=process.argv[2]||'G:/My Drive/Artwork/Assets/8bitadventure/Enemies',bytes=[];
for(const dir of ['right','left'])for(const f of [1,2]) {
 const im=decodePng(fs.readFileSync(`${root}/shielder ${dir} (${f}).png`));
 for(let half=0;half<2;half++)for(let y=0;y<16;y++) {
  let lo=0,hi=0;
  for(let x=0;x<8;x++){const i=(y*im.width+half*8+x)*4;const c=!im.rgba[i+3]?0:im.rgba[i]>200&&im.rgba[i+1]>150?1:im.rgba[i]>180?2:3;lo=lo*2+(c&1);hi=hi*2+(c>>1);}bytes.push(lo,hi);
 }
}
fs.writeFileSync(new URL('../src/bulwark_art.inc',import.meta.url),'// Generated from 8bitadventure shielder sprites by tools/bulwark-art.mjs.\nstatic const uint8_t bulwark_art[]={\n'+Array.from({length:bytes.length/16},(_,i)=>' '+bytes.slice(i*16,i*16+16).join(',')).join(',\n')+'\n};\n');
