# Mercenary source art

`source-sheet.png` is the NPC template supplied by the user for this enemy.
It is a 2x export: each native 16x16 frame occupies 32x32 pixels in the PNG.

`node tools/import-mercenary-art.mjs` encodes walk rows 4–6 and roll rows 12–14
into `src/mercenary_art.h`. Rows and frames are zero based. The left-facing
poses mirror the right-facing ones. The sheet's pale green background maps to
transparent; cream, cyan, and navy map to the three visible sprite colors.

The game reuses its existing sixteen-direction rifle art in VRAM bank 1.
