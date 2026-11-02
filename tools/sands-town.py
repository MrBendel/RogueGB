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
for y in range(16):g.append([sand]*16);coll.append([False]*16)
def stamp(sx,sy,w,h,dx,dy,solid=False):
 for yy in range(h):
  for xx in range(w):g[dy+yy][dx+xx]=cell(sx+xx*16,sy+yy*16);coll[dy+yy][dx+xx]=solid
# Preserve the entire original composed scene pixel for pixel, including its ground edges.
stamp(0,0,15,12,0,0)
# Collision follows the original silhouettes rather than inventing square streets.
mask=['###############','..###########..','.#########.....','.##....##.###..','.........####..','.....#...####.#','..#####..####.#','#.#####........','###.###..#.....','##.......#.....','...###...######','.........######']
for y,row in enumerate(mask):
 for x,c in enumerate(row):coll[y][x]=c=='#'
# Face/base collision follows the source silhouettes; ground under a finished
# cliff is walkable. The narrow plank bridge remains traversable.
for x in range(1,10):coll[3][x]=True
for x in range(10,13):coll[2][x]=True
for y in range(6):
 for x in (13,14):coll[y][x]=True
for y in range(4,9):coll[y][1]=True
coll[9][1]=False
coll[6][14]=False
for x in range(9,15):coll[11][x]=False
# Three complete buildings from the original composition; no duplicate market wing.
# Continue the original right edge into the room boundary.
stamp(224,0,1,12,15,0,True)
# Storage containers stack vertically as in the source demo, with darker lower
# tiers. Palette-only aliases share the exact same hardware tile patterns.
def containers(dx,top,tiers):
 for tier in range(tiers):
  for xx in range(4):
   base=cell(32+xx*16,224)
   n=f'{base}_storage_{tier}'
   pieces[n]={'rect':pieces[base], 'palette':tier}
   g[top+tier][dx+xx]=n;coll[top+tier][dx+xx]=True
containers(1,13,2)
containers(10,12,3)
stamp(64,272,2,1,6,14)
stamp(128,208,1,3,6,12,True)
# The elevated tank occludes the player; only the grounded feet block walking.
for y in (12,13):
 base=g[y][6];n=base+'_behind'
 pieces[n]={'rect':pieces[base], 'behind':True}
 g[y][6]=n;coll[y][6]=False
for x in range(16):coll[0][x]=coll[15][x]=True
for y in range(16):coll[y][0]=coll[y][15]=True
# Door approaches and the central yard lane are explicitly walkable.
for x,y in [(5,4),(8,4),(5,9),(11,7),(7,10),(6,10),(3,12),(4,12),(8,12),(12,8)]:coll[y][x]=False
rows=[]
for y in range(16):
 row=''
 for x in range(16):
  key=(g[y][x],coll[y][x])
  if key not in chars:
   ch=chr(0x100+len(chars));chars[key]=ch;legend[ch]={'piece':key[0],**({'solid':True} if key[1] else {})}
  row+=chars[key]
 rows.append(row)
used={n for row in g for n in row}
pieces={n:p for n,p in pieces.items() if n in used}
d={'comment':'The original 240x192 Sands composition occupies the northwest unchanged. The southern departure yard extends connected terrain; the original three buildings have accessible interiors. Unicode legend characters identify exact source blocks, not generic terrain. See docs/design/town.md.','src':'artwork/tiles/sands.png','backdrop':[136,192,112],'sheets':old['sheets'],'palettes':6,'vramBanks':2,'paletteOverrides':{'0':['#f8f0d8','#e0c888','#906840','#302820']},'paletteVariants':[['#d8c098','#b09860','#705030','#282018'],['#b09870','#887040','#503820','#201810']],'pieces':pieces,'legend':legend,'maps':{'town':rows}}
p.write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
