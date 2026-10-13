// Hardware sprite (OAM) layout during play. Where sprites overlap, a lower index
// is drawn on top (on the DMG, after the smaller x coordinate), so the order is
// front to back: the muzzle flash over the gun, the gun over the body when it's
// held in front, the body, the gun when it's held behind (aiming up), then the
// shadow and the aim reticle, then props (the chest). Actors, the player's shots and enemy bullets share
// the rest, handed out every frame by actors_update.
//
// Also used by aim_score.s (RETICLE_OAM) and the tests.
#ifndef OAM_H
#define OAM_H

#define OAM_FLASH 0        // 1 sprite: muzzle flash
#define OAM_GUN_FRONT 1    // 2 sprites: the held weapon, in front of the body
#define OAM_PLAYER 3       // 2 sprites: the player's body
#define OAM_GUN_BACK 5     // 2 sprites: the held weapon, behind the body
#define OAM_SHADOW 7       // 2 sprites: the player's shadow while in the air
#define OAM_RETICLE 9      // 2 sprites: the aim reticle (or the reload bar)
#define OAM_PROP 11        // 4 sprites: the arena's chest, and the shield it holds (chest.c)
#define OAM_FIRST_ACTOR 15 // actors, shots, bullets

#endif
