# Sands town — preserve the composition

The earlier grid of repeated roofs, cliff columns, and plain paths discarded the
character of Sands.png. This revision preserves the complete original 240x192
northwest composition, including its irregular silhouettes, broken ground edges,
cracks, shadow strips, and distinct building shapes. The service court to the east
and departure yard to the south extend connected patches rather than repeating a
single transition tile as a floor. The town is 24x16 blocks.

Source art: artwork/tiles/sands.png, unchanged. Authored blocks and collision:
content/tilesets/sands.json. Each legend character names an exact source block;
collision is separate from its appearance. Optional authoring helper:
`python tools/sands-town.py` (requires Pillow), then `node tools/tileset.mjs sands`.
Do not replace unique source blocks with generic terrain to simplify the map.

The detailed map uses 88 visual blocks and 214 unique hardware tiles. It explicitly
opts into two VRAM banks: 172 tiles in bank 0 and 42 in bank 1 at tile indices
80..121 ($9500..$979f). The loader sets the background bank bit per quadrant.
This stays above bank-1 weapon/portrait sprite art; generated combat worlds retain
their existing 172-tile limit. The compiler caps this authored-scene mode at 220.

Echo now stands on the path outside the original relay building. Marro, the
company posts, vending machines, and lift retain their functions. Building doors
remain decorative. Tests preserve the original composition shading slot for shading slot, independently of display colours, check service reachability, and exercise town interactions.

Town HUD names use seven on-demand glyph tiles at 44..50 instead of the full
64..127 font. Pause/menu text borrows the detail range while covering the scene;
HUD initialization reloads the detail tiles on close. Dialogue/loot-card tiles
192..253 and weapon sprite tiles remain separate.

Sands source greens are index guides, not a final aesthetic. Palette 0 now uses
Cragback rock colours (RGB555 28/25/17, 18/13/8, 6/5/4), with the menu paper
highlight (248/240/216). Vending colours remain separate. `paletteOverrides`
changes the display ramp after tile fitting, preserving all source pixel indices.
