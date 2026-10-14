# Bosses and minibosses: Game Boy Color combat trials

Claimjaw and Warden validate the combat design before building a campaign
roster; the larger [Collector gunship](gunship.md) is a third standalone trial.
These are original Skavengar concepts inspired by sections A2 and A3 of
[the local Borderlands notes](../inspiration/borderlands-2.md). Names and campaign
placement are provisional. Further concepts below are not implemented features.

## Playing the trials

Run `npm run play`. On the title, press **DOWN** to open the playtest submenu.
Use **UP/DOWN** to choose a category and **A/START** to open it, then select an
encounter with **A/START**. **Minibosses** contains Claimjaw; **Bosses** contains
Warden and the [Collector Ship](gunship.md). **Enemies** offers one Slime, one Peashooter, or a mixed group of two of
each, in a compact arena with the starter shield and no escalating waves.
**B** returns to the previous menu. **Movement Lab** launches the existing lab.
On the main title, START still begins a descent and SELECT still opens the lab.

- D-pad: move. A: attack with auto-aim.
- B: jump; B again in mid-air: roll/dash.
- SELECT tap: cycle weapons. SELECT+B: reload.
- START: pause/tune. SELECT while paused: end the attempt. START on the results
  returns to the title for another attempt.

Both arenas fit in one 160×128 playfield above the player HUD. Boss HP and mechanic
instructions stay on the top wall. Trials grant the starter shield. Weapons and
movement retain the lab's tuning behavior; fresh boots give baseline presets.
Victory clears enemies/projectiles and awards one **trial-only salvage token**
(`boss_reward`), not campaign currency.

## Design principles

- A miniboss tests one new decision. A major boss combines mechanics progressively.
- Use warning, committed attack, and recovery. A charge cannot track a dodge after
  the warning begins.
- Keep important threats on screen. Readability matters more than a huge sprite.
- Telegraph with state, text, and flashes; don't depend on subtle color differences.
- Auto-aim excludes protected bodies and can select support targets. Translate
  precise weak-point aiming into exposed damage windows for this first experiment.
- Use a few readable projectiles and clear safe spaces. Let multiple weapons work.
- Tune timing, positioning, and counterplay before increasing health.

## Built miniboss: Claimjaw

**Fiction:** a corporate salvage clamp used to seize unauthorized equipment. A
cooling vent opens after it commits to a rush.

**Reference:** heavy enemies with state changes and weak-point opportunities.
The locked charge is our adaptation, not a copy of a specific Borderlands boss.

| Beat | Implementation | Player response |
|---|---|---|
| Exposed | 18 HP; initial 90-frame opening | Shoot or approach for melee |
| Lock | 42-frame warning, flashing body, header names cardinal direction | Step out of the lane or prepare jump/roll |
| Charge | 2 px/frame, up to 44 frames, stops before walls; protected | Dodge the committed path |
| Vent | 100-frame damage window | Attack, close for a combo, or reload |

One actor, no enemy bullets. Contact, jumping, and roll invulnerability use existing
rules. Melee cannot push machines out of the arena.

**Human playtest:** can players read and dodge the first charge? Is the opening
obvious? Is sword range usable without unavoidable contact? Can players reload safely?

## Built boss: Relay Warden

**Fiction:** an ancient security core connected to two corporate control relays.
Destroying the relays releases its original defense routine.

**References:** shield support targets, health-threshold reinforcements, and damage
windows. Our relays provide hard protection rather than the reference's optional
healing, making target priority easy to see at this scale.

| Beat | Implementation | Player response |
|---|---|---|
| Relays | Protected 48-HP body; two 5-HP relays fire slow cardinal shots every 100 frames with warnings | Reposition to select and destroy each relay |
| First opening | Clear hostile bullets; expose core for 100 frames | Recognize protection is gone and attack |
| Fan | Lock direction during 42-frame warning, fire three shots at 1 px/frame | Move through gaps, circle, or jump |
| Recovery | 90-frame damage window | Attack or reload |
| Half health | At 24 HP, one 60-frame reinforcement warning and clear old bullets | Prepare for more targets |
| Pressure | Up to two slimes spawn away from player and wait briefly; later recoveries shorten to 60 frames | Prioritize adds or pressure the core |

Custom 16×16 machine sprites distinguish core and relays; the core has closed/open
drawings. All bullets are jumpable. Reinforcements happen once and respect the
actor limit. Shots currently pass through protected cores rather than being absorbed,
so protection cannot obstruct shots at valid targets.

**Human playtest:** does repositioning select the intended relay? Is the transition
clear? Does each weapon have room to work? Does support removal feel like progress?

## Candidate roster after these tests

| Encounter | Scale | Reference pattern | Small-screen adaptation | New work |
|---|---|---|---|---|
| Survey Twins | Miniboss duo | Duo/disarming | Scanner marks a lane for a gunner; killing scanner changes gunner behavior | Coordinated controllers and shared status |
| False Signal | Miniboss | Decoys | Three machines; the real one visibly pulses before attacking | Recognition cues and suitable auto-aim |
| Furnace Custodian | Boss | Rising arena hazard | Warn and activate one floor lane while preserving a safe route; expose core between cycles | Hazard tiles, height rules, collision/visual timing |
| Debt Collector | Boss | Air/mortar phase | Mark a few landing spots, then return to an exposed ground phase | Ground markers, delayed impacts, altitude cues |
| Network Bailiff | Late boss | Combined learned patterns | Relays plus alternating hazardous lanes, taught separately first | Proven support and hazard systems |

Element-cycling bosses should wait for established elements and reliable weapon
access. Strict timer bosses should wait for gear balance so they test mastery
rather than loot luck.

## Implementation and evidence

`src/boss.c` owns arenas, compact art, state machines, and status text. Scripted
actors reuse the normal actor/hitbox/OAM pools. `actor_guard` is checked by shots,
melee damage, and auto-aim. Normal enemies retain their existing AI. Bosses are
standalone trials, not yet encounters inside the generated descent.

Art takes 16 sprite tiles at 96–111 in VRAM bank 0, replacing chest art only in
trials. Pool limits remain 8 actors, 20 enemy bullets, and 16 player shots. Dynamic
encounter text is queued for vblank; encounter logic lives in switchable ROM.

`tests/boss.test.mjs` checks selection, locked charges, successful sidestepping,
walls, protected targeting, actual shot collisions, melee protection, relay
destruction, one-time reinforcements, pause, death/restart, reward uniqueness,
finite pools, and cleanup of both logical actors and drawn sprites.

A patrol bot defeats both using only movement and starter-pistol fire with normal
health and shield, without healing or damage injection. One observed run finished
Claimjaw in 1,142 frames (about 19 seconds) at 60 HP, and Warden in 2,302 frames
(about 38 seconds) at 47 HP. This demonstrates a viable route, not final balance
or first-time player comprehension. Another test exercises a full actor/bullet pool
during the reinforcement transition while firing a shotgun.

Validation on 2026-10-13: all 190 tests pass, including submenu navigation, back
navigation, enemy selection, clear state, and clean re-entry. Peak measured update costs were
42,316 cycles for Claimjaw, 57,992 for Warden, and 73,488 for the full-pool stress
test, against the 131,328-cycle update budget in CGB double-speed mode. The stress
case uses about 56% of that budget. Emulator CPU measurements are regression
evidence, not cycle-accurate hardware certification.
Verify timing in a second emulator and on hardware before release.

## Next after playing

Record whether players can explain a mechanic after one attempt, time to defeat,
damage by attack, failed dodges, aiming frustration, and weapon preference. Tune
timings and safe space, then try an arena-hazard boss before expanding the roster.
Campaign rewards/dialogue, full boss health bars, unique music, reconstruction,
and larger multi-part artwork remain future work.
