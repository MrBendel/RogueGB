# Optional layout authoring helper: python tools/sands-town.py (requires Pillow).
import json
from pathlib import Path
from PIL import Image
p=Path('content/tilesets/sands.json');old=json.loads(Path('content/tilesets/ice.json').read_text(encoding='utf-8'));im=Image.open('artwork/tiles/sands.png').convert('RGBA')
pieces={};known={};legend={};chars={};g=[];coll=[]
W,H=16,19
def cell(x,y):
 tile=im.crop((x,y,x+16,y+16));key=tile.tobytes()
 if key not in known:
  n='sand' if x==0 and y==0 else f'art_{x}_{y}';known[key]=n;pieces[n]=[x,y,16,16]
 return known[key]
sand=cell(0,0)
for y in range(H):g.append([sand]*W);coll.append([False]*W)
def stamp(sx,sy,w,h,dx,dy,solid=False):
 for yy in range(h):
  for xx in range(w):g[dy+yy][dx+xx]=cell(sx+xx*16,sy+yy*16);coll[dy+yy][dx+xx]=solid
# Preserve the painted scene; extend its unfinished ground into the departure yard.
stamp(0,0,15,12,0,0)
# Close the pale ground tongues with the existing irregular sand edges. These
# cells finish the ground boundary: never replace painted rocks or props.
# Reusing the source's three edge variants adds no hardware tile patterns.
for sx,dx in [(48,2),(48,6),(64,7),(80,8)]:
 stamp(sx,144,1,1,dx,11)
# Collision follows the original silhouettes rather than inventing square streets.
mask=['###############','..###########..','.#########.....','.##....##.###..','.........####..','.....#...####.#','..#####..####.#','#.#####........','###.###..#.....','##.......#.....','...###...######','.........######']
for y,row in enumerate(mask):
 for x,c in enumerate(row):coll[y][x]=c=='#'
# Face/base collision follows the source silhouettes; ground under a finished
# cliff is walkable. Only the actual stair tile at (8,5) crosses the wall.
for x in range(1,10):coll[3][x]=True
for x in range(10,13):coll[2][x]=True
for y in range(6):
 for x in (13,14):coll[y][x]=True
for y in range(4,9):coll[y][1]=True
coll[9][1]=True
coll[6][13]=coll[6][14]=True
for x,y in [(6,5),(7,5),(14,9),(10,8),(10,9)]:coll[y][x]=True
for x in range(9,15):coll[11][x]=False
# Three complete buildings from the original composition; no duplicate market wing.
# Continue the original right edge into the room boundary.
stamp(224,0,1,12,15,0,True)
# Complete container surfaces from the source, not recoloured copies of roofs.
# Small left container: light top over its authored dark face.
stamp(32,224,4,2,1,13,True)
# Preserve the entire staggered example: back container, overlapping foreground
# container, doors and grass. Empty/grass cells are walkable.
stamp(16,224,5,6,10,12)
container_mask=['.####','.####','#####','###..','###..','##...']
for yy,row in enumerate(container_mask):
 for xx,c in enumerate(row):coll[12+yy][10+xx]=c=='#'
stamp(64,272,2,1,6,14)
stamp(128,208,1,3,6,12,True)
# The elevated tank occludes the player; only the grounded feet block walking.
for y in (12,13):
 base=g[y][6];n=base+'_behind'
 pieces[n]={'rect':pieces[base], 'behind':True}
 g[y][6]=n;coll[y][6]=False
for x in range(W):coll[0][x]=coll[H-1][x]=True
for y in range(H):coll[y][0]=coll[y][W-1]=True
# Door approaches and the central yard lane are explicitly walkable.
for x,y in [(5,4),(8,4),(5,9),(11,7),(7,10),(6,10),(3,12),(4,12),(8,12),(12,8)]:coll[y][x]=False
# Material aliases change palette attributes only; indexed patterns stay shared.
def material(x,y,palette):
 base=g[y][x];n=f'{base}_material_{palette}'
 spec=pieces[base]
 pieces[n]={**({'rect':spec} if isinstance(spec,list) else spec), 'palette':palette}
 g[y][x]=n
# Enclosed roof surfaces can use weathered wood for their fill. Outdoor props retain sand
# in slot 1, so exposed ground and transparent source pixels stay continuous.
for x0,y0,w,h in [(4,6,3,2),(10,3,3,3)]:
 for y in range(y0,y0+h):
  for x in range(x0,x0+w):material(x,y,1)
for y in (13,14):
 for x in range(1,5):material(x,y,2)
for yy,row in enumerate(container_mask):
 for xx,c in enumerate(row):
  if c=='#':material(10+xx,12+yy,2)
for x,y in [(0,11),(1,11),(7,14),(13,15),(14,15)]:material(x,y,3)
rows=[]
for y in range(H):
 row=''
 for x in range(16):
  key=(g[y][x],coll[y][x])
  if key not in chars:
   ch=chr(0x100+len(chars));chars[key]=ch;legend[ch]={'piece':key[0],**({'solid':True} if key[1] else {})}
  row+=chars[key]
 rows.append(row)
used={n for row in g for n in row}
pieces={n:p for n,p in pieces.items() if n in used}
d={'comment':'The original Sands scene is preserved outside four ground transition cells; existing irregular sand edges finish its southern boundary. The southern departure yard extends connected terrain; the original three buildings have accessible interiors. Unicode legend characters identify exact source blocks, not generic terrain. See docs/design/town.md.','src':'artwork/tiles/sands.png','backdrop':[136,192,112],'sheets':old['sheets'],'palettes':6,'vramBanks':2,'paletteOverrides':{'0':['#f8d878','#e0b858','#986038','#302018']},'paletteVariants':[['#d8b078','#a87848','#684028','#281c18'],['#f8ad50','#e0b858','#a84828','#382018'],['#b8b868','#e0b858','#708040','#303820']],'pieces':pieces,'legend':legend,'maps':{'town':rows}}
p.write_text(json.dumps(d,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
