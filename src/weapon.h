// Weapons: what the player holds and how it fires (see ROADMAP "Weapons").
//
// Five classes for now, switched with SELECT: a pistol, a shotgun, an assault
// rifle, a sniper rifle and a sword. Each is a set of stats (WeaponStats) plus art: 16
// aiming directions drawn by tools/make-weapon-art.mjs. Every weapon's art is
// loaded into sprite VRAM at the start of a scene, so switching is instant: it
// copies the chosen one's frame and placement tables into the fixed RAM tables
// the drawing code reads (gun_frames, gun_place).
//
// The firing itself (modes, pellets, recoil) is in aim.c.
#ifndef WEAPON_H
#define WEAPON_H

#include <gb/gb.h>
#include <stdint.h>

#define WEAPON_PISTOL 0
#define WEAPON_SHOTGUN 1
#define WEAPON_RIFLE 2
#define WEAPON_SNIPER 3
#define WEAPON_SWORD 4
#define WEAPON_COUNT 5

// How holding and tapping A fire.
#define FIRE_SEMI 0    // each press fires at once (tap_interval apart); holding fires every fire_interval
#define FIRE_AUTO 1    // holding fires every fire_interval
#define FIRE_RIFLE 2   // the assault rifle: tap, burst or full auto, by rifle_mode (below)
#define FIRE_CHARGE 3  // holding steadies for windup frames and fires a critical shot (2x damage);
                       // letting go earlier fires a normal one
#define FIRE_MELEE 4   // each press swings (aim.c): a 3-hit combo if pressed within tap_interval
                       // frames of the last swing; range = reach, kick = knockback (px)

typedef struct {
    uint8_t mode;           // FIRE_*
    uint8_t fire_interval;  // frames between shots while A is held
    uint8_t tap_interval;   // FIRE_SEMI: frames between shots when tapping
    uint8_t pellets;        // projectiles per shot
    uint8_t spread;         // 32-steps between pellets (a fan around the aim)
    uint8_t jitter;         // random 32-steps either way, per shot
    uint16_t speed;         // px/frame 8.8
    uint8_t range;          // px a shot flies
    uint8_t aim_range;      // px: farthest target
    uint8_t damage;         // HP per hit
    uint8_t kick;           // px the gun kicks back (1-3)
    uint8_t push;           // shoves the player back by shot velocity >> push (0 = no shove)
    uint8_t windup;         // FIRE_CHARGE: frames to steady; FIRE_RIFLE: the pause after a burst
    uint8_t slow;           // halve walking speed while A is held
    uint8_t mag;            // rounds per magazine (a shotgun blast is one); 0 = never reloads
    uint8_t reload;         // frames to reload
} WeaponStats;

// Reloading (aim.c): automatic when the magazine empties, or SELECT+B any time
// it isn't full (play.c). A reload can't be cut short except by switching
// weapons, which cancels it. Each weapon keeps its own magazine.
extern uint8_t weapon_ammo[];  // rounds left in each weapon's magazine
extern uint8_t reload_t;       // frames of the current reload left (0 = not reloading)
extern uint16_t reloads;       // reloads started (watchable)
// Start a reload if the held weapon has a magazine that isn't full (SELECT+B).
void aim_reload(void) BANKED;

// The assault rifle's trigger (FIRE_RIFLE). A setting: for now on the pause
// panel; later on an options screen off the title.
#define RIFLE_TAP 0    // one shot per press (tap_interval apart at most)
#define RIFLE_BURST 1  // each press fires BURST_SHOTS, BURST_GAP frames apart, then pauses windup frames
#define RIFLE_AUTO 2   // holding fires every fire_interval
#define RIFLE_MODES 3
#define BURST_SHOTS 3
#define BURST_GAP 4
extern uint8_t rifle_mode;

// --- watchable state ---
extern uint8_t weapon_cur;                 // WEAPON_*
extern WeaponStats wpn;                    // the current weapon's stats (tunable)
extern WeaponStats weapon_stats[WEAPON_COUNT];  // each weapon's (tuned) stats

// The held weapon's art, for the drawing code (aim.c): VRAM tile base, the empty
// column (a frame using it there needs no sprite), the frame table (left column,
// flags, right column, flags per direction), and its placement per direction:
// rows GX, GY (the frame's top-left), MX, MY (the muzzle), KX, KY (the recoil
// kick, scaled to the weapon's kick), BEHIND (drawn behind the body); all
// relative to the player's 16x16 frame.
#define PLACE_GX 0
#define PLACE_GY 1
#define PLACE_MX 2
#define PLACE_MY 3
#define PLACE_KX 4
#define PLACE_KY 5
#define PLACE_BEHIND 6
extern uint8_t gun_vram, gun_blank;
extern uint8_t gun_frames[16 * 4];
extern const uint8_t *gun_tiles;  // the held weapon's art in ROM, in gun_tiles_bank
extern uint8_t gun_tiles_bank;
// The held weapon's name (7 characters, space-padded, and a 0) into dst[8].
void weapon_get_name(char *dst) BANKED;
extern int8_t gun_place[7][16];
// The sword's smeared frames (the same placement), shown while a swing can hit.
extern uint8_t smear_vram, smear_blank;
extern uint8_t smear_frames[16 * 4];
// What draw_gun (aim.c) draws this frame: the held weapon's frames, or the smear's.
extern uint8_t draw_vram, draw_blank;
extern uint8_t *draw_frames;

// Load every weapon's art into sprite VRAM bank 1 from tile 0 (display off);
// the first run also sets each weapon's stats from its preset.
void weapon_init(void) BANKED;
// Hold weapon w: save the current one's (tuned) stats, load w's stats and art.
void weapon_select(uint8_t w) BANKED;
// The current weapon's name on the window layer at (x, y) (its strings are in
// weapon.c's bank, so it draws them itself).
void weapon_draw_name(uint8_t x, uint8_t y) BANKED;
// A weapon type's look lying on the floor (its right-facing frame): f[0] the left
// column's sprite tile (0xFF: empty), f[1] its flags (palette, bank, flips), f[2],
// f[3] the right column's.
void weapon_icon(uint8_t type, uint8_t *f) BANKED;

#endif
