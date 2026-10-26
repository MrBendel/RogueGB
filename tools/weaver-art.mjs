import fs from 'node:fs';
import {decodePng} from './assets.mjs';
const source=process.argv[2]||'G:/My Drive/Artwork/Assets/SGQ_Enemies/normal/16x16/spider.png';
const im=decodePng(fs.readFileSync(source)),bytes=[];
for(let pose=0;pose<2;pose++)for(let half=0;half<2;half++)for(let y=0;y<16;y++) {
 let lo=0,hi=0;
 for(let x=0;x<8;x++) {
  const px=half*8+x,i=(y*im.width+(pose%2)*16+px)*4;
  let c=!im.rgba[i+3]?0:im.rgba[i]>180?1:im.rgba[i+1]>100?2:3;

  lo=lo*2+(c&1);hi=hi*2+(c>>1);
 }
 bytes.push(lo,hi);
}
fs.writeFileSync(new URL('../src/wireweaver_art.inc',import.meta.url),'// Generated from SGQ_Enemies spider.png by tools/weaver-art.mjs.\nstatic const uint8_t weaver_art[]={\n'+Array.from({length:bytes.length/16},(_,i)=>' '+bytes.slice(i*16,i*16+16).join(',')).join(',\n')+'\n};\n');
