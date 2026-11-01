// Missions (mission.c): jobs from the contracts board in town (board.c), carried
// down into the descent and turned in back at the board. docs/design/runs.md,
// "Missions".
#ifndef MISSION_H
#define MISSION_H

#include <gb/gb.h>
#include <stdint.h>

#define MISSION_SLOTS 3   // jobs carried at once (save.h profile.mis_*)
#define MISSION_OFFERS 4  // on the board at a time
#define MIS_NONE 0
#define MIS_CULL 1        // kill `need` of enemy kind `target` (ACTOR_*)
#define MIS_RETRIEVE 2    // bring back `need` data cores (a cleared camp yields one; lost if you die)
#define MIS_BOUNTY 3      // beat miniboss `target` (play.c's MODE_MINIBOSS kind: 0 Claimjaw ...)
#define MIS_DEBT 4        // pay the company `need` x DEBT_UNIT credits at the board
#define MIS_KINDS 4
#define DEBT_UNIT 50

typedef struct {
    uint8_t kind, target, need, have;
} Mission;

// --- watchable state ---
extern uint8_t mission_cull;  // a bit per ACTOR_* a carried cull job counts (actor.c; 0 outside a file's descent)

void mission_restock(void) BANKED;                 // new offers (each time you come into town)
void mission_offer(uint8_t k, Mission *m) BANKED;  // the board's offer k (from profile.mis_seed)
void mission_job(uint8_t s, Mission *m) BANKED;    // carried job s
uint8_t mission_take(uint8_t k) BANKED;            // offer k into a free slot: 1, or 0 (none free, or taken)
uint8_t mission_turn_in(uint8_t s) BANKED;         // job s done (a debt: paid now): rewards it, 1; else 0
void mission_drop(uint8_t s) BANKED;               // give job s up
uint8_t mission_ready(uint8_t s) BANKED;           // job s can be turned in (a debt: you have the credits)
uint16_t mission_credits(const Mission *m) BANKED; // what it pays (a debt: what it costs)
uint16_t mission_xp(const Mission *m) BANKED;
void mission_text(char *out, const Mission *m) BANKED;  // "CULL 8 LOADERS" ... (out: 24 chars)
void mission_refresh(uint8_t counting) BANKED;     // mission_cull for this scene (counting: a file's descent)
void mission_kill(uint8_t type) BANKED;            // an enemy of a counted kind fell (actor.c)
void mission_found(void) BANKED;                   // a camp cleared: a data core, if a job wants one
void mission_beat(uint8_t boss) BANKED;            // an area's miniboss beaten (play.c)
void mission_lost(void) BANKED;                    // the run ended in death: carried cores are lost

#endif
