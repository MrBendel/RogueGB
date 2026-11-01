#pragma bank 255
// Missions (mission.h). The board's offers come from a seed (profile.mis_seed, new
// each time you're back in town), so they cost no save space until taken; taken
// ones are copied into the profile's 3 job slots and counted as you play.
#include <gb/gb.h>
#include <rand.h>
#include "mission.h"
#include "save.h"
#include "actor.h"
#include "world.h"
#include "progress.h"
#include "hud.h"
#include "sfx.h"
#include "rescue.h"

uint8_t mission_cull;

void mission_restock(void) BANKED {
    profile.mis_seed = ((uint16_t)rand() << 8) | rand();
    profile.mis_taken = 0;
}

// A small hash of the seed and the offer (its own numbers: rand's sequence is the run's).
static uint16_t mix(uint16_t x) {
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    return x;
}

void mission_offer(uint8_t k, Mission *m) BANKED {
    uint16_t r = mix(profile.mis_seed + (uint16_t)k * 0x9E37 + 1);
    uint8_t deepest = profile.best_depth;  // how far you've been: what's fair to ask
    m->have = 0;
    m->kind = 1 + (uint8_t)(r % MIS_KINDS);
    r = mix(r);
    if (m->kind == MIS_CULL) {
        uint8_t tiers = deepest >= WORLD_TIERS ? WORLD_TIERS : deepest ? deepest : 1;
        m->target = world_roster_kind((uint8_t)(r % tiers), (uint8_t)(r >> 4) & (WORLD_ROSTER - 1));
        m->need = 4 + (((uint8_t)(r >> 8) & 3) << 1);  // 4-10
    } else if (m->kind == MIS_RETRIEVE) {
        m->target = 0;  // (data cores; more items later)
        m->need = 2 + ((uint8_t)(r >> 8) & 1) + (deepest >= 2);  // 2-4
    } else if (m->kind == MIS_RESCUE) {
        // Someone not yet rescued and not on a job already (nobody left: a cull instead).
        uint8_t busy = profile.rescued;
        for (uint8_t s = 0; s < MISSION_SLOTS; s++)
            if (profile.mis_kind[s] == MIS_RESCUE) busy |= 1 << RESCUE_WHO(profile.mis_target[s]);
        uint8_t p = (uint8_t)r & 3;
        for (uint8_t k = 0; k < PEOPLE && (busy & (1 << p)); k++) p = (p + 1) & 3;
        if (busy & (1 << p)) {
            m->kind = MIS_CULL;
            m->target = world_roster_kind(0, (uint8_t)(r >> 4) & (WORLD_ROSTER - 1));
            m->need = 6;
            return;
        }
        m->target = p | (((uint8_t)(r >> 4) % RESCUE_STYLES) << 4);
        m->need = 1 + (uint8_t)(r >> 8) % (deepest ? deepest : 1);  // an area you've reached
    } else if (m->kind == MIS_BOUNTY) {
        uint8_t n = deepest >= WORLD_MINIBOSSES ? WORLD_MINIBOSSES : deepest + 1;
        m->target = world_miniboss_kind((uint8_t)(r % n));
        m->need = 1;
    } else {  // MIS_DEBT
        m->target = 0;
        m->need = 3 + (profile.level >> 1) + ((uint8_t)(r >> 8) & 3);
    }
}

void mission_job(uint8_t s, Mission *m) BANKED {
    m->kind = profile.mis_kind[s];
    m->target = profile.mis_target[s];
    m->need = profile.mis_need[s];
    m->have = profile.mis_have[s];
}

uint8_t mission_take(uint8_t k) BANKED {
    if (profile.mis_taken & (1 << k)) return 0;
    for (uint8_t s = 0; s < MISSION_SLOTS; s++) {
        if (profile.mis_kind[s]) continue;
        Mission m;
        mission_offer(k, &m);
        profile.mis_kind[s] = m.kind;
        profile.mis_target[s] = m.target;
        profile.mis_need[s] = m.need;
        profile.mis_have[s] = 0;
        profile.mis_taken |= 1 << k;
        return 1;
    }
    return 0;
}

void mission_drop(uint8_t s) BANKED {
    profile.mis_kind[s] = MIS_NONE;
}

uint16_t mission_credits(const Mission *m) BANKED {
    uint8_t lv = profile.level;
    if (m->kind == MIS_CULL) return (uint16_t)m->need * (8 + lv);
    if (m->kind == MIS_RETRIEVE) return (uint16_t)m->need * (30 + 3 * lv);
    if (m->kind == MIS_BOUNTY) return 120 + 60 * (uint16_t)m->target + 10 * lv;
    if (m->kind == MIS_RESCUE) {  // (escorts and holds: half again)
        uint16_t c = 80 + 40 * (uint16_t)m->need + 10 * lv;
        return RESCUE_HOW(m->target) ? c + (c >> 1) : c;
    }
    return (uint16_t)m->need * DEBT_UNIT;  // a debt: what it costs
}

uint16_t mission_xp(const Mission *m) BANKED {
    if (m->kind == MIS_CULL) return (uint16_t)m->need * 6;
    if (m->kind == MIS_RETRIEVE) return (uint16_t)m->need * 20;
    if (m->kind == MIS_BOUNTY) return 60 + 30 * (uint16_t)m->target;
    if (m->kind == MIS_RESCUE) {
        uint16_t x = 40 + 20 * (uint16_t)m->need;
        return RESCUE_HOW(m->target) ? x + (x >> 1) : x;
    }
    return (uint16_t)m->need * 15;  // a debt paid: the company's goodwill
}

uint8_t mission_ready(uint8_t s) BANKED {
    Mission m;
    mission_job(s, &m);
    if (m.kind == MIS_NONE) return 0;
    if (m.kind == MIS_DEBT) return profile.credits >= mission_credits(&m);
    if (m.kind == MIS_RESCUE) return m.have;  // (need: the area)
    return m.have >= m.need;
}

uint8_t mission_turn_in(uint8_t s) BANKED {
    if (!mission_ready(s)) return 0;
    Mission m;
    mission_job(s, &m);
    if (m.kind == MIS_DEBT) profile.credits -= mission_credits(&m);
    else profile.credits += mission_credits(&m);
    if (m.kind == MIS_RESCUE) profile.rescued |= 1 << RESCUE_WHO(m.target);  // (in town from now on)
    profile.mis_kind[s] = MIS_NONE;
    xp_gain(mission_xp(&m));
    return 1;
}

static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

static char *put_num(char *p, uint16_t n) {
    char d[5];
    uint8_t k = 0;
    do {
        d[k++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (k) *p++ = d[--k];
    return p;
}

// (Plural names by kind: the Moon's look; a world could name its own later.)
static const char *const kinds[ACTOR_TYPE_COUNT] = {"", "DRONES", "TURRETS", "HOUNDS", "MERCS", "LOADERS", "MEDICS"};
static const char *const bosses[WORLD_MINIBOSSES] = {"THE CLAIMJAW", "THE SCRAPBACK", "THE CRAGBACK"};
static const char *const people[PEOPLE] = {"TESS", "OKAFOR", "NELL", "PIP"};

void mission_text(char *out, const Mission *m) BANKED {
    char *p = out;
    if (m->kind == MIS_CULL) {
        p = put(p, "CULL ");
        p = put_num(p, m->need);
        *p++ = ' ';
        p = put(p, kinds[m->target]);
    } else if (m->kind == MIS_RETRIEVE) {
        p = put(p, "FIND ");
        p = put_num(p, m->need);
        p = put(p, " DATA CORES");
    } else if (m->kind == MIS_BOUNTY) {
        p = put(p, "BEAT ");
        p = put(p, bosses[m->target]);
    } else if (m->kind == MIS_RESCUE) {
        p = put(p, "RESCUE ");
        p = put(p, people[RESCUE_WHO(m->target)]);
    } else if (m->kind == MIS_DEBT) {
        p = put(p, "PAY ");
        p = put_num(p, mission_credits(m));
        p = put(p, " CR DEBT");
    }
    *p = 0;
}

void mission_refresh(uint8_t counting) BANKED {
    mission_cull = 0;
    if (!counting) return;
    for (uint8_t s = 0; s < MISSION_SLOTS; s++)
        if (profile.mis_kind[s] == MIS_CULL && profile.mis_have[s] < profile.mis_need[s])
            mission_cull |= 1 << profile.mis_target[s];
}

// Progress on job s: "JOB 3/8" on the HUD (after whatever it's showing).
static void progress(uint8_t s) {
    uint8_t have = ++profile.mis_have[s], need = profile.mis_need[s];
    char t[8] = "JOB    ";
    // "JOB 3/8", or for two digits just "10/10".
    char *p = put_num(need >= 10 ? t : t + 4, have);
    *p++ = '/';
    p = put_num(p, need);
    while (p < t + 7) *p++ = ' ';
    t[7] = 0;
    hud_say_next(t);
    if (have >= need) sfx_play(SFX_PICKUP);
}

void mission_kill(uint8_t type) BANKED {
    for (uint8_t s = 0; s < MISSION_SLOTS; s++)
        if (profile.mis_kind[s] == MIS_CULL && profile.mis_target[s] == type && profile.mis_have[s] < profile.mis_need[s]) {
            progress(s);
            mission_refresh(1);  // (done: stop counting its kind)
            return;
        }
}

void mission_found(void) BANKED {
    for (uint8_t s = 0; s < MISSION_SLOTS; s++)
        if (profile.mis_kind[s] == MIS_RETRIEVE && profile.mis_have[s] < profile.mis_need[s]) {
            progress(s);
            return;
        }
}

void mission_beat(uint8_t boss) BANKED {
    for (uint8_t s = 0; s < MISSION_SLOTS; s++)
        if (profile.mis_kind[s] == MIS_BOUNTY && profile.mis_target[s] == boss && !profile.mis_have[s]) progress(s);
}

void mission_rescued(uint8_t s) BANKED {
    char t[8] = "SAFE!  ";
    profile.mis_have[s] = 1;
    hud_say_next(t);
    sfx_play(SFX_PICKUP);
}

void mission_lost(void) BANKED {
    for (uint8_t s = 0; s < MISSION_SLOTS; s++)
        if (profile.mis_kind[s] == MIS_RETRIEVE || profile.mis_kind[s] == MIS_RESCUE) profile.mis_have[s] = 0;
}

// What a rescue asks, for the board: "Area 2: free them." ...
void mission_rescue_text(char *out, const Mission *m) BANKED {
    static const char *const how[RESCUE_STYLES] = {": free them.", ": walk them out.", ": hold while they cut free."};
    char *p = put(out, "Area ");
    p = put_num(p, m->need);
    p = put(p, how[RESCUE_HOW(m->target)]);
    *p = 0;
}
