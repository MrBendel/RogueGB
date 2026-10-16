# Collector gunship trial

An original corporate salvage-enforcement craft, inspired by the flying-fortress
pattern in [the Borderlands notes](../inspiration/borderlands-2.md). This tests a
larger enemy and readable area attacks on the existing 160×128 arena.

## Play

Title → DOWN → Bosses → Collector Ship → A/START. Normal combat controls apply.
START pauses; SELECT while paused ends the attempt. The starter shield is equipped.

## Fight loop

| Phase | Behavior | Counterplay |
| --- | --- | --- |
| Bombing run | Armored ship patrols overhead for 180 frames. Three strikes mark your current floor position, wait 36 frames, then explode for 12 frames. | Leave each 16×16 marker; it does not follow you. Bombs deal 12 damage. |
| Laser warning | Ship moves to the left edge, then starts a full 60-frame warning. A blinking harmless line stays aligned beneath the ship; text announces the sweep direction. | Prepare to pass behind the beam or jump across it. |
| Laser sweep | Beam moves right for 96 frames and deals 10 damage on contact. The belly core opens. | Dodge and attack the exposed core when safe. |
| Cooldown | Core remains exposed for 120 frames before another bombing run. | Concentrate damage during the quiet window. |

The boss has 40 HP. Below half health, subsequent laser warnings shorten to 48
frames. Pause and hitstop freeze the fight. Only the exposed belly core takes
weapon damage; auto-aim ignores it while armored. Victory grants one trial-only
salvage token and removes the ship and active attacks.

Bombs target the lower fighting floor (y=56–111); the laser also covers that
floor. The upper strip is a retreat route, though touching the ship's core still
uses ordinary contact damage. No cover destruction, adds, offscreen phase, or
campaign rewards are implemented. This is a mechanics prototype, not final balance.

## Larger art without larger unfair hitboxes

The ship is 32×32, assembled from eight 8×16 hardware sprites. Six body pieces
surround the two-sprite core actor. The core retains the existing 12×10 damage
hitbox inside its 16×16 footprint; wings are visual and do not collide. All pieces
move together, including camera offsets. Grey slatted armor indicates immunity.
When the laser starts, the hatch opens through a 12-frame intermediate pose into
a gold/orange core. Damage becomes possible as soon as the opening begins; the
core remains gold through cooldown and closes for the next bombing run. The HUD
reinforces these states with ARMORED and CORE OPEN - FIRE! messages.

The laser uses four narrow sprite segments and bombs use two sprites. Ten OAM
entries are reserved for the extra body pieces and hazards, ahead of enemy/player
projectiles, so a full player-shot pool cannot hide a damaging beam. Unused entries
are cleared every frame. Sprite tiles 96–125 replace the trial-only machine
art; gameplay code remains in switchable ROM. Typical battles still need visual
checks for the hardware's ten-sprites-per-scanline limit, particularly when firing
many pellets through the ship.

## Validation

Automated tests exercise real menu selection, eight-piece rendering, armored
immunity, locked bomb markers, explosion damage and dodging, harmless warning vs
live laser damage, exposed windows, pause, half-health timing, victory cleanup,
warning/beam alignment in both health phases, hatch animation and pause behavior,
and completion with normal health, starter-pistol fire, movement and reactive jumps. A crowded-shot test
also checks that all four laser segments remain drawn with sixteen live shots.
On 2026-10-14 all 208 repository tests pass. The reactive-jump completion test
peaked at 55,012 update cycles of a 131,328-cycle budget (about 42%), with the OAM allocation
ending at slot 30 including reserved slots. Screenshots of all attack phases and
victory were reviewed. These are emulator measurements, not hardware certification.
Human playtesting should focus on recognizing
markers, understanding the laser direction, and finding the exposed core.
