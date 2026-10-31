# Sands town — first playable layout

The hub now uses the original Sands.png sheet, preserved in artwork/tiles/sands.png.
The authored map and collision legend are in content/tilesets/sands.json. The normal
tileset compiler deduplicates its 8px hardware tiles; gameplay still uses 16px blocks.
The sheet keeps its four colours (rounded to GBC RGB555); vending machines retain
an additional service palette so guns and shields remain distinguishable.

The settlement is 24 by 20 blocks. A north–south main street connects a residential
row, the market square, and the southern workshops. The departure yard has a fence
and lookout tower. Buildings face the streets; their roofs and walls are solid.
Doors are visual for now, without interior transitions. Open routes connect every
service. Echo, Marro, company posts, both vending machines, and the descent lift
keep their existing interactions; the northern company post moves one block
south to stand outside the residential frontage.

Next art/layout pass: unique shop signage, directional cliff corners, more varied
house fronts, and a clearer landing/reconstruction point. Playtest the walking
distances and street readability before adding building interiors.
