# Bosses and minibosses: Game Boy Color combat trials

## Captain Flynt boss trial: Furnace Deck Showdown

Title → DOWN → Bosses → Captain Flynt. The Southern Shelf pirate commander in his furnace deck arena (`boss_kind 10`); 36 HP, starter pistol and shield.

- **Stalking & Positioning**:
  - Closes distance smoothly toward the player, maintaining ~48 px combat spacing before raising his harpoon cleaver.
- **Flame Cleave**:
  - 32-frame warning telegraph with flashing windup; launches a 3-projectile spreading incendiary wave aimed directly at the player. In Phase 3 (below 12 HP), expands into a 5-projectile fan.
- **Furnace Grates (Arena Hazard)**:
  - 4 iron grates in the four quadrants of the arena floor cycle through Safe (0), Steaming Warning (1), and Erupting Flame Pillars (2).
  - Standing on an active flame jet deals 8 fire damage (`hurt_cause 15`).
  - **Jumping clears active furnace vents safely** (`player_z > 0`), rewarding acrobatic movement.
- **Flame Shield / Overheat (HP 24 and 12)**:
  - At health thresholds, Flynt roars and ignites a blazing heat shield (`actor_guard = 1`), deflecting all incoming projectile fire.
  - **Leap Slam**: Leaps high into the air with visual altitude and ground shadow (in `OAM_PROP`), then slams down with camera shake, close shockwave damage, and 6 radial fire bullets.
  - **Overheat Burnout & Punish Window**: After 90 frames, his cooling radiators blow out with a blast of steam; he collapses to one knee (`FLYNT_STUNNED`, `actor_guard = 0`), giving the player a 75-frame massive damage opening.
- **Desperation Phase & Second Wind Fodder (HP < 12)**:
  - Vents cycle faster, cleaves fire 5-pellet fans, and an occasional bandit/psycho minion add spawns from the corner to provide tactical Second Wind opportunities.
- **Implementation**:
  - `src/flynt.c`, `src/flynt.h`, `boss_kind 10`.
  - Uses 4 metasprite frames in `MACHINE_VRAM` (96–111) and background tiles 144–155 for furnace grates on background palette 5.
  - Leap shadow exclusively uses `OAM_PROP` (slots 11–12), preserving all player and combat sprite slots.

## Scrapback miniboss trial

Title → DOWN → Minibosses → Scrapback. A 24x24 armored scavenger beast in a
single-screen arena; 24 HP, starter pistol and the ordinary starting shield.

- Stalk for 36 frames initially, 28 thereafter, following the player at half
  running speed. Stop within about 36 pixels.
- Shake the body and flash for a 36-frame warning. Continue tracking the
  player's position through the warning; lock the actual continuous-angle vector
  on launch. The charge never steers after launch.
- Charge roughly three pixels per frame, using fractional motion and three wall
  checks per update. The whole 24x24 footprint remains inside the arena. A charge
  hit deals 20 damage; jumping or stepping sideways avoids it.
- A wall crash uses a sustained 24-frame camera shake (0.4 seconds), decaying from three pixels,
  visibly offsets the floor and sprites even in this screen-sized arena, rebounds
  along the reverse charge vector for eight frames (roughly eight pixels), and
  opens the cyan back panel for 100 frames including the rebound.
  Grounded players anywhere in the arena are knocked down for 24 frames (0.4 s):
  the body lies sideways and movement, jumping, and attacks are disabled. Being
  airborne at impact avoids it. Knockdown itself deals no damage and temporarily
  protects against hits, including acid, so loss of control cannot chain damage.
  Four recovery-grace frames allow a jump off acid when control returns. Physical
  charge contact resolves before this protection; the shock itself adds no damage.
  Pause freezes both timers; changing encounters resets the player's stun.
  In this first prototype the entire damage hitbox is vulnerable during recovery,
  and the whole shell is guarded otherwise. Precise directional front armor and
  a separate rear-only weak-point hitbox remain follow-up work.
- Charges leave up to four 16x16 acid patches, spaced about 20 pixels apart
  (short charges leave fewer). Each patch fizzes harmlessly for 20 frames before
  turning green and dealing 8 contact damage through normal shields and hit
  invulnerability. Jumping clears it. Patches stay through crash recovery, fade
  harmlessly during its final 12 frames, and disappear before the next charge.
  Defeat removes the trail too; pause freezes it. Impact projectiles are not part
  of this pass: the trail alone is the new source of pressure.
  Patches occupy non-overlapping floor cells centered on the path; an impact-point
  patch is added when capacity allows, before Scrapback rebounds.
- Defeat clears the body and grants the existing trial completion flag. Pause
  freezes the encounter. No campaign spawns or campaign rewards are added.

Implementation: `src/scrapback.c`, boss kind 5, scripted actor 11, death cause 14.
Four rotated art headings use tiles 96–143; movement is not quantized to these
headings. The six sprite parts use prop slots 11–14 and the single actor's two
dynamic slots 15–16; player sprites and the projectile pool remain intact. The
shared 16x16 actor anchor remains the damage target inside the larger visual body.
Acid uses background tiles 144–155 and scene palette 5, rather than hardware
sprites, so shots cannot hide a damaging pool. One patch redraws per frame via
queued tile/attribute strips; cleanup restores the original room tiles. Collision
remains ordinary walkable floor, with damage checked separately.
Focused emulator checks cover arbitrary-angle aim, committed trajectories, all
four corners, actual projectile protection/damage, pause, sprite cleanup, and
normal-health pistol completion. Standing still holding fire loses. The moving
updated timed-jump victory test stays within the 131,328-cycle frame budget. Trail checks cover safe warning/fade stages, grounded damage,
jumping, pause, all three distinct animation frames, palette fade progression,
visibility with all player shot slots full, and floor restoration.
Impact tests cover recoil distance, sustained shake, ground/air outcomes, locked
controls and attacks, recovery, pause, charge contact, and encounter reset. Rare
stun and pit-respawn handling live in player_setup.c to keep fixed ROM within its
budget; the existing pit and movement tests still pass.

## Earlier trials and campaign concepts

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

Claimjaw uses a single-screen arena. Warden uses a scrolling 240-by-224 arena
with three terraces and a fixed third HUD row for boss HP and attack/chase cues.
The player health, shield, weapon and ammo display remains available. Trials
start with the same shield and use the lab's weapon/movement settings.
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
| Stair retreats | At 32 and 16 HP, close armor, clear the current volley and walk upstairs to the next terrace | Follow the stair route while clearing Husks |
| Pressure | Replenishing Spore Husk waves; three active before half health, four afterward | Make a path to the boss, then turn back to attack |
| Half health | At 24 HP, triple bursts and 54-frame recoveries | Dodge the extra blast before committing |

Each attack is a two-blast burst, with 18 frames between blasts. Below half
health it becomes three blasts. Shotgun pellets travel forward at 1.5 px/frame
with seven distinct lateral velocities across the cone; follow-ups keep the
original aim rather than tracking a dodge. Ring attacks repeat on the same cadence.
The core is orange and vulnerable throughout the burst and recovery. Jumping
across pellets is useful while moving toward a firing position. The shared live
bullet pool stays capped at 20, so a crowded third blast may emit fewer pellets.
Enemy bullets get sprite priority over player shots in this trial; existing
rotation shares the remaining OAM when more enemy pellets are alive than fit.

The terrace walls block players and projectiles. The lower staircase is on the
right, the upper staircase on the left; no jumping is required. Warden and Husks
follow the same openings and commit to a landing before turning. This is a
top-down terraced map, not vertically overlapping floors or a new 3D projectile
height system. Warden stops firing and closes its armor when the player is on
another terrace or too far away, then resumes when the chase catches up.

The first Husk wave arrives after 180 frames. Later waves check every 210 frames
(150 below half health), adding up to two enemies without exceeding the cap.
Husks have 2 HP in this encounter, spawn on the far side of the player's current
terrace, flash for 36 frames, then pursue. They use separate sprite-bank art;
normal enemy trials retain their existing art and balance. Wave navigation and
animation are spread across four frames to keep the busiest fight within budget.

A security field still claims a locked 32-by-32 square. An empty outline warns
for 60 frames, then a striped field deals 12 damage for 42 frames. Below half
health those times become 48 and 48 frames. There is a 90-frame clear interval.
This gives more breathing room than the original flat arena because Husks and
stairs now add pressure. Normal shield, jump and invulnerability rules apply.
Eight reserved sprites keep the field visible; offscreen segments are clipped.

The completion test uses real movement, jumping and starter-pistol fire to clear
the relays, climb both stairs and defeat Warden without health/damage overrides.
Standing still cannot follow the armored retreats and loses the camping test.
HP remains 48. Camera shake is disabled here to keep scrolling geometry stable.

Custom 16×16 machine sprites distinguish core and relays; the core has closed/open
drawings. All bullets are jumpable. Husk waves respect the actor limit. Shots currently pass through protected cores rather than being absorbed,
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
`src/warden.c` owns the terraced room, stair navigation, Husk waves, fixed boss
HUD row and locked floor field. Scripted
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

A control-driven test defeats both trials with normal health and shield. Warden's
route now follows the authored stairs, dodges fields and jumps nearby threats.
Geometry tests separately verify walkable stairs, both boss retreats, Husk paths
that never cut through walls, replenishment caps, fixed HUD after pause, and
victory cleanup. Crowded-pool tests cover simultaneous shots and enemies.

Validation on 2026-10-14: all 216 tests pass. The complete chase peaks at
125,180 of 131,328 update cycles (95%) with up to five actors and twenty enemy
bullets; this is a demanding encounter with limited remaining CPU headroom.
Stair, combat and Husk screenshots were inspected. Two procedural-room screenshot
baselines were refreshed after inspecting their changed seeded layouts. These
are emulator checks, not hardware certification. Human playtesting should focus
on finding the stairs, recognizing armored retreats, and clearing enough Husks
to maintain pressure on the boss.

## Next after playing

Record whether players can explain a mechanic after one attempt, time to defeat,
damage by attack, failed dodges, aiming frustration, and weapon preference. Tune
timings and safe space, then try an arena-hazard boss before expanding the roster.
Campaign rewards/dialogue, full boss health bars, unique music, reconstruction,
and larger multi-part artwork remain future work.
