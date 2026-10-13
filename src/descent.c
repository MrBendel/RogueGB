#pragma bank 255
// The descent: a generated layer (level.c) played room by room. Walking through
// a door fades out, builds the next room from its seed, puts the player at the
// matching door and fades back in; health, ammo and the rest carry over.
// Combat rooms spawn enemies at their chunks' slots until they're cleared.
// Banked: a few compares a frame, the rest only on a room change.
#include <gb/gb.h>
#include <rand.h>
#include "descent.h"
#include "level.h"
#include "room.h"
#include "player.h"
#include "camera.h"
#include "actor.h"
#include "gfx.h"
#include "hud.h"

uint8_t layer_depth;      // 1 for the first layer
uint16_t rooms_entered;   // room changes this run (watchable)
uint8_t room_live;        // the room's enemies are still being fought

extern uint8_t actor_count;  // enemies alive (actor.c; actors_active lags a frame behind spawning)

// A new layer for this run, starting in its start room.
void descent_start(uint16_t seed) BANKED {
    layer_depth = 1;
    rooms_entered = 0;
    level_generate(seed);
    descent_enter(layer_start, DESCENT_NO_DOOR);
}

// Spawn the room's enemies: at its enemy slots, as many as the room offers up
// to 3 + depth, a third of them peashooters, none right beside the player.
static void spawn_room(void) {
    uint8_t n = 3 + layer_depth;
    room_live = 0;
    for (uint8_t s = 0; s < level_slots && n; s++) {
        uint16_t x = level_slot_x[s], y = level_slot_y[s];
        if (room_block_at(x + 8, y + 8) != BLK_FLOOR) continue;  // the room's edge or a door took it
        int16_t dx = (int16_t)(x - player_x), dy = (int16_t)(y - player_y);
        if (dx > -48 && dx < 48 && dy > -48 && dy < 48) continue;
        actor_spawn((rand() & 3) == 0 ? ACTOR_PEASHOOTER : ACTOR_SLIME, x, y);
        room_live = 1;
        n--;
    }
}

// Where the player stands on arriving through door `side` (the side of the new
// room they enter by): just inside its 2-block gap.
static void place_player(uint8_t side) {
    uint16_t mx = (uint16_t)((room_w >> 1) - 1) * BLOCK_SIZE, my = (uint16_t)((room_h >> 1) - 1) * BLOCK_SIZE;
    switch (side) {
    case DOOR_N: player_x = mx + 8; player_y = 2 * BLOCK_SIZE - 8; break;
    case DOOR_S: player_x = mx + 8; player_y = room_px_h - 3 * BLOCK_SIZE + 4; break;
    case DOOR_W: player_x = 2 * BLOCK_SIZE; player_y = my + 4; break;
    case DOOR_E: player_x = room_px_w - 3 * BLOCK_SIZE; player_y = my + 4; break;
    default: player_x = mx + 8; player_y = my + 4; break;  // the middle
    }
    player_xs = player_ys = 0;
}

void descent_enter(uint8_t room, uint8_t side) BANKED {
    level_room = room;
    level_build_room(room);
    place_player(side);
    rooms_entered++;
    if (layer_type[room] == ROOM_COMBAT && !layer_cleared[room]) spawn_room();
    else room_live = 0;
    if (layer_type[room] == ROOM_EXIT) {
        char t[8] = "EXIT   ";
        hud_say(t);
    }
}

// The neighbouring room through door d, and the side of it the player comes in by.
static uint8_t through(uint8_t d, uint8_t *side) {
    switch (d) {
    case DOOR_N: *side = DOOR_S; return level_room - LAYER_W;
    case DOOR_S: *side = DOOR_N; return level_room + LAYER_W;
    case DOOR_E: *side = DOOR_W; return level_room + 1;
    default: *side = DOOR_E; return level_room - 1;
    }
}

uint8_t descent_door(void) BANKED {
    // A door's gap is in the outer ring of blocks; stepping into it leaves.
    int16_t cx = (int16_t)player_x + 8, fy = (int16_t)player_y + 12;
    uint8_t d = 0;
    if (cx < BLOCK_SIZE) d = DOOR_W;
    else if (cx > (int16_t)room_px_w - BLOCK_SIZE) d = DOOR_E;
    else if (fy < BLOCK_SIZE) d = DOOR_N;
    else if (fy > (int16_t)room_px_h - BLOCK_SIZE) d = DOOR_S;
    if (!(layer_doors[level_room] & d)) return 0;
    return d;
}

void descent_update(void) BANKED {
    if (room_live && !actor_count) {
        room_live = 0;
        layer_cleared[level_room] = 1;
        char t[8] = "CLEAR! ";
        hud_say(t);
    }
}

void descent_change_room(uint8_t d) BANKED {
    uint8_t side, next = through(d, &side);
    fade_out();
    DISPLAY_OFF;
    gfx_sprites(0);
    actors_clear();
    descent_enter(next, side);
    camera_init(player_x + 8, player_y + 8);
    player_draw();
    DISPLAY_ON;
    gfx_sprites(1);
    fade_in();
}
