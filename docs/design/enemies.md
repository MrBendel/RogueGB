# Enemies

Acid Skimmer, Mercenary, Acid Wolf, Rift Stalker, and Wireweaver are the active enemy playtests. Acid Skimmer remains
the reference for future enemy quality. From the title, press DOWN, select
Enemies, then an encounter. B goes back;
START pauses and SELECT while paused ends the attempt.

The Slime, Peashooter, Mixed Group, Spore Husk, Ragebound, Contractor, Bastion
Drone, Faction Mix, and Scrap Hound trials have been removed, along with the
unused family controller and artwork. Their earlier prototypes remain in Git
history. Campaign/lab enemies and boss-specific reinforcements remain intact.
The new Mercenary is a fresh controller using the supplied NPC artwork, not a
restoration of the removed Contractor prototype.

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
At the prior cleanup, 212 repository tests passed. The normal Acid Skimmer completion test peaked at
49,400 of the 131,328 available update cycles (about 38%).

## Mercenary rifle duelist

Select **Enemies → Mercenary**. One 28-HP mercenary uses the supplied cyan NPC
sheet's four-frame directional walk and roll animations, with a separate rifle
that rotates through sixteen directions. It is a fresh prototype, rather than
the removed Contractor design.

- Strafe at combat distance, advance when far away, and retreat when crowded.
  Running covers one pixel per frame; reload movement runs at half that rate.
- An 18-frame firing warning leads into six rounds, seven frames apart. Each
  group of three commits to a muzzle-to-player aim vector; the second group
  reacquires the target. Rifle hits deal 16 damage through the ordinary shield
  and invulnerability rules. Accuracy is continuous, despite the gun's sixteen
  visual angles.
- Reload for 30 frames, then reposition for 16 before the next warning. The
  player can shoot throughout; there is no armor or arbitrary immunity phase.
- Nearby approaching player shots can trigger an eight-frame dodge warning,
  a committed 14-frame lateral roll at three pixels per frame, and an 18-frame
  stationary recovery. A 140-frame cooldown prevents endless dodges. Wall
  collision checks each roll substep.
- The rifle recoils and flashes when firing, is tucked away during rolls, and
  returns afterward. A shadow stays under the body and a filling meter marks
  reloads. Defeat clears its bullets.

`tools/import-mercenary-art.mjs` converts the original 2x sheet into native 16x16
frames. Body tiles use VRAM bank 0 at 96–191, shadow at 192–193, and reload meter
at 194–201; rifle art reuses
the player's loaded rifle in VRAM bank 1 without changing the player's weapon.
OAM 15–22 is reserved for the gun and effects. AI is in `src/mercenary.c`;
scripted actor value 9 identifies contact damage, and death cause 12 identifies
the mercenary's shots. Encounter projectile settings reset when actors load.

Validation covers six-shot bursts, reload visibility, reactive rolls and recovery,
wall boundaries, pause, normal-health starter-pistol victory with movement, defeat
when standing still holding fire, sustained combat, and switching back to Acid
Skimmer. The merged suite also checks that the third boss entry remains available
and that all global RAM sections end before the fixed room map at `0xD000`.
Mercenary completion peaks at 63,580/131,328 update cycles; the sustained stress
test peaks at 62,624. These are headless-emulator measurements.

## Acid Wolf trial

Select **Enemies → Acid Wolf**. This is a fresh movement prototype, using a
16×16 quadruped silhouette with running, crouching, leaping, and spitting poses.
One 30-HP wolf closes to roughly 25–48 pixels, circles, and backs off if crowded.

- Approaching player projectiles can trigger a four-frame crouch followed by a
  committed sideways leap: 12 frames at three pixels per frame. It alternates
  sides, reversing when a landing would be blocked. Every movement substep checks
  the floor; it cannot jump through walls. Its body rises eight pixels above a
  ground shadow. Collision remains on the ground; there is no invulnerability.
- A leap has an 84-frame cooldown and ends in a 14-frame stationary landing.
  Bait the dodge, then punish the landing. It reacts to visible projectiles,
  never to the player's fire button, and cannot dodge while attacking/recovering.
- An open-mouth, flashing 18-frame warning locks a muzzle-to-player aim vector.
  Three green acid globs fan out at roughly two pixels per frame, dealing 18 damage through ordinary
  shields. Sidestep after the warning or jump the volley. The wolf holds still
  for 22 frames after spitting; there are no persistent acid pools in this trial.
- Pause freezes the entire controller. Defeat clears its projectiles and shadow.

`src/acid_wolf.c` owns the trial (scripted actor 10, death cause 13). Body art uses
tiles 96–111, shadow 112–113; the glob replaces the scene's enemy bullet art.
The shadow uses OAM 11. Actors and projectiles retain all 25 dynamic slots from
OAM 15 onward. The temporary body elevation assumes this single-actor trial;
campaign packs will need shared actor elevation before integrating these wolves.

Emulator tests exercise trajectories, committed leaps, landing windows, warnings,
pause, walls, normal-health pistol completion, damage, cleanup, and frame limits.
Standing still while holding fire loses across four starting frame phases; moving
and jumping wins. Peak measured update cost is 55,696 of 131,328 cycles (43%).


## Rift Stalker trial (2026-10-24)

An alien phase ambusher using the local SGQ_Enemies 16x16 ghost sheet.
Select **Enemies → Rift Stalker**. Cyan highlights, a violet body, and dark
purple outlines distinguish it from the acid enemies.

- One 18-HP stalker approaches alternating player flanks as a moving cyan
  shimmer. It cannot hurt the player during stalking, warning, or recovery.
- It becomes solid for a 26-frame warning and locks its attack vector before
  that warning. Moving sideways can bait the subsequent diagonal dash.
- A 16-frame dash can deal 16 contact damage; jumping avoids it. Walls stop
  the dash and immediately begin recovery.
- Recovery lasts 48 frames and is the damage window. Other phases are guarded.
- Pause freezes its state; defeating it clears the standalone trial.

The controller uses scripted actor 15, ordinary two-part sprite rendering from
OAM 15, and only five bytes of additional persistent state. Art conversion is
reproducible with tools/stalker-art.mjs; source sheets remain in the local art
library. This first playtest does not add campaign spawns or teleportation.


## Wireweaver trial (2026-10-25)

A 14-HP spider from the SGQ_Enemies 16x16 spider sheet, with amber highlights
and a magenta body. Select **Enemies → Wireweaver**.

- Scuttles sideways while trying to keep some distance from the player.
- Marks the player's position for 26 frames, then fires a visible web toward
  that fixed point over 16 frames. Moving after the mark avoids the patch.
- One web patch persists for 210 frames; a newer web replaces the old one.
  Grounded steering is half speed inside it, with no damage or lingering slow.
  Jumping and air dashes escape normally; heavy-weapon slow does not stack.
- At close range, a separate 20-frame warning precedes a committed 12-frame
  lunge for 12 damage. Jumping avoids it. It pauses 30 frames after either attack.
- Always vulnerable to gunfire; passive body contact does no damage. Defeat
  clears its web and slow immediately; pausing freezes the encounter.

Body sprites use OAM 15 onward. Warning/projectile and web use props 11–14;
optional gun overlays yield when necessary to preserve the player and spider.
Controller: src/wireweaver.c (scripted actor 16). Art: tools/weaver-art.mjs.
This is a standalone trial; mixed enemy encounters remain future work.
