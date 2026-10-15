// The chunk format (M2, docs/design/level-generation.md), shared by the build
// (tools/chunks.mjs) and the chunk editor (web/chunks.js). No Node or DOM here.
//
// A chunk is CHUNK_W x CHUNK_H blocks (a quarter screen), written as rows of
// characters (LEGEND). Each has tags (the roles it can fill in a map) and a
// weight (how often it's picked among those that fit). The ROM holds the tables
// at a fixed size (CHUNK_MAX), so the editor can patch edited chunks straight
// into a built ROM and preview them without a rebuild.

export const CHUNK_W = 5, CHUNK_H = 4, CHUNK_CELLS = CHUNK_W * CHUNK_H;
export const CHUNK_MAX = 128;
// Character -> cell code (must match CELL_* in the generated header).
export const LEGEND = {
  '.': 0, // floor
  '#': 1, // wall
  o: 2, // pit
  '=': 3, // raised
  '?': 4, // maybe wall (else floor)
  ':': 5, // maybe pit
  '+': 6, // maybe raised
  e: 7, // enemy slot (floor)
  '*': 8, // item slot (floor)
};
export const CELL_NAMES = ['FLOOR', 'WALL', 'PIT', 'RAISED', 'MAYBE_WALL', 'MAYBE_PIT', 'MAYBE_RAISED', 'ENEMY', 'ITEM'];
// The roles a chunk can fill in a map.
export const TAGS = { open: 1, rock: 2, camp: 4, cache: 8, start: 16, exit: 32 };
export const ANY = 63;

/**
 * Check a parsed chunk file. Returns { theme, chunks: [{ name, tags, weight, cells }] }
 * (tags as bits, cells as codes); throws with the chunk's name on errors.
 * `file` only labels the errors.
 */
export function parseChunkDoc(doc, file = 'chunks') {
  const chunks = [];
  for (const [i, c] of (doc.chunks || []).entries()) {
    const where = `${file}, chunk ${c.name ?? i}`;
    if (!Array.isArray(c.rows) || c.rows.length !== CHUNK_H) throw new Error(`${where}: needs ${CHUNK_H} rows`);
    const cells = [];
    for (const row of c.rows) {
      if (typeof row !== 'string' || row.length !== CHUNK_W) throw new Error(`${where}: row "${row}" must be ${CHUNK_W} characters`);
      for (const ch of row) {
        if (!(ch in LEGEND)) throw new Error(`${where}: unknown character '${ch}' (legend: ${Object.keys(LEGEND).join(' ')})`);
        cells.push(LEGEND[ch]);
      }
    }
    let tags = 0;
    for (const t of c.tags || ['any']) {
      if (t === 'any') tags |= ANY;
      else if (TAGS[t]) tags |= TAGS[t];
      else throw new Error(`${where}: unknown tag '${t}' (${Object.keys(TAGS).join(', ')}, any)`);
    }
    const weight = c.weight ?? 1;
    if (!(Number.isInteger(weight) && weight >= 1 && weight <= 15)) throw new Error(`${where}: weight must be 1-15`);
    chunks.push({ name: c.name ?? String(i), tags, weight, cells });
  }
  return { theme: doc.theme, chunks };
}

/** Problems that stop a chunk set from working in the generator (an empty list if none). */
export function checkChunkSet(chunks) {
  const errs = [];
  if (!chunks.length) errs.push('no chunks');
  if (chunks.length > CHUNK_MAX) errs.push(`at most ${CHUNK_MAX} chunks (${chunks.length})`);
  for (const [name, bit] of Object.entries(TAGS)) {
    if (!chunks.some((c) => c.tags & bit)) errs.push(`no chunk is tagged '${name}' (or 'any'): the generator needs one`);
  }
  // Weights are summed in a byte per tag.
  for (const [name, bit] of Object.entries(TAGS)) {
    const total = chunks.reduce((s, c) => s + (c.tags & bit ? c.weight : 0), 0);
    if (total > 255) errs.push(`the weights of '${name}' chunks add up to ${total} (at most 255)`);
  }
  return errs;
}

/**
 * The ROM tables, in the order tools/chunks.mjs lays them out: count, then
 * tags, weights and themes (CHUNK_MAX each), then cells (CHUNK_MAX * CHUNK_CELLS).
 */
export function encodeChunkTables(chunks) {
  const tags = new Uint8Array(CHUNK_MAX), weight = new Uint8Array(CHUNK_MAX), theme = new Uint8Array(CHUNK_MAX);
  const cells = new Uint8Array(CHUNK_MAX * CHUNK_CELLS);
  chunks.forEach((c, k) => {
    tags[k] = c.tags;
    weight[k] = c.weight;
    theme[k] = c.theme ?? 0;
    cells.set(c.cells, k * CHUNK_CELLS);
  });
  return { count: chunks.length, tags, weight, theme, cells };
}

/** Rows of characters from cell codes (the file format). */
export function cellsToRows(cells) {
  const ch = Object.keys(LEGEND);
  const rows = [];
  for (let r = 0; r < CHUNK_H; r++) rows.push(cells.slice(r * CHUNK_W, (r + 1) * CHUNK_W).map((v) => ch[v]).join(''));
  return rows;
}

/** Tag names from bits ('any' when all are set). */
export function tagNames(bits) {
  if ((bits & ANY) === ANY) return ['any'];
  return Object.entries(TAGS).filter(([, b]) => bits & b).map(([n]) => n);
}
