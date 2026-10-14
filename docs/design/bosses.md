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
| Reposition / burst | Close grey armor, lock a cardinal direction, move between x=104 and x=40 during a 42-frame warning; alternate seven-pellet shotgun cones and eight-shot rings | Read the header; leave the locked shotgun cone or use gaps in the ring |
| Recovery | Orange exposed core for 72 frames (54 below half health) | Attack or reload while watching floor claims |
| Half health | At 24 HP, one 60-frame reinforcement warning and clear old bullets | Prepare for more targets |
| Pressure | Up to two slimes spawn away from player and wait briefly; later recoveries shorten to 54 frames | Prioritize adds or pressure the core |

Each attack is a two-blast burst, with 18 frames between blasts. Below half
health it becomes three blasts. Shotgun pellets travel forward at 1.5 px/frame
with seven distinct lateral velocities across the cone; follow-ups keep the
original aim rather than tracking a dodge. Ring attacks repeat on the same cadence.
The core is orange and vulnerable throughout the burst and recovery. Jumping
across pellets is useful while moving toward a firing position. The shared live
bullet pool stays capped at 20, so a crowded third blast may emit fewer pellets.
Enemy bullets get sprite priority over player shots in this trial; existing
rotation shares the remaining OAM when more enemy pellets are alive than fit.

Throughout the fight, a security field claims a locked 32-by-32 square centered
on the player's position. An empty outline warns for 48 frames; diagonal stripes
then mark dangerous floor for 60 frames, dealing 24 damage using normal shield,
jump and invulnerability rules. A 30-frame clear interval precedes the next mark.
Below half health, warning drops to 36 frames and the field lasts 72. The mark
never tracks once placed. Leave the outline before it fills; jumping can cross
the active field, but does not remove it. Eight reserved OAM entries keep the
field visible when the shot pool is full.

Previously, holding A at spawn won in 2,284 frames. Updated tests require that
strategy to lose at spawn and all four corners, while real movement and starter
pistol fire can still win without health or damage overrides. HP remains 48.

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

`src/boss.c` owns arenas, compact art, state machines, and status text.
`src/warden.c` owns the locked floor field and its reserved sprites. Scripted
actors reuse the normal actor/hitbox/OAM pools. `actor_guard` is checked by shots,
melee damage, and auto-aim. Normal enemies retain their existing AI. Bosses are
standalone trials, not yet encounters inside the generated descent.

Machine art takes 16 sprite tiles at 96–111 in VRAM bank 0, replacing chest art only in
trials. Warden floor claims use tiles 112-127. Pool limits remain 8 actors, 20 enemy bullets, and 16 player shots. Dynamic
encounter text is queued for vblank; encounter logic lives in switchable ROM.

`tests/boss.test.mjs` checks selection, locked charges, successful sidestepping,
walls, protected targeting, actual shot collisions, melee protection, relay
destruction, one-time reinforcements, pause, death/restart, reward uniqueness,
finite pools, and cleanup of both logical actors and drawn sprites.

A patrol bot defeats both using movement, reactive jumps, and starter-pistol fire with normal
health and shield, without healing or damage injection. Before the territory-control redesign, one observed run finished
Claimjaw in 1,142 frames (about 19 seconds) at 60 HP, and Warden in 2,302 frames
(about 38 seconds) at 47 HP. This demonstrates a viable route, not final balance
or first-time player comprehension. Another test exercises a full actor/bullet pool
during the reinforcement transition while firing a shotgun.

Validation on 2026-10-14: all 214 tests pass. Holding fire without movement
loses in 398-488 frames at spawn and four corner positions. Tests also verify
locked warning positions, harmless warnings, active field damage, pause,
alternating firing positions/patterns, and visible fields with sixteen live shots.
Peak update costs: 44,108 cycles for Claimjaw, 111,404 for Warden, and 94,320
for full-pool stress, against the 131,328-cycle CGB update budget (85% at the measured Warden peak).
Shotgun spread, warning and active-field screenshots were reviewed. The generated descent-combat
screenshot baseline was refreshed after visually checking its changed seeded
layout; campaign mechanics were not changed. Emulator measurements do not replace
human balance testing or validation on hardware.

## Next after playing

Record whether players can explain a mechanic after one attempt, time to defeat,
damage by attack, failed dodges, aiming frustration, and weapon preference. Tune
timings and safe space, then try an arena-hazard boss before expanding the roster.
Campaign rewards/dialogue, full boss health bars, unique music, reconstruction,
and larger multi-part artwork remain future work.
