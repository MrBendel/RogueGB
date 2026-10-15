# Enemy families

Skavengar uses familiar combat roles with original names, silhouettes, and fiction.
The [local Borderlands notes](../inspiration/borderlands-2.md), especially A3,
inform rushers, gunners, supports, and heavies. Spore Husks are our zombie-like
addition; the supplied notes do not specify a zombie encounter to reproduce.

## Playable prototypes

From the title, press DOWN, choose Enemies with UP/DOWN and A/START, then choose
an encounter. The list scrolls; B goes back. The original Slime, Peashooter, and
Mixed Group trials remain available. START pauses; SELECT while paused ends the
attempt, and START on the results returns to the title.

| Family | Fiction | Behavior | Player response |
| --- | --- | --- | --- |
| Spore Husk | Scavengers overtaken by an alien colony | Three fragile bodies slowly close in; 3 HP each | Keep an escape route and thin the swarm |
| Ragebound | A raider driven by an unstable combat implant | 7 HP; 36-frame warning locks a cardinal lane, then a rush and 70-frame rest | Sidestep the committed rush and attack during recovery |
| Contractor | A corporate mercenary | 5 HP; retreats up close or strafes, stops to warn for 30 frames, fires three shots, reloads for 80 frames | Leave the firing lane, then close in during reload |
| Bastion Drone | A corporate salvage-security robot | 8 HP; guarded for 90 frames, warns for the last 24, drops its shield to fire a three-shot fan, stays exposed for 72 | Dodge and hit the open core; auto-aim skips the shield |
| Scrap Hound | Mutant scavenger animals with scrap lodged in their hides | Two 4-HP pack hunters; stalk, warn for 24 frames, commit to an eight-direction pounce for up to 24, recover for 42 | Bait the pounce, step out of its lane, punish the landing |

Faction Mix contains one of the original four families. Scrap Hounds have a
separate two-hound trial with staggered attack timing. This animal-rusher role
draws from skags in the reference notes; its art, name, and fiction are original.
Every trial equips the starter shield. Timers
are gameplay frames at roughly 60 Hz; pause and hitstop freeze them. Contact
uses the existing 10-damage hit and bullets use the existing 8-damage hit.

The initial designs distinguish movement, target priority, and attack windows.
They do not require precise headshots, multiple elements, or large sprites to
make the enemies different. A protected Bastion is the lowest priority until it
opens; a nearby Ragebound may become the urgent threat during its warning.

## Implementation scope

These families currently live in the enemy trials, not procedural campaign
spawns. They reuse the actor collision, damage, projectile, and OAM systems.
`actor_scripted` values 2–6 select the family controller, while value 1 remains
reserved for boss machinery. The underlying actor type is the existing shooter;
family-specific contact death messages and behavior distinguish the prototypes.
Humanoid trial actors can be knocked back by the sword. They share existing
projectile visuals and damage; unique weapons and elemental interactions remain
future work.

Each body is 16×16 pixels and uses two hardware sprites. Two small poses for each
of four simultaneously loaded families occupy 32 sprite tiles at 96–127. The
hound-only trial replaces the unused Husk art in that allocation. Those tiles replace chest or
boss graphics only in these trials. AI runs at 30 Hz in switchable ROM. Movement
checks the existing floor collision and projectile allocation obeys the existing
pool limits. The Husks use simple pursuit, not obstacle-aware pathfinding;
Contractors reposition but do not evaluate cover.

## Acid Skimmer trial (2026-10-15)

An alien flying insect that denies ground with acid, inspired by the requested
Varkid role. Select **Enemies → Acid Skimmer** in the title playtest menu.
Its original silhouette has flapping wings and a ground shadow; it stays
vulnerable to ordinary weapons throughout the fight.

- One 8-HP insect patrols across the top of a compact arena.
- A 36-frame warning marks the player's position, then a visible glob travels
  to that fixed location over 24 frames. Moving after the warning avoids it.
- The impact leaves a bubbling acid pool for 180 gameplay frames (about three
  seconds). At most two pools persist, with a 60-frame rest before targeting again.
- Ground contact deals 6 damage with a 30-frame acid cooldown. Jumping high
  enough clears the hazard. The warning itself is harmless.
- Killing the insect clears its remaining acid. Pause freezes warnings,
  projectiles, and pool timers.

The standalone controller is `src/acid_skimmer.c`, using scripted actor value 8.
Its 16×16 body, two animation poses, marker, pools, glob, and shadow occupy sprite
tiles 96–119. Eight reserved OAM entries keep hazards allocated during heavy
player fire. This prototype does not yet add campaign spawns or metamorphosis.

Focused emulator tests cover menu access, animation, target lock, damage,
jumping, pause, expiration, sprite allocation, and completion with the starter
pistol and normal health. Visuals are reviewed at the native game resolution.
All 221 repository tests pass. The normal Acid Skimmer completion test peaks at
49,476 of the 131,328 available update cycles (about 38%).

## Further enemy variants, not implemented

- An infected carrier that splits into smaller threats when defeated.
- A berserker whose armor breaks into a faster, vulnerable phase.
- A shield-bearing mercenary paired with a flanker.
- A repair robot that makes target priority matter without attacking directly.
- Planet-specific combinations, spawn budgets, drops, sounds, and encounter pacing.

First playtest the individual roles, then the mixed encounter. Record whether a
player understands the warning, knows when a robot is vulnerable, and can name
why a mixed fight calls for changing targets before adding more variants.

## Validation (2026-10-13)

All 199 repository tests pass. The family tests cover scrolling selection, pursuit
and floor bounds, locked charge direction, burst count and reload, shield immunity
and exposure, pause, contact death labels, and clearing every trial with movement
and starter-pistol fire using normal health, including the hounds' locked diagonal
pounce and recovery window. The mixed encounter peaked at 61,876
update cycles; a stress test with eight actors, twenty initial enemy bullets, and
shotgun fire peaked at 87,904 of the 131,328-cycle CGB budget (about 67%). The hound
trial peaked at 46,148 cycles. Health is
refilled only in the stress test, not the normal completion tests.

Screenshots of the menu, all six new encounters, and the death message were
visually reviewed. Measurements come from the headless emulator; hardware timing
and first-time human understanding still need playtesting.
