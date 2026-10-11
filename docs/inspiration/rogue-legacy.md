# Rogue Legacy: design reference

These notes summarize Rogue Legacy's systems as design inspiration for RogueGB.
They are written in our own words from the
[Rogue Legacy Complete Guide by Maxscart](https://gamefaqs.gamespot.com/pc/711433-rogue-legacy/faqs/68252)
(v1.0.9, for game version 1.1.0). The guide doesn't allow full reproduction, so
for exact wording, the journal text and the walkthrough, read the original.
Every section ends with **RogueGB takeaways**.

---

## 1. The core loop

1. Choose an heir from **three randomly rolled children**. Each child has a class and 0–2 traits.
2. Enter the castle. Its **layout is randomized each run, but built from hand-made rooms**, so players learn individual rooms over time.
3. Collect gold, chests and blueprints, and fight until you die.
4. After death, spend the gold in the Manor (permanent upgrades), at the Blacksmith (gear) and at the Enchantress (runes).
5. On re-entry, the gatekeeper (Charon) **takes all unspent gold**. This "use it or lose it" rule keeps players from hoarding and pushes them to spend every run.

Death is the progress mechanic. The character is lost but the family's upgrades stay.

**RogueGB takeaways**
- The "pick 1 of 3 heirs" screen is cheap to build on Game Boy and gives each run a different start before it begins.
- Stitching handmade rooms together fits GB memory well: store rooms as compressed tile maps in ROM banks and keep only the run's room list in WRAM.
- An "unspent currency is taxed" rule is a simple way to keep the meta-loop moving. A partial tax could be a skill upgrade (Rogue Legacy has "Haggle").

---

## 2. Areas and bosses

There are four zones, from easiest to hardest: Castle → Forest → Tower → Dungeon (the
dungeon is dark, so you only see near yourself). A final area opens once all four
zone bosses are dead. Each zone has a boss room with a teleporter and healing
statues. Killing a boss crosses off a seal on the final door. Players can skip a
zone's boss and move on, and bosses can be retried until beaten.

**RogueGB takeaways**
- Four biomes plus a final area is a realistic scope: one biome per ROM bank or so, with different tilesets and palettes.
- "Darkness" in the hardest zone is a cheap but striking effect on GB: use window or sprite masking, or a palette trick that only lights near the player.

---

## 3. Classes as stat multipliers

Each class is a set of **percent multipliers** on base stats plus **one active ability** (it costs MP) and sometimes a passive. Upgrading a class in the Manor improves it.

| Class | HP | MP | STR | INT | Ability | Passive / quirk |
|---|---|---|---|---|---|---|
| Knight | 100% | 100% | 100% | 100% | Shield block (25 MP) | Balanced baseline |
| Mage | 50% | 150% | 50% | 125% | Swap between 3 spells | Regains MP on kill |
| Barbarian | 150% | 50% | 75% | 100% | Shout clears projectiles (20 MP) | Tank |
| Knave | 75% | 65% | 75% | 100% | Mist: invulnerable, can't attack (MP per second) | +15% crit chance, +125% crit damage |
| Shinobi | 60% | 40% | 175% | 100% | Short dash through enemies (5 MP) | +30% move speed, can't crit |
| Miner | 50% | 50% | 75% | 100% | Headlamp lights dark rooms | +30% gold, reveals chests on map |
| Lich | starts 35% | starts 50% | 75% | 150% | Convert half of max HP into MP | +4 max HP per kill (capped) |
| Spellthief | 75% | 40% | 75% | 100% | Double-size, double-cost spell | Regains MP equal to 30% of damage dealt |
| Dragon | 40% | 25% | 0% | 100% | Free flight, fireball only | No jump, no melee |

Design patterns:
- **Every class trades something away.** The strongest numbers come with a hard drawback (the Shinobi can't crit, the Dragon can't melee).
- **Resource loops differ per class**: MP back on kill, on hit, or over time. The same spell plays differently depending on how MP comes back.
- The guide's view is that the "fun" classes (Miner, Dragon) are weak late game. Classes need a late-game reason to exist.

**RogueGB takeaways**
- Classes become **gun-handling archetypes**: for example a Gunner (balanced), a Grenadier (big HP, slow fire rate, shout clears bullets), a Sniper (crit-focused), a Runner (dash, no crits), and a Scavenger (more loot, weak).
- Store stats as 8-bit multipliers in fixed point (for example 128 = 100%) so the math stays cheap on the SM83 CPU.
- Give each class one ability on the **B button** and keep A for shooting, so the controls stay at two buttons.

---

## 4. Traits (random heir modifiers)

There are 31 traits and each heir has 0–2. They fall into clear groups:

| Group | Examples (effect in brief) |
|---|---|
| Stat or mechanic changes | Move 30% faster; no knockback taken; much more knockback dealt; extra knockback taken; regain MP from smashing objects; immune to spike traps |
| Size | Tiny (fits secret passages, shorter reach); giant (longer reach) |
| Information changes | HP bar hidden; damage numbers shown ×100; no full map, minimap only; enemies shown as dots on the map; projectile warnings hidden |
| Screen and presentation | Black and white; sepia; blur near or far; screen flipped upside down; harmless fake enemies |
| Pure jokes | No hair; sound effects; text scrambled |
| Random rule changes | Spell changes after each cast; projectiles come from the opposite side |

Design patterns:
- Traits are **cheap to build and very memorable**. Many are only visual or comedic.
- A good share are **double-edged**: tiny gives secret access but shorter reach.
- Traits that hide information raise difficulty without any new content.

**RogueGB takeaways**
- GB-friendly traits that cost almost nothing to build:
  - palette swaps: inverted (BGP register), "night vision" or washed-out palettes
  - flipped screen: flip the sprites and the map draw
  - hidden HP
  - big or small hitbox
  - bullets bounce once
  - recoil (each shot knocks you back)
  - "trigger happy": auto-fire but less accurate
- Traits are a natural slot for jokes that fit a Borderlands-style tone.

---

## 5. Spells → secondary weapons

There are 11 spells, each a projectile pattern: a stationary blade wall, orbiting
fireballs, a returning chakram, an arcing axe, a straight dagger with infinite
range, bouncing orbs, time stop, a teleport marker, crows that hit every enemy
on screen, twin diagonal scythes, and a short-range fire spray.

**RogueGB takeaways**
- These map directly to **grenade or special-weapon patterns** on a sprite budget: arcing, boomerang, orbit, bounce, screen-clear.
- Each pattern is a few bytes of motion data (velocity, gravity, lifetime, bounce flag), which makes it a good fit for a data-driven projectile table.

---

## 6. Chests, runes and loot

**Chest tiers**

| Chest | Contents |
|---|---|
| Wooden | Mostly coins, rare blueprint |
| Iron | More coins and gems, better blueprint odds |
| Royal | From minibosses and challenge rooms: high blueprint odds |
| Fairy | Always a rune, but locked behind a **challenge** (no jumping, no looking, find the hidden exit, and so on). After all runes are found they give stat boosts |
| Giant | After a boss: large stat boosts |

**Runes** are found once, then bought and equipped at a vendor. They **stack**, so
two Vault runes mean three jumps. Examples: air dash, extra jump, +10% coin value,
reflect 50% of damage, brief flight, +20% move speed, easier enemies (less loot),
harder enemies (more loot), HP on kill, MP on kill, and HP+MP on kill.

**Equipment**: five slots (sword, helmet, cape, bracers, chest). Each item has a
**Weight**, and total weight is capped by a Manor upgrade. Items trade stats
against each other: one sword gives +9 damage but −30 HP plus lifesteal, another
gives −10 damage but +10% gold. Item tiers unlock as you find blueprints in chests.

**RogueGB takeaways**
- This is where Borderlands-style gun loot plugs in. Rogue Legacy's gear is **found once as a blueprint, then bought permanently**. Borderlands guns are **found and kept per run**. A hybrid could work:
  - Guns drop per run, are random, and are lost on death.
  - Gun **parts or manufacturers** unlock permanently, like blueprints, so they can appear in future drops.
- Stacking runes on a weight budget is a good **meta-build** system. A "loadout weight" cap keeps it simple.
- Challenge chests (fairy chests) are good level-design content: small rule-change rooms built from existing pieces.

---

## 7. Special rooms (the room catalog)

Besides normal combat rooms, the castle mixes in:

| Room | Function |
|---|---|
| Boss entrance | Teleporter, HP and MP refills, 2 chests |
| Sector link | Doorway to the next zone, plus refills |
| Miniboss | Royal chest on kill |
| Journal | Lore entry (story told in fragments) |
| Spell stand | Swap your current spell (one-way choice) |
| Fountain | One-time 20–30% HP/MP refill |
| Random teleporter | Sends you to a random room, a risky escape |
| Jukebox | Choose the music |
| Gamble chest | Pay 25% of your gold, pick 1 of 3 chests; one returns 3× what you paid |
| Mini-game | Target-shooting challenge for a royal chest |
| Shrine | Random run-long blessing; one of seven outcomes is a curse (lose coins when hit) |
| Secret compass room | A compass points to a hidden room with 2 royal chests |

**RogueGB takeaways**
- Plan the room-type list early and give each a byte-sized ID in the level generator.
- A **shooting gallery mini-game** suits a run-and-gun directly.
- "Gamble 25% of your gold" and the "shrine with a chance of a curse" are cheap risk/reward moments.
- Pick-one-of-several-weapons stands work like the spell stand: a permanent choice for the run.

---

## 8. Meta-progression (the Manor skill tree)

The Manor is a tree. The first node (Blacksmith) unlocks the rest. Categories:

- **Vendors**: Blacksmith (gear), Enchantress (runes), Architect (locks the castle layout for the next run at 60% gold)
- **Class unlocks and upgrades**: unlock Shinobi, Miner, Spellthief, Lich and Dragon, and upgrade each class's ability
- **Stats**: HP, MP, attack, armor, crit chance (max about 50%), crit damage, magic damage, equip weight, invulnerability time after a hit, potion strength, down-strike damage, mana cost
- **Economy**: +10% gold per level, Haggle (keep some unspent gold), reroll your heir choices once per generation
- **Safety net**: a small chance to survive a killing blow

**Pricing**: each upgrade's cost rises with its level, *and every purchase raises all other costs by 10 gold.* This flattens overall progress and makes order matter.

The guide's priority advice: buy gold gain and class upgrades first, then attack, HP, crit and i-frames, and leave armor, magic and Haggle for last. It also notes that buying a weak class unlock (Dragon) **pollutes the heir pool**.

**RogueGB takeaways**
- A ~20-node tree fits in a menu and in SRAM: one byte per node level.
- The "every purchase raises all costs" rule is one global counter, so it costs almost nothing to implement.
- The "lock the layout" vendor (the Architect) is handy for testing too, since a fixed seed gives repeatable runs. We already need seeded generation for replays.
- Avoid unlocks that hurt the player. Let players turn classes off in the heir pool.

---

## 9. Difficulty and New Game+

- Enemies come in **families with 3 tiers**. For example, a bone-throwing skeleton goes from single bone → double bone + jump → bone stream + jump. A miniboss variant sits on top.
- Families are defined by **one readable gimmick**: arcing throw, melee lunge, aimed shot, triple fireball, aimed ice shot, ground pillar with a warning marker, and ambush from the ground.
- The base game is mostly tier 1, with tier 2 near the end. NG+ raises every enemy one tier (max tier 3) and adds +50% coin value per cycle.
- Runes tune difficulty: one lowers enemy tier for less loot, the other raises it for more loot. Players pick their own difficulty.
- Difficulty comes from learning enemy **tells**, room patterns, and slow early gold.

**RogueGB takeaways**
- Build each enemy family once and get 3 tiers from the data (shot count, speed, HP, whether it jumps). Tiers can also be **palette variants**, which on Game Boy Color is free.
- Telegraphs matter more on a small, blurry screen. Use a 1–2 frame flash or a warning tile before every attack.
- Bullet-hell density has to respect the **10 sprites per scanline** limit. Design enemy bullet patterns to avoid horizontal bunching, or use flicker on purpose.
- NG+ as "tier +1, loot +50%" is an easy late-game extension.
