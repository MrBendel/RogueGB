#pragma bank 255
// Run-over screen: stats for the run that just ended. START returns to the title.
// This is where meta-progression (spending gold, picking an heir) will hook in.
#include <gb/gb.h>
#include "waves.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"
#include "health.h"
#include "save.h"

extern uint16_t saved_seconds;  // descent.c

extern uint16_t enemies_killed;  // aim.c

static const char *const causes[] = {"", "A SLIME", "A PEASHOOTER", "A PEA", "A PIT",
    "A SPORE HUSK", "A RAGEBOUND", "A CONTRACTOR", "A BASTION", "A SCRAP HOUND", "THE COLLECTOR", "ACID SKIMMER", "A MERCENARY", "AN ACID WOLF"};
static uint8_t len(const char *s) {
    uint8_t n = 0;
    while (s[n]) n++;
    return n;
}

// Ignore input briefly so a held or mashed button doesn't skip the screen.
#define INPUT_DELAY 30

uint8_t run_over_timer;

// The run's end, for a file (save.h): the corporation rebuilds you for a cut of
// your credits (docs/design/play-loop.md), the expedition is over, and the
// file is saved.
static void settle(void) {
    if (save_slot == SAVE_NONE) return;
    profile.last_fee = profile.credits >> DEATH_FEE_SHIFT;
    profile.credits -= profile.last_fee;
    profile.exp_active = 0;
    profile.kills += enemies_killed;
    save_add_time(run_seconds - saved_seconds);
    save_write();
}

void run_over_enter(void) BANKED {
    run_over_timer = 0;
    settle();
    palettes_menu();
    bkg_frame(2, 2, 16, 12);
    bkg_text(6, 3, "RUN OVER");
    if (death_cause) {
        const char *c = causes[death_cause];
        bkg_text(5, 5, "KILLED BY");
        bkg_text((SCREEN_TW - len(c)) >> 1, 6, c);
    } else {
        bkg_text(5, 5, "RUN ENDED");
    }
    bkg_text(4, 8, "TIME");
    bkg_number(11, 8, run_seconds / 60, 2, 1);
    bkg_text(13, 8, ":");
    bkg_number(14, 8, run_seconds % 60, 2, 1);
    bkg_text(4, 9, "KILLS");
    bkg_number(12, 9, enemies_killed, 4, 0);
    bkg_text(4, 10, "WAVES");
    bkg_number(12, 10, waves_cleared, 4, 0);
    bkg_text(5, 15, "PRESS START");
}

void run_over_update(void) BANKED {
    if (run_over_timer < INPUT_DELAY) {
        run_over_timer++;
        return;
    }
    if (!(joy_pressed & (J_START | J_A))) return;
    if (save_slot != SAVE_NONE) {
        // Rebuilt, back in town (the hub).
        play_mode = MODE_YARD;
        run_start(run_new_seed());
        state_change(STATE_PLAY);
    } else {
        state_change(STATE_TITLE);
    }
}
