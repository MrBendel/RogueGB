#pragma bank 255
// Loot in the world (pickups.h). Banked: per frame only in maps with chests or
// drops (play.c checks pickups_on first).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <string.h>
#include "pickups.h"
#include "loot.h"
#include "level.h"
#include "descent.h"
#include "room.h"
#include "camera.h"
#include "player.h"
#include "chest.h"
#include "gfx.h"
#include "hud.h"
#include "sfx.h"
#include "save.h"
#include "skills.h"
#include "text.h"
#include "weapon.h"

#define COIN_VRAM 84        // sprite tiles (bank 0): free between the reload bar's and the chest's
#define OPEN_REACH 14       // px between the player's feet and a chest's middle to open it
#define TAKE_REACH 10       // ... and a drop's, to pick it up (or see a gun's card)
#define CARD_ROWS 6
#define CARD_TILES 192      // the card's text: second VRAM bank (the dialogue box's, never both)
#define CARD_PAL (BG_PAL_HEALTH << 2)  // palette 7 (the HUD's, hidden under the card): the rarity

uint8_t pickups_on;
uint8_t pickups_map;  // a descent map is loaded (loot can drop)
uint8_t chest_open_bits;
uint8_t drop_kind[DROP_MAX];
uint16_t drop_x[DROP_MAX], drop_y[DROP_MAX];
uint16_t drop_value[DROP_MAX];
Gun floor_gun[DROP_GUNS];
uint8_t floor_icon[DROP_GUNS][4];
uint8_t card_drop = CARD_NONE;
uint16_t credits_picked, guns_taken, chests_opened;
uint16_t kill_x, kill_y;
uint8_t pickups_tick;
uint16_t card_saved_pal[4];

// A coin: gold with a highlight (the shots' palette), over a blank tile.
static const uint8_t coin_tiles[32] = {
    0x3C, 0x3C, 0x62, 0x5E, 0xC1, 0xBF, 0xC1, 0xBF, 0xC1, 0xBF, 0x81, 0xFF, 0x42, 0x7E, 0x3C, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// Rarity colors for the card's name (on the card's paper): grey, green, blue, purple, orange.
static const uint16_t rarity_ink[RARITIES] = {
    RGB8(96, 88, 80), RGB8(40, 140, 48), RGB8(40, 96, 216), RGB8(144, 56, 200), RGB8(224, 112, 16),
};
static const char *const rarity_names[RARITIES] = {"Common", "Uncommon", "Rare", "Epic", "Legendary"};

void pickups_init(void) BANKED {
    set_sprite_data(COIN_VRAM, 2, coin_tiles);
    pickups_on = pickups_map = 0;
    card_drop = CARD_NONE;
}

static void card_hide(void);

void pickups_load_map(uint8_t opened) BANKED {
    if (card_drop != CARD_NONE) card_hide();
    pickups_map = 1;
    chest_open_bits = opened;
    for (uint8_t i = 0; i < DROP_MAX; i++) drop_kind[i] = DROP_NONE;
    card_drop = CARD_NONE;
    // Chests are solid (drawn as the floor under them; the sprite shows the chest).
    for (uint8_t c = 0; c < level_chests; c++)
        room_map[((chest_y[c] >> 4) << 5) | (chest_x[c] >> 4)] = BLK_PROP;
    pickups_on = level_chests != 0;
}

static uint8_t near(uint16_t x, uint16_t y, uint8_t reach) {
    int16_t dx = (int16_t)(player_x + 8 - x), dy = (int16_t)(player_y + 12 - y);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx <= reach && dy <= reach;
}

static uint8_t free_drop(void) {
    for (uint8_t i = 0; i < DROP_MAX; i++)
        if (drop_kind[i] == DROP_NONE) return i;
    return 0xFF;
}

static void drop_credits(uint16_t x, uint16_t y, uint16_t value) {
    uint8_t d = free_drop();
    if (d == 0xFF) return;
    drop_kind[d] = DROP_CREDITS;
    drop_x[d] = x;
    drop_y[d] = y;
    drop_value[d] = value;
    pickups_on = 1;
}

static void drop_gun(uint16_t x, uint16_t y, const Gun *g) {
    uint8_t used = 0;
    for (uint8_t i = 0; i < DROP_MAX; i++)
        if (drop_kind[i] == DROP_GUN) used |= 1 << drop_value[i];
    uint8_t r = 0;
    while (r < DROP_GUNS && (used & (1 << r))) r++;
    uint8_t d = free_drop();
    if (r == DROP_GUNS || d == 0xFF) return;
    floor_gun[r] = *g;
    weapon_icon(g->type, floor_icon[r]);
    drop_kind[d] = DROP_GUN;
    drop_x[d] = x;
    drop_y[d] = y;
    drop_value[d] = r;
    pickups_on = 1;
}

// A chest's loot: credits and a gun, on the floor in front of it. Caches (not
// guarded by a camp) are a little luckier.
static void open_chest(uint8_t c) {
    chest_open_bits |= 1 << c;
    area_chests_opened[level_map] |= 1 << c;
    chests_opened++;
    sfx_play(SFX_CHEST);
    uint16_t x = chest_x[c], y = chest_y[c] + 18;
    drop_credits(x - 10, y, (uint16_t)(10 + (rand() & 15)) * layer_depth);
    Gun g;
    uint8_t luck = (layer_depth - 1) * 4 + (chest_camp[c] == 0xFF ? 6 : 0);
    uint8_t lv = profile.level + (rand() & 1);
    loot_roll(&g, lv > 1 ? lv - (rand() & 1) : 1, luck);
    drop_gun(x + 10, y, &g);
    // Opened for good: into the file now (a continue mustn't refill it).
    profile.exp_chests[level_map] = area_chests_opened[level_map];
    save_write();
}

void loot_kill_drop(void) BANKED {
    if (!pickups_map) return;  // (only where loot can lie: the descent's maps)
    if ((rand() & 3) == 0) drop_credits(kill_x, kill_y, (uint16_t)(1 + (rand() & 3)) * layer_depth);
}

// --- the card: a gun on the floor against the one held ---

static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

static char *put_num(char *p, int16_t n, uint8_t sign) {
    if (n < 0) *p++ = '-', n = -n;
    else if (sign) *p++ = '+';
    char d[5];
    uint8_t k = 0;
    do {
        d[k++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (k) *p++ = d[--k];
    return p;
}

// " (+n)" when they differ.
static char *put_diff(char *p, int16_t d) {
    if (!d) return p;
    p = put(p, " (");
    p = put_num(p, d, 1);
    return put(p, ")");
}

static uint8_t card_tile;

static void card_line(uint8_t row, const char *s, uint8_t pal) {
    text_on_window = 1;
    card_tile += text_draw(FONT_COMPACT, 11, row, s, card_tile, pal | TEXT_BANK1);
    text_on_window = 0;
}

// Shots a second, from frames between them.
static int16_t rate(const WeaponStats *w) {
    uint8_t f = w->mode == FIRE_SEMI && w->tap_interval ? w->tap_interval : w->fire_interval;
    return f ? 60 / f : 0;
}

static void card_draw(void) {
    const Gun *g = &floor_gun[drop_value[card_drop]];
    WeaponStats n = weapon_stats[g->type];
    gun_apply(&n, g);
    if (skill_fx) skills_weapon(&n);
    const WeaponStats *h = &wpn;
    char line[TEXT_MAX + 1], name[24];
    char *p;
    card_tile = CARD_TILES;
    win_frame(0, 0, SCREEN_TW, CARD_ROWS);
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, CARD_ROWS, BG_PAL_UI);
    VBK_REG = 0;
    pal_base_bg[CARD_PAL + 3] = rarity_ink[g->rarity];
    gfx_set_fade(0);
    gun_name(name, g);
    p = put(line, name);
    p = put(p, "  LV ");
    p = put_num(p, g->level, 0);
    *p = 0;
    card_line(1, line, BG_PAL_HEALTH);
    p = put(line, rarity_names[g->rarity]);
    *p = 0;
    card_line(2, line, BG_PAL_HEALTH);
    // What A does: into a free slot, or a swap for the gun held.
    uint8_t full = 1;
    for (uint8_t i = 0; i < LOADOUT; i++) if (profile.guns[i].type == GUN_NONE) full = 0;
    p = put(line, full ? "A: swap" : "A: take");
    *p = 0;
    text_on_window = 1;
    card_tile += text_draw(FONT_COMPACT, 112, 2, line, card_tile, BG_PAL_UI | TEXT_BANK1);
    text_on_window = 0;
    // Damage (x pellets), and shots a second or the magazine and reload.
    p = put(line, "DMG ");
    p = put_num(p, n.damage, 0);
    if (n.pellets > 1) *p++ = 'x', p = put_num(p, n.pellets, 0);
    p = put_diff(p, (int16_t)n.damage * n.pellets - (int16_t)h->damage * h->pellets);
    if (n.mode != FIRE_MELEE) {
        p = put(p, "   RATE ");
        p = put_num(p, rate(&n), 0);
        p = put_diff(p, rate(&n) - rate(h));
    }
    *p = 0;
    card_line(3, line, BG_PAL_UI);
    if (n.mag) {
        p = put(line, "MAG ");
        p = put_num(p, n.mag, 0);
        p = put_diff(p, (int16_t)n.mag - h->mag);
        p = put(p, "   RELOAD ");
        p = put_num(p, n.reload / 6 / 10, 0);
        *p++ = '.';
        p = put_num(p, n.reload / 6 % 10, 0);
        *p++ = 's';
        *p = 0;
    } else {
        p = put(line, "A blade: no magazine.");
        *p = 0;
    }
    card_line(4, line, BG_PAL_UI);
    move_win(7, 0);
    gfx_box_split(CARD_ROWS * 8);
}

static void card_show(uint8_t d) {
    if (card_drop == CARD_NONE) {
        memcpy(card_saved_pal, &pal_base_bg[CARD_PAL], sizeof(card_saved_pal));
        hud_hide();
    }
    card_drop = d;
    card_draw();
}

static void card_hide(void) {
    card_drop = CARD_NONE;
    memcpy(&pal_base_bg[CARD_PAL], card_saved_pal, sizeof(card_saved_pal));
    gfx_set_fade(0);
    gfx_box_split(0);
    hud_draw();
    move_win(7, VIEW_H);
    gfx_sprite_split(VIEW_H);
}

void pickups_card_off(void) BANKED {
    if (card_drop != CARD_NONE) card_hide();
}

void pickups_take(void) BANKED {
    if (card_drop == CARD_NONE) return;
    uint8_t d = card_drop, r = drop_value[d];
    Gun g = floor_gun[r];
    uint8_t s = 0;
    while (s < LOADOUT && profile.guns[s].type != GUN_NONE) s++;
    if (s < LOADOUT) {
        drop_kind[d] = DROP_NONE;  // into a free slot
    } else {
        s = profile.gun_sel;       // swapped for the one held: it lies where this one was
        floor_gun[r] = profile.guns[s];
        weapon_icon(floor_gun[r].type, floor_icon[r]);
    }
    profile.guns[s] = g;
    slot_ammo[s] = 255;  // a full magazine
    if (s == profile.gun_sel) gun_mods = 0;  // (don't keep the old gun's magazine for it)
    loadout_select(s);
    guns_taken++;
    sfx_play(SFX_PICKUP);
    save_write();
    card_hide();  // (shows again next frame if a gun is still underfoot)
}

void pickups_update(void) BANKED {
    pickups_tick++;
    // Chests: a camp's opens once the camp is cleared; caches any time.
    for (uint8_t c = 0; c < level_chests; c++) {
        if (chest_open_bits & (1 << c)) continue;
        uint8_t camp = chest_camp[c];
        if (camp != 0xFF && camp_state[camp] != CAMP_CLEARED) continue;
        if (near(chest_x[c] + 8, chest_y[c] + 8, OPEN_REACH)) open_chest(c);
    }
    // Drops underfoot: credits are taken at once; a gun shows its card.
    uint8_t card = CARD_NONE;
    for (uint8_t i = 0; i < DROP_MAX; i++) {
        if (drop_kind[i] == DROP_NONE || !near(drop_x[i] + 8, drop_y[i] + 8, TAKE_REACH)) continue;
        if (drop_kind[i] == DROP_CREDITS) {
            profile.credits += drop_value[i];
            credits_picked += drop_value[i];
            char t[8] = "+    CR";
            uint16_t v = drop_value[i] > 999 ? 999 : drop_value[i];
            for (int8_t k = 3; k >= 1; k--) {
                t[k] = '0' + v % 10;
                v /= 10;
                if (!v) break;
            }
            hud_say(t);
            sfx_play(SFX_PICKUP);
            drop_kind[i] = DROP_NONE;
        } else if (card == CARD_NONE) {
            card = i;
        }
    }
    if (card != card_drop) {
        if (card == CARD_NONE) card_hide();
        else card_show(card);
    }
}

// A 16x16 frame at room pixel (x, y) in sprites oam, oam + 1 (a column with tile
// 0xFF is left out); returns the next free sprite.
static uint8_t draw16(uint8_t oam, const uint8_t *f, uint16_t x, uint16_t y) {
    uint16_t sx = x - cam_x + 8, sy = y - cam_y + 16;
    if (sx >= 168 + 8 || sy >= VIEW_H + 16 + 16) return oam;
    for (uint8_t c = 0; c < 2; c++) {
        if (f[c << 1] == 0xFF) continue;
        set_sprite_tile(oam, f[c << 1]);
        set_sprite_prop(oam, f[(c << 1) + 1]);
        move_sprite(oam++, (uint8_t)sx + (c << 3), (uint8_t)sy);
    }
    return oam;
}

uint8_t pickups_draw(uint8_t oam) BANKED {
    uint8_t f[4];
    for (uint8_t c = 0; c < level_chests; c++) {
        uint8_t open = chest_open_bits & (1 << c) ? 4 : 0;
        uint8_t camp = chest_camp[c];
        uint8_t ready = !open && (camp == 0xFF || camp_state[camp] == CAMP_CLEARED);
        uint8_t pal = ready && (pickups_tick & 16) ? SPR_PAL_SHOT : SPR_PAL_BULLET;  // glows when it can open
        f[0] = CHEST_VRAM + (chest_frames[open] << 1);
        f[1] = (chest_frames[open + 1] & 0xF8) | pal;
        f[2] = CHEST_VRAM + (chest_frames[open + 2] << 1);
        f[3] = (chest_frames[open + 3] & 0xF8) | pal;
        oam = draw16(oam, f, chest_x[c], chest_y[c]);
    }
    uint8_t bob = (pickups_tick >> 4) & 1;
    for (uint8_t i = 0; i < DROP_MAX; i++) {
        if (drop_kind[i] == DROP_CREDITS) {
            f[0] = 0xFF;
            f[2] = COIN_VRAM;
            f[3] = SPR_PAL_SHOT;
            oam = draw16(oam, f, drop_x[i] - 4, drop_y[i] - bob);  // one sprite, in the middle
        } else if (drop_kind[i] == DROP_GUN) {
            oam = draw16(oam, floor_icon[drop_value[i]], drop_x[i], drop_y[i] - bob);
        }
    }
    return oam;
}
