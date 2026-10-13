// Placeholder sound effects (issue #13): short register-level sounds on the
// Game Boy's square channels (1: gunshots and the player's moves, 2: chimes and
// clicks) and its noise channel (4: blasts, hits, the shield breaking). Channel 3
// (wave) is left for music.
//
// Each sound is a few steps of register values; the hardware's own envelopes and
// sweeps do the shaping, so a step is written once and runs on its own. A sound
// with a higher priority cuts off the one playing on its channel; a lower one
// waits its turn (it's dropped). sfx_watch() plays most of them by noticing what
// changed in the game's counters each frame, so the hot code needs no calls.
#ifndef SFX_H
#define SFX_H

#include <gb/gb.h>
#include <stdint.h>

#define SFX_PISTOL 0
#define SFX_SHOTGUN 1
#define SFX_RIFLE 2
#define SFX_SNIPER 3
#define SFX_CRIT 4         // a sniper critical
#define SFX_SWORD 5        // a swing
#define SFX_HIT 6          // a shot or swing lands on an enemy
#define SFX_KILL 7
#define SFX_HURT 8         // a hit that reached health
#define SFX_SHIELD_HIT 9
#define SFX_SHIELD_BREAK 10
#define SFX_SHIELD_FULL 11 // recharged
#define SFX_RELOAD 12      // a reload starts
#define SFX_RELOADED 13    // ... and ends
#define SFX_CHEST 14       // the chest opens
#define SFX_PICKUP 15
#define SFX_JUMP 16
#define SFX_DASH 17
#define SFX_ENEMY_SHOT 18  // a peashooter fires
#define SFX_DEATH 19
#define SFX_START 20       // a run starts (title)
#define SFX_DEFLECT 21     // the sword turns a bullet
#define SFX_COUNT 22

// --- watchable state ---
extern uint8_t sfx_last;      // the last sound started (0xFF: none yet)
extern uint16_t sfx_played;   // sounds started

void sfx_init(void) BANKED;          // sound on (once, at boot)
void sfx_play(uint8_t id) BANKED;
void sfx_update(void) BANKED;        // per frame: advance multi-step sounds
void sfx_watch_reset(void) BANKED;   // play scene start: take the counters as they are
void sfx_watch(void) BANKED;         // per frame in play: sounds for what changed

#endif
