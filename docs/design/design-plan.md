# Skavengar: design work and path to a playable game

Created 2026-10-12. This is a planning sketch, not a list of newly approved features.
All scope counts and starter concepts below are proposals. Existing decisions live
in [the decision log](decisions.md); implementation milestones live in
[the roadmap](../../ROADMAP.md).

## The goal

Prove this complete experience on one planet: wake up, learn to survive, trust a
mysterious voice, recover a relic, reach a home base, improve your character, and
choose to descend again. Then expand the content that makes repeated trips rewarding.

We already have a combat foundation. The current weapon definitions include pistol,
shotgun, assault rifle, sniper, and sword, with distinct firing behavior and reload
stats. Treat these as prototypes to build on, not five completed procedural loot
families. The campaign, saved build progression, and collection economy need their
own designs and implementation.

## 1. Decide the rules that everything else depends on

- [ ] What counts as one expedition, and what ends it?
- [ ] How long should a short trip and a full descent take?
- [x] Basic death rule: corporate body reconstruction returns the player to the
      ship with their loadout, charging a percentage of money.
- [ ] Set the fee details, non-loadout retention, mission resets, and voluntary return rules.
- [ ] Which character, equipment, discovery, and mission progress persists?
- [ ] What changes on a repeat visit: rooms, enemies, events, rewards, or all four?
- [ ] What lets you reach deeper: player skill, gear, character level, discoveries,
      or story objectives? Avoid requiring the perfect random drop to advance.
- [ ] How do unlocked landing depths and surface-to-core runs coexist?
- [ ] Is there an expedition-only build layer, such as temporary relic effects?
      Include it only if it creates worthwhile choices alongside persistent builds.

Deliverable: one example successful expedition and one failed expedition, showing
exactly what the player keeps and what they can do next.

## 2. Story and campaign

Use [story.md](story.md) and [opening.md](opening.md) as the foundation.

- [ ] Player: why wake here, what happened beforehand, what is remembered, and why
      continue after reaching safety? Is each class a named character or a custom
      character's profession?
- [ ] Echo: the actual truth about her, what she wants, what she knows at each stage,
      what she conceals, and the limits of her communication and influence.
- [ ] CEO: concrete plan, resources, personal reason for pursuing it, visible harm,
      escalation, and how the player can ultimately stop them.
- [ ] Ancient civilization: what cores do, why they are inside planets, why the
      network shut down, and how relics connect to its restoration.
- [ ] Campaign spine: opening, first core, major reversal, discovery of the central
      threat, final descent, ending, and the state of the galaxy afterward.
- [ ] Per-planet arcs: arrival problem, local characters, discovery, corporate
      interference, boss, permanent consequence, and next lead.
- [ ] Supporting cast: ship contact, local ally, rival, vendor, corporate commander;
      combine roles where one memorable character can serve several purposes.
- [ ] Repeat dialogue: distinguish first discovery, return visit, failed attempt,
      completed mission, and post-boss states.
- [ ] Delivery: short conversations, environmental clues, relic descriptions, and
      optional records, with required reading outside dangerous action.

Deliverable: a spoiler-complete campaign outline and a reveal table: truth, clue,
when the player learns it, and what action that knowledge enables. We should know
Echo's truth before writing clues, even while it stays hidden from the player.

## 3. Guns, melee, and manufacturers

Keep the roles separate: weapon type defines handling; manufacturer defines a
recognizable behavior and tradeoff; a particular model provides its identity;
rolled modifiers create variation; a unique weapon adds a handcrafted rule.

### Weapon families

- [ ] Define the combat role of each existing family: pistol, shotgun, assault
      rifle, sniper, sword. Identify weaknesses as well as strengths.
- [ ] Decide if SMGs, launchers, beam weapons, or heavy weapons add genuinely new
      decisions before adding their art and projectile requirements.
- [ ] Define aim behavior, range, cadence, reload, recoil, mobility, and feedback.
- [ ] Decide critical hits and weak points in a game with auto-aim.
- [ ] Decide reserve ammo, grenade/mine resources, and how the player replenishes them.
- [ ] Define valid combinations, especially melee, charge shots, pellets, pierce,
      ricochet, and status effects. Every manufacturer need not make every type.

### Manufacturers

For each: name, short display name, logo/palette, origin, attitude, relationship to
the CEO's corporation, supported types, one signature mechanic, one drawback, and
model naming style. Corporate branding should tell players something useful about
how a gun behaves.

Starter directions to discuss, not final companies:

| Direction | Signature | Cost | Naming voice |
|---|---|---|---|
| Corporate security | Controlled, dependable bursts | Limited flexibility or long reload | Formal product codes and reassuring euphemisms |
| Frontier salvage workshop | Heavy kick and close-range stopping power | Small magazines or erratic spread | Tools, nicknames, improvised machinery |
| Ancient-tech specialist | Charge shots or a focused energy/status interaction | Windup or low sustained damage | Signals, astronomy, recovered terminology |

### Gun names and loot rules

- [ ] Create a short list of base model names per manufacturer and family.
- [ ] Define prefix meanings, rarity presentation, and unique-name conventions.
- [ ] Provide full item-card names and short HUD labels; the current weapon-name
      interface uses seven characters, so long names need a display plan.
- [ ] Decide whether weapon level, rarity, parts, or some combination determines power.
- [ ] Set bounded roll ranges and invalid combinations; avoid pure junk variants.
- [ ] Decide how favorite guns remain useful: replacement, upgrades, or another rule.
- [ ] Design readable compare cards; raw damage alone cannot compare a shotgun and rifle.
- [ ] Specify drop sources, rare drops, targeted farming, duplicate uses, and unique rewards.

Deliverable: a small catalog of actual items with names, behavior, stats, sources,
and comparison cards. Demonstrate two guns of the same type that invite a real choice.
Do behavior first, then name it: a joke should reinforce a weapon's personality.

## 4. Characters, classes, and skill trees

- [ ] Choose starting roster and character identity. Keep Echo distinct from the
      player's class roster unless the story deliberately changes that.
- [ ] For each class define fantasy, silhouette, movement, starting equipment,
      passive, active ability, resource/cooldown, and vulnerability.
- [ ] Decide whether all classes can use all weapon families. Proposed default:
      preferences and synergies without rigid weapon locks.
- [ ] Establish XP sources, level curve, skill-point timing, level cap, and respec rules.
- [ ] Define which bonuses belong to gear, skills, and permanent ship improvements.
- [ ] Decide skill stacking, damage calculation order, cooldown caps, and maximum
      summons/projectiles so combinations remain readable and performant.

Starter class fantasies, not final names:

| Archetype | Combat identity | Possible active | Build directions |
|---|---|---|---|
| Mobile scout | Reposition, exploit openings, recover quickly | A decoy that draws fire | Mobility/precision or risky close combat |
| Armored breacher | Push through pressure and fight nearby | A directional defensive pulse | Shield retaliation or shotgun/melee aggression |
| Salvage technician | Turn recovered technology into combat tools | Deploy a bounded device | Device support or weapon/status modification |

Start by designing two small skill branches per class, with roughly three meaningful
nodes in each as a discussion scope. Build one class first. Each branch needs an
early change in behavior and a distinctive later payoff, not only percentage gains.

Example node: reloading an empty magazine charges your next shot to knock enemies
back. Follow-up choice: spend that charge on one stronger hit or spread it across
several weaker hits. Exact effects must suit the damage and crowd-control rules.

For every skill record: prerequisite, ranks, cost, trigger, effect, duration,
cooldown, feedback, interaction limits, and why a player would choose it.

Deliverable: one class with two demonstrably different builds and at least one
interesting interaction with each of two weapons.

## 5. Relics, collections, and the economy

- [ ] Separate story relics, equippable artifacts (if any), ordinary salvage, and
      temporary expedition effects. Avoid calling all four the same thing in the UI.
- [ ] Define the first relic: appearance, source, useful effect, Echo's reaction,
      persistence, and the next destination or action it enables.
- [ ] Define collection sets, clues to missing pieces, completion rewards, and duplicates.
- [ ] Decide equipment slots: weapons, shield, and any extra slots that justify their menus.
- [ ] Choose currencies and materials sparingly. Identify how each is earned and spent.
- [ ] Decide shops, selling/scrapping, upgrade costs, storage, and inventory-full behavior.
- [ ] Determine whether crafting is needed at all; collection rewards can grant upgrades
      directly without requiring a separate recipe system.
- [ ] Guarantee essential story progress while leaving optional finds worth hunting.
- [ ] Define chest/cache tiers, opening interaction, reveal presentation, and reward tables.

Deliverable: rewards and spending opportunities for the first three expeditions,
including an unlucky run, a duplicate find, and a full inventory.

## 6. Planets, rooms, missions, enemies, and bosses

- [ ] Give each planet a gameplay identity as well as a visual one: terrain, routes,
      hazards, enemy combinations, and distinctive discoveries.
- [ ] Design depth layers, travel connections, locked barriers, shortcuts, and safe sites.
- [ ] Establish a room vocabulary: entry, combat, traversal, cache, event, rest,
      challenge, guardian, core. Only build types needed for the first expedition.
- [ ] Separate fixed story landmarks from randomized branches; define generation
      guarantees so objectives and return routes are always reachable.
- [ ] Define repeatable objectives such as recover, survey, restore, rescue, or
      intercept, and the events required to track success or failure.
- [ ] Create enemy roles with readable tells: chaser, ranged attacker, defender,
      support, elite. Combine a few roles before increasing counts.
- [ ] Define damage, shields, armor, elements, status rules, and visual feedback.
- [ ] For each boss specify arena, readable attacks, phases, counterplay, reward,
      narrative consequence, and repeat-fight explanation.
- [ ] Decide how revisiting completed cores works in the fiction and the loot system.
- [ ] Plan difficulty across depths and planets, avoiding automatic scaling that
      cancels out the feeling of getting stronger.

Deliverable: a playable first-site layout and encounter list, including an optional
branch and a guardian that tests the mechanics learned in the opening.

## 7. Interfaces, persistence, and production work

- [ ] Title/character selection, HUD, pause/map, inventory/compare, skill selection,
      collection journal, mission tracking, travel, and return/death screens.
- [ ] Resolve pickup/interact input against firing, jumping, reloading, and weapon
      switching. Follow current controls in the roadmap rather than old reference notes.
- [ ] Check text length, icon readability, palette use, and controller navigation on
      the actual Game Boy Color display. Decide which menus pause the world.
- [ ] Define stable IDs for characters, items, skills, discoveries, objectives,
      and destinations; separate permanent save data from temporary expedition state.
- [ ] Decide save slots, checkpoints, suspend/resume, save timing, versioning,
      checksums, and recovery from invalid data before users invest in a character.
- [ ] Build content tables and authoring/conversion tools; keep content separate
      from behavior so adding a gun or room does not require duplicating systems.
- [ ] Budget sprites, projectile pools, effects, VRAM, RAM, ROM banks, and CPU for
      combinations of gear and skills, not just isolated features.
- [ ] Plan portraits, character/enemy animations, room tiles, weapon art, relic icons,
      UI, sound effects, music, and credits/licensing for shipped assets.
- [ ] Carry settings such as screen shake and control preferences into player menus.
- [ ] Verify deterministic generation, loot, mission state, saves, and performance
      with the existing harness; playtest clarity and enjoyment directly.

Deliverable: a saved campaign can be closed and resumed with the same equipment,
build, discoveries, and completed objectives, with legible UI and acceptable timing.

## 8. Build the first complete slice

This is a proposed dependency order across M2–M5, not a replacement for the roadmap.
Bring the minimum save and progression systems forward rather than waiting until
all weapons and planets exist.

| Step | Build | Pass condition |
|---|---|---|
| 1. Lock essential rules | Success/death outcomes, persistence, first relic, one class | A successful and failed expedition can be described without contradictory rules |
| 2. Playable opening | Authored rooms, contextual prompts, Echo dialogue, first relic | A new player reaches the relic and understands movement, combat, and collecting |
| 3. Complete the loop | Minimal hub, item handling, one improvement, next expedition, save/load | Return with a find, make a useful choice, save, reload, and head out again |
| 4. Make repeats interesting | Seeded branches, reward sources, a few variants, guardian | A second and third trip offer purposeful choices and worthwhile rewards |
| 5. Prove build variety | One skill tree and meaningful manufacturer differences | Two builds change how players approach the same encounters |
| 6. Expand | Another class and planet, then broader campaign content | New content adds decisions while reusing proven systems |

Proposed initial slice budget: one planet/site, approximately 6–8 authored room
templates, one class, two weapon families using existing code, two manufacturer
identities, two ordinary enemy roles plus one guardian, one story relic, one small
collection, and one hub contact/function. These are targets to discuss, not mandates.

The opening can use a fixed route through that room set; the first replayable
expedition can reuse parts with different branches and encounters. Include a
return/death path and save/load in the slice, not only a polished opening fight.

## 9. What to defer

Full manufacturer catalogs, many currencies, large skill trees, numerous planets,
complete crafting, sprawling hub crews, multiple endings, and extensive endgame
modes can wait until the first complete loop is enjoyable. Record promising ideas
without treating every idea as required release scope.

## Suggested next design sessions

1. **The first hour:** wake-up reason, first relic, Echo's help, first return,
   failure rules, and what motivates the second trip.
2. **Player builds:** select class fantasies and fully sketch one class's choices.
3. **The armory:** type roles, manufacturer identities/names, a small named gun
   catalog, rarity, and loot comparisons.
4. **First planet:** rooms, depth, enemies, guardian, collection, and story reveal.
5. **Campaign truth:** settle Echo, the builders, the CEO's plan, and the ending,
   then distribute clues through the planets.

Keep expanding the relevant topic documents as decisions are made. A checkbox
means a design question has a reviewable answer, not that the feature is implemented.
