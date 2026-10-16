# Enemies

Acid Skimmer is the sole enemy playtest and the reference for future enemy quality.
From the title, press DOWN, select Enemies, then Acid Skimmer. B goes back;
START pauses and SELECT while paused ends the attempt.

The Slime, Peashooter, Mixed Group, Spore Husk, Ragebound, Contractor, Bastion
Drone, Faction Mix, and Scrap Hound trials have been removed, along with the
unused family controller and artwork. Their earlier prototypes remain in Git
history. Campaign/lab enemies and boss-specific reinforcements remain intact.

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
The remaining 212 repository tests pass. The normal Acid Skimmer completion test peaks at
49,400 of the 131,328 available update cycles (about 38%).
