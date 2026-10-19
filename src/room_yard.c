#pragma bank 255
// The ice yard: a hand-laid test map for the visual layer (room.h). A few
// generic, repeatable tiles in six material palettes build ice, cliffs, rusty
// catwalks over a void, plank paths, dirt and a shack. The shack's roof is
// drawn over sprites (the Color's background priority flag), so you walk behind
// it. Each block's look follows simple rules from its neighbours (a cliff face
// where rock meets the ground, a catwalk's front edge, a roof's eave): the
// start of the visual rules generated maps will use.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "room.h"
#include "gfx.h"
#include "talk.h"
#include "save.h"
#include "dialogue.h"

// --- Palettes (background 0-5): one per material, sharing one ink. ---
#define C(r, g, b) RGB8(r, g, b)
#define INK C(26, 40, 48)
static const uint16_t pals[6][4] = {
    {C(232, 250, 248), C(170, 220, 216), C(106, 156, 152), INK},  // 0 ice
    {C(184, 216, 216), C(122, 160, 168), C(74, 104, 112), INK},   // 1 rock
    {C(224, 144, 96), C(168, 90, 48), C(90, 152, 144), INK},      // 2 rusted metal, teal patina
    {C(200, 144, 96), C(138, 90, 56), C(90, 58, 40), INK},        // 3 wood
    {C(232, 250, 248), C(192, 128, 96), C(138, 80, 56), INK},     // 4 dirt on ice
    {C(58, 80, 96), C(40, 56, 64), C(24, 32, 40), C(10, 16, 20)}, // 5 void and shadow
};
#define P_ICE 0
#define P_ROCK 1
#define P_RUST 2
#define P_WOOD 3
#define P_DIRT 4
#define P_VOID 5

// --- Tiles: colour indices 0 (lightest) to 3 (ink). ---
enum {
    T_ICE, T_ICE_CRACK, T_ICE_CRACK2, T_ROCK_TOP, T_FACE_UP, T_FACE_LOW,
    T_PANEL, T_BRACE, T_VOID, T_VOID_EDGE, T_PLANK, T_DIRT, T_DIRT2,
    T_ROOF, T_EAVE, T_BOARDS, T_DOOR_TOP, T_DOOR, T_COUNT
};
static const char art[T_COUNT][8][9] = {
    {"00000000", "00000000", "00010000", "00000000", "00000000", "00000100", "00000000", "00000000"},  // ice
    {"00000000", "00000020", "00000200", "00002000", "00220000", "02000000", "00000000", "00000000"},  // ice, crack
    {"00000000", "02000000", "00200000", "00022000", "00000200", "00000020", "00000000", "00000000"},  // ice, crack 2
    {"00100001", "01110011", "00100000", "00000100", "10001110", "11000100", "10000000", "00010001"},  // rock top
    {"31111113", "31211213", "31211213", "31211213", "31211213", "32211223", "32211223", "32211223"},  // cliff face, upper
    {"32211223", "32221223", "32222223", "32222223", "33222233", "33322333", "33333333", "33333333"},  // cliff face, lower
    {"33333333", "31000000", "31011100", "31012100", "31011100", "31000000", "31111222", "31112222"},  // rust panel (flipped x4)
    {"33333333", "31000013", "30300303", "30033003", "30033003", "30300303", "31000013", "33333333"},  // catwalk brace
    {"22222222", "22122222", "22222212", "22222222", "21222222", "22222122", "22222222", "22221222"},  // void
    {"33333333", "33333333", "32323232", "23232323", "22222222", "22122222", "22222212", "22222222"},  // void under an edge
    {"33333333", "00000000", "11011111", "11111211", "33333333", "00000000", "11112111", "11111111"},  // planks
    {"00100000", "01210010", "00100121", "00001010", "01000100", "12100000", "01000210", "00002120"},  // dirt
    {"00000000", "00120000", "01221000", "00110010", "00000121", "00100010", "01210000", "00100000"},  // dirt, sparse
    // The roof is drawn over sprites, and colour 0 always lets a sprite show
    // through: tiles that hide the player use colours 1-3 only.
    {"11231123", "11231123", "12231123", "11231123", "11231223", "11231123", "11231123", "12231123"},  // corrugated roof
    {"11231123", "11231123", "11231223", "11231123", "33333333", "33333333", "22222222", "11111111"},  // roof eave
    {"01130110", "01130110", "01130120", "01130110", "02130110", "01130110", "01130110", "01130110"},  // wall boards
    {"22222222", "23333333", "23333333", "23333333", "23333333", "23333333", "23333333", "23333333"},  // door frame, top
    {"23333333", "23333333", "23333333", "23333333", "23333333", "23333333", "23333333", "23333333"},  // door frame, side
};

// --- Visual blocks: 4 tiles (TL TR BL BR) and their attributes. ---
#define FX 0x20  // attribute: flip x
#define FY 0x40  // flip y
#define PRI 0x80 // drawn over sprites
enum {
    V_ICE, V_ICE2, V_ICE3, V_ROCK, V_FACE, V_CATWALK, V_CATWALK_FRONT, V_VOID, V_VOID_EDGE,
    V_PLANKS, V_BRIDGE_FRONT, V_DIRT, V_ROOF, V_EAVE, V_WALL, V_DOOR, V_COUNT
};
static const uint8_t vtile[V_COUNT][4] = {
    {T_ICE, T_ICE, T_ICE, T_ICE},
    {T_ICE, T_ICE_CRACK, T_ICE_CRACK2, T_ICE},
    {T_ICE_CRACK2, T_ICE, T_ICE, T_ICE_CRACK},
    {T_ROCK_TOP, T_ROCK_TOP, T_ROCK_TOP, T_ROCK_TOP},
    {T_FACE_UP, T_FACE_UP, T_FACE_LOW, T_FACE_LOW},
    {T_PANEL, T_PANEL, T_PANEL, T_PANEL},
    {T_PANEL, T_PANEL, T_BRACE, T_BRACE},
    {T_VOID, T_VOID, T_VOID, T_VOID},
    {T_VOID_EDGE, T_VOID_EDGE, T_VOID, T_VOID},
    {T_PLANK, T_PLANK, T_PLANK, T_PLANK},
    {T_PLANK, T_PLANK, T_BRACE, T_BRACE},
    {T_DIRT, T_DIRT2, T_DIRT2, T_DIRT},
    {T_ROOF, T_ROOF, T_ROOF, T_ROOF},
    {T_ROOF, T_ROOF, T_EAVE, T_EAVE},
    {T_BOARDS, T_BOARDS, T_BOARDS, T_BOARDS},
    {T_DOOR_TOP, T_DOOR_TOP, T_DOOR, T_DOOR},
};
static const uint8_t vattr[V_COUNT][4] = {
    {P_ICE, P_ICE | FX, P_ICE | FY, P_ICE | FX | FY},
    {P_ICE, P_ICE, P_ICE, P_ICE},
    {P_ICE, P_ICE | FX, P_ICE, P_ICE},
    {P_ROCK, P_ROCK | FX, P_ROCK | FY, P_ROCK | FX | FY},
    {P_ROCK, P_ROCK | FX, P_ROCK, P_ROCK | FX},
    {P_RUST, P_RUST | FX, P_RUST | FY, P_RUST | FX | FY},  // one panel tile, four ways
    {P_RUST, P_RUST | FX, P_RUST, P_RUST | FX},
    {P_VOID, P_VOID | FX, P_VOID | FY, P_VOID | FX | FY},
    {P_VOID, P_VOID | FX, P_VOID | FY, P_VOID},
    {P_WOOD, P_WOOD | FX, P_WOOD, P_WOOD | FX},
    {P_WOOD, P_WOOD | FX, P_RUST, P_RUST | FX},
    {P_DIRT, P_DIRT, P_DIRT | FX, P_DIRT | FX},
    {P_RUST | PRI, P_RUST | PRI, P_RUST | PRI, P_RUST | PRI},  // the roof: walk behind it
    {P_RUST | PRI, P_RUST | PRI, P_RUST | PRI, P_RUST | PRI},
    {P_WOOD, P_WOOD | FX, P_WOOD, P_WOOD | FX},
    {P_WOOD, P_WOOD | FX, P_WOOD, P_WOOD | FX},
};

// --- The map. # rock, . ice, , dirt, = catwalk (raised), b plank bridge
// (raised), o void (a pit), p plank path, R roof (walkable: you pass behind it),
// W wall, D door. ---
#define YARD_W 24
#define YARD_H 16
static const char layout[YARD_H][YARD_W + 1] = {
    "########################",
    "########################",
    "###...........RRRRR...##",
    "##...,,.......RRRRR....#",
    "#...,,,.......RRRRR....#",
    "#....,........WWDWW....#",
    "#.............WWDWW....#",
    "#..=====bb=====........#",
    "#..oooooppooooo...,,...#",
    "#..oooooppooooo..,,,...#",
    "#........pp............#",
    "#........pp.......####.#",
    "#..,,....pp.......####.#",
    "#.,,,...........##.....#",
    "##..............##....##",
    "########################",
};

static uint8_t cell(int8_t x, int8_t y) {
    if (x < 0 || y < 0 || x >= YARD_W || y >= YARD_H) return '#';
    return (uint8_t)layout[y][x];
}

void room_load_yard(void) BANKED {
    // With a file this is the town, drawn with the artist's tiles (room_town.c);
    // without one (B on the title), this hand-laid test yard.
    if (save_slot != SAVE_NONE) {
        room_load_town();
        return;
    }
    // Tiles from the drawings, into the scene's tiles.
    uint8_t t[16];
    for (uint8_t i = 0; i < T_COUNT; i++) {
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t lo = 0, hi = 0;
            for (uint8_t x = 0; x < 8; x++) {
                uint8_t c = art[i][r][x] - '0', bit = 0x80 >> x;
                if (c & 1) lo |= bit;
                if (c & 2) hi |= bit;
            }
            t[r * 2] = lo;
            t[r * 2 + 1] = hi;
        }
        set_bkg_data(SCENE_TILES + i, 1, t);
    }
    for (uint8_t v = 0; v < V_COUNT; v++) {
        for (uint8_t q = 0; q < 4; q++) {
            room_tile_lut[(v << 2) | q] = SCENE_TILES + vtile[v][q];
            room_attr_lut[(v << 2) | q] = vattr[v][q];
        }
    }
    room_attr_uniform = 0;  // six palettes and flips: stream attributes while scrolling
    memcpy(pal_base_bg, pals, sizeof(pals));

    room_w = YARD_W;
    room_h = YARD_H;
    room_px_w = (uint16_t)YARD_W * BLOCK_SIZE;
    room_px_h = (uint16_t)YARD_H * BLOCK_SIZE;
    for (uint16_t i = 0; i < sizeof(room_map); i++) room_map[i] = BLK_SOLID, room_vis[i] = V_ROCK;
    // Each block: its type, and its look from its neighbours.
    for (int8_t y = 0; y < YARD_H; y++) {
        for (int8_t x = 0; x < YARD_W; x++) {
            uint8_t c = cell(x, y), below = cell(x, y + 1), type = BLK_FLOOR, vis;
            switch (c) {
            case '#':
                type = BLK_SOLID;
                vis = below == '#' ? V_ROCK : V_FACE;  // a face where rock meets the ground
                break;
            case ',':
                vis = V_DIRT;
                break;
            case '=':
                type = BLK_RAISED;
                vis = below == '=' ? V_CATWALK : V_CATWALK_FRONT;  // its front edge over the void
                break;
            case 'b':
                type = BLK_RAISED;
                vis = V_BRIDGE_FRONT;
                break;
            case 'o': {
                uint8_t above = cell(x, y - 1);
                type = BLK_PIT;
                vis = above == '=' || above == 'b' ? V_VOID_EDGE : V_VOID;  // in the catwalk's shadow
                break;
            }
            case 'p':
                vis = V_PLANKS;
                break;
            case 'R':
                vis = below == 'R' ? V_ROOF : V_EAVE;  // walkable, drawn over you
                break;
            case 'W':
                type = BLK_SOLID;
                vis = V_WALL;
                break;
            case 'D':
                type = BLK_SOLID;
                vis = V_DOOR;
                break;
            default: {
                uint8_t h = (uint8_t)(x * 7 + y * 13);  // ice, varied by position
                vis = h % 5 == 0 ? V_ICE2 : h % 7 == 1 ? V_ICE3 : V_ICE;
                break;
            }
            }
            uint16_t k = ((uint16_t)y << 5) | (uint8_t)x;
            room_map[k] = type;
            room_vis[k] = vis;
        }
    }
    // Who talks here (docs/design/voices.md): Echo's relay near the start (walking
    // up to it plays her first contact once), a company post, a Meridian kiosk,
    // and the scrap broker.
    talk_add(9, 3, TALK_RELAY, DLG_VOICE_ECHO);
    talk_zone(8, 2, 11, 5, DLG_RELAY_FIRST_CONTACT, STORY_MET_ECHO);
    talk_add(20, 4, TALK_POST, DLG_VOICE_CEO);
    talk_add(20, 13, TALK_POST, DLG_VOICE_MERIDIAN);
    talk_add(18, 10, TALK_MARRO, DLG_VOICE_MARRO);
}
