"""Authored surface maps using the town's existing indexed art and palettes.

Called by sands-town.py; no new patterns, source pixels or palette fitting.
"""
def add_surface(d):
    town = d['maps']['town']
    legend = d['legend']
    def tile(x, y, solid=None):
        spec = dict(legend[town[y][x]])
        if solid is not None:
            spec.pop('solid', None)
            if solid: spec['solid'] = True
        for ch, other in legend.items():
            if other == spec: return ch
        ch = chr(0x100 + len(legend))
        legend[ch] = spec
        return ch
    sand = tile(0, 0, False)
    # Flat light sand shares the existing flat pattern; only its palette differs.
    # Do not repeat a ground-edge tile through the middle of a clearing.
    d['paletteVariants'].append(['#e8c888', '#e8c888', '#a05030', '#382820'])
    d['pieces']['surface_light_sand'] = {'rect': [0, 0, 16, 16], 'palette': 5}
    light = chr(0x100 + len(legend))
    legend[light] = {'piece': 'surface_light_sand'}
    d['pieces']['surface_ground_north'] = {'tiles': ['89y 99y', '88y 98y']}
    north = chr(0x100 + len(legend))
    legend[north] = {'piece': 'surface_ground_north'}
    def new_map(height=16):
        g = [[sand] * 20 for _ in range(height)]
        # Continuous cliff rims: upper ridge and its face, irregular lower lip.
        for x in range(20):
            sx = [4, 5, 6, 5, 4, 6, 6, 5][x % 8]
            g[0][x] = tile(sx, 1, True)
            g[1][x] = tile(sx, 2, True)
            g[2][x] = tile(sx, 3, True)
            g[height-2][x] = tile(3 if x % 7 == 0 else 5 if x % 7 == 6 else 4, 9, True)
            g[height-1][x] = tile(3 if x % 7 == 0 else 5 if x % 7 == 6 else 4, 10, True)
        for y in range(3, height-2):
            g[y][0] = tile(1, 7 if y % 3 else 8, True)
            g[y][19] = tile(14, 5 if y % 3 else 6, True)
        return g
    def patch(g, sx, sy, w, h, dx, dy):
        for y in range(h):
            for x in range(w): g[dy+y][dx+x] = tile(sx+x, sy+y)
    def clearing(g, x, y, w, h):
        # Existing ground textures and irregular lower transitions form a single
        # continuous pale clearing, not checkerboard patches.
        for yy in range(h):
            for xx in range(w):
                g[y+yy][x+xx] = north if yy == 0 else light
        for xx in range(w):
            g[y+h-1][x+xx] = tile(3 + xx % 3, 9, False)
    cross = new_map()
    clearing(cross, 6, 3, 8, 10)
    for y in range(4):
        for x in range(8, 12): cross[y][x] = sand
    for y in range(7, 11):
        for x in range(13, 20): cross[y][x] = sand
    patch(cross, 4, 1, 3, 3, 2, 5)  # complete rocky outcrop
    patch(cross, 6, 12, 1, 3, 9, 7)  # landmark tank, walk behind its top
    patch(cross, 1, 13, 4, 2, 2, 11)
    for x, y in [(15, 4), (16, 12), (5, 3)]: patch(cross, 13, 15, 2, 1, x, y)
    for y in range(13, 16):
        for x in range(8, 12): cross[y][x] = sand
    rust = new_map()
    clearing(rust, 5, 6, 12, 7)
    for y in range(7, 11):
        for x in range(6): rust[y][x] = sand
    patch(rust, 10, 12, 5, 6, 13, 3)  # full staggered container assembly
    patch(rust, 1, 13, 4, 2, 3, 3)
    patch(rust, 1, 13, 4, 2, 3, 11)
    patch(rust, 6, 12, 1, 3, 8, 4)
    patch(rust, 13, 15, 2, 1, 16, 12)
    d['maps']['crossroads'] = [''.join(row) for row in cross]
    d['maps']['rustyard'] = [''.join(row) for row in rust]
    wake = new_map(24)
    clearing(wake, 7, 3, 7, 12)
    for y in range(4):
        for x in range(8, 12): wake[y][x] = sand
    # A sheltered southern pocket, with a two-block pass beside the shield
    # cache. Every walking route north passes within its pickup reach.
    for x in range(1, 19):
        for yy in range(3): wake[16+yy][x] = tile(4 + x % 3, 1 + yy, True)
    for y in range(16, 19):
        for x in (9, 10): wake[y][x] = sand
    # Use complete formations from the town scene, with their original caps,
    # faces and ground at the foot. Avoid a fence of identical cliff columns.
    patch(wake, 4, 0, 3, 5, 2, 14)
    patch(wake, 10, 0, 3, 3, 14, 16)
    patch(wake, 0, 5, 3, 5, 0, 5)
    patch(wake, 13, 0, 3, 7, 17, 0)
    # Keep the perimeter closed even where a source formation has open ground.
    for y in range(5, 10):
        wake[y][0] = tile(0, 5 + y - 5, True)
    for y in range(7):
        wake[y][19] = tile(15, y, True)
    # The ground apron is the town's irregular painted transition, not a tiled
    # rectangle of a single edge. The shield pass remains clear at x=9..10.
    patch(wake, 2, 4, 7, 1, 2, 19)
    patch(wake, 2, 9, 6, 1, 11, 19)
    patch(wake, 4, 1, 3, 3, 3, 9)
    patch(wake, 1, 13, 4, 2, 14, 10)
    patch(wake, 6, 12, 1, 3, 14, 19)
    for x, y in [(3, 20), (15, 5), (6, 13)]: patch(wake, 13, 15, 2, 1, x, y)
    d['maps']['wake'] = [''.join(row) for row in wake]
    # The southern yard is the outward road. Keep a four-block approach clear;
    # the two-block opening itself is deliberately narrower than the combat maps.
    rows = [list(row) for row in town]
    for x in (7, 8):
        rows[16][x] = north
        rows[17][x] = rows[18][x] = light
    d['maps']['town'] = [''.join(row) for row in rows]
