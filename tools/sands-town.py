# Optional layout authoring helper: python tools/sands-town.py (requires Pillow).
import json
from pathlib import Path
from PIL import Image
p=Path('content/tilesets/sands.json');old=json.loads(Path('content/tilesets/ice.json').read_text(encoding='utf-8'));im=Image.open('artwork/tiles/sands.png').convert('RGBA')
pieces={};known={};legend={};chars={};g=[];coll=[]
def cell(x,y):
 tile=im.crop((x,y,x+16,y+16));key=tile.tobytes()
 if key not in known:
  n='sand' if x==0 and y==0 else f'art_{x}_{y}';known[key]=n;pieces[n]=[x,y,16,16]
 return known[key]
sand=cell(0,0)
for y in range(16):g.append([sand]*24);coll.append([False]*24)
def stamp(sx,sy,w,h,dx,dy,solid=False):
 for yy in range(h):
  for xx in range(w):g[dy+yy][dx+xx]=cell(sx+xx*16,sy+yy*16);coll[dy+yy][dx+xx]=solid
# Preserve the entire original composed scene pixel for pixel, including its ground edges.
stamp(0,0,15,12,0,0)
# Collision follows the original silhouettes rather than inventing square streets.
mask=['###############','..###########..','.#########.....','.##....##.###..','.........####..','.....#...####.#','..#####..####.#','#.#####........','###.###..#.....','##.......#.....','...###...######','.........######']
for y,row in enumerate(mask):
 for x,c in enumerate(row):coll[y][x]=c=='#'
# Extend the same rock enclosure around a small eastern service court.
stamp(32,16,5,3,16,0,True)
stamp(160,0,3,3,21,0,True)
stamp(160,48,3,4,17,6,True)
# Worn ground edges drawn as connected strips, never tiled across a square.
stamp(32,64,7,1,16,4)
stamp(112,112,2,3,15,9)
stamp(160,112,5,2,18,11)
stamp(144,128,1,3,23,8,True)
stamp(144,160,5,2,18,14,True)
# Southern departure ledge: fence sections, tower, grass tufts, and room to approach the lift.
stamp(32,224,4,1,1,14,True)
stamp(64,272,2,1,6,14)
stamp(128,208,1,3,6,12,True)
stamp(32,160,4,1,9,13)
stamp(32,176,4,1,9,14)
# Existing service decals keep their familiar controls and colours.
for n in ['vend_gun','vend_shield']:pieces[n]=old['pieces'][n]
# Two-block decals each need separate map indices, represented by their repeated character.
for x,n in [(20,'vend_gun'),(21,'vend_shield')]:
 for y in [8,9]:g[y][x]=n;coll[y][x]=True
for x in range(24):coll[0][x]=coll[15][x]=True
for y in range(16):coll[y][0]=coll[y][23]=True
# Left edge opens into the southern yard; retain broad, connected routes around the buildings.
for x,y in [(5,4),(8,4),(19,5),(19,13),(17,10),(20,10),(21,10),(3,12),(4,12)]:coll[y][x]=False
rows=[]
for y in range(16):
 row=''
 for x in range(24):
  key=(g[y][x],coll[y][x])
  if key not in chars:
   ch=chr(0x100+len(chars));chars[key]=ch;legend[ch]={'piece':key[0],**({'solid':True} if key[1] else {})}
  row+=chars[key]
 rows.append(row)
d={'comment':'The original 240x192 Sands composition occupies the northwest unchanged. The eastern court and southern yard extend connected patches of the sheet. Unicode legend characters identify exact source blocks, not generic terrain. See docs/design/town.md.','src':'artwork/tiles/sands.png','backdrop':[136,192,112],'sheets':old['sheets'],'palettes':6,'vramBanks':2,'paletteOverrides':{'0':['#f8f0d8','#e0c888','#906840','#302820']},'pieces':pieces,'legend':legend,'maps':{'town':rows}}
p.write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
