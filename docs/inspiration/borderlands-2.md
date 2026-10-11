# Borderlands 2: design reference

These notes summarize Borderlands 2 as design inspiration for RogueGB.

The linked FAQ is the
[Borderlands 2 Guide and Walkthrough by noz3r0 (Greg Boccia)](https://gamefaqs.gamespot.com/pc/638786-borderlands-2/faqs/67326).
It's copyrighted and may not be reproduced, so nothing here is copied from it.

**Most of it is a mission-by-mission walkthrough** covering the main story, about 100
side missions, two DLC campaigns, and a zone travel guide. It says little about the
loot and gun systems, which are what we care about most. So this file has two
parts:

- **Part A** covers what the FAQ itself documents: elements, boss fights and enemy roles, in our own words.
- **Part B** covers Borderlands' core systems from general knowledge of the series. It's **not from this FAQ**, so check details against a wiki if exact numbers matter.

---

## Part A: From the FAQ

### A1. Elements

The game's side missions introduce one element per mission. In short:

| Element | Role |
|---|---|
| Fire | Strong against flesh (health) |
| Shock | Strong against shields |
| Corrosive | Strong against armor and robots |
| Slag | Debuff: a slagged target takes extra damage from all non-slag sources |
| Explosive | General damage (rockets, grenades) |

The fights reward **switching weapons to match the defense layer**: shock through a
shield, then corrosive or fire on what's underneath. Boss stat blocks list a
weakness and a resistance for each boss.

**RogueGB takeaways**
- Keep **3 elements + 1 debuff**: Fire / Shock / Acid plus a "mark" debuff (Slag). Each enemy has one defense layer: flesh, shield or armor.
- On DMG (4 grey shades), elements can't rely on color. Use **bullet shape** and an **impact effect** (flame flicker, spark, drip) instead. On GBC, use palettes as well.
- Holding 2–4 guns and swapping on SELECT (or a quick tap) keeps "pick the right element" a live decision.

### A2. Boss design patterns

Every boss in the FAQ gets a stat block with relative health, shield, damage, a
critical spot, a weakness and a resistance. Across the bosses, these mechanics
repeat (paraphrased):

| Pattern | Example in the FAQ | What it teaches |
|---|---|---|
| **Adds on a health threshold** | The first boss leaps away and summons small enemies after taking damage | Phases split by HP; the player manages the room |
| **Duo boss** | A jetpack flyer plus a heavy gunner; disarming the gunner weakens him | Target priority; break a part to change behavior |
| **Boss behind minions, then drops in** | A pirate captain shoots from above while minions rush you, then comes down and uses a flame-ring move | Snipe early, then dodge the phase that marks itself |
| **Shield support enemies** | Flying drones recharge the boss's shield; killing them is optional but helps | Kill the healer first, or don't |
| **Element-cycling boss** | A beast that cycles slag → fire → shock → corrosive, changing which weakness works | Read its current state and swap weapons |
| **Air phase** | A flying fortress lifts off, rains mortars, and comes back with a laser and a big charged beam | Telegraph big attacks; give the player cover |
| **Timer fight** | A guard robot must die within a few minutes or the mission fails | Pressure through a clock |
| **Clone fight** | The villain spawns decoys; find and burn the real one | A recognition puzzle mid-combat |
| **Arena hazard** | Rising lava pushes you to high ground; the boss's attacks also kill its own adds | The environment changes during the fight |
| **Weak point** | Head, eye, mouth or joints take crit damage | Aim matters, even in 2D |

Challenge goals, such as "beat it without destroying the drones" or "avoid the
flame move", add optional mastery goals to each fight.

**RogueGB takeaways**
- **Each zone gets a boss that shows one pattern clearly.** Mix two patterns for the final boss.
- Weak points in 2D: a crit hitbox on the boss's metasprite (eye or head). On GB, crit feedback is a sprite flash plus a different hit sound.
- Optional "challenge goals" per boss are good meta-unlock triggers, like achievements that unlock gun parts.

### A3. Enemy roles

The enemies that come up most in the FAQ, sorted by role. The FAQ names them in
its walkthrough; some behavior notes (nomad shields, the goliath helmet
mechanic) come from general series knowledge rather than the FAQ:

| Role | Examples | Behavior |
|---|---|---|
| Rusher | Psychos (melee bandits), skags, bullymongs | Closes distance fast and punishes standing still |
| Gunner | Marauders, nomads (with shields), loaders | Mid-range fire, often behind a shield |
| Flyer | Rakk, buzzards | Swoops past and attacks from above |
| Support | Surveyors (repair or shield others), engineers | Raises target priority |
| Heavy | Goliaths (enrage and level up when their helmet is shot off), heavy robots | Tanky, with a state change |
| Badass / elite | Badass variants of every type | Higher HP, special attacks, better loot |

The FAQ also uses **Second Wind**: when you're downed, you get a short window to kill
an enemy and get back up. It repeatedly suggests leaving a weak enemy alive during
boss fights for that purpose.

**RogueGB takeaways**
- Give each zone 3–4 enemy families covering these roles, then add tiers (see the Rogue Legacy notes).
- The **"Badass" prefix** is an easy elite variant: ×2 HP, one extra attack, a guaranteed better drop, and a visual tag.
- **Second Wind** fits a roguelite well: at 0 HP, enter a 5-second "last stand" with a smaller hitbox. Get a kill to revive, otherwise the heir dies. It's a cheap system with big drama.

---

## Part B: Core Borderlands systems (general knowledge, not from this FAQ)

These are the series systems the project description points at ("run and gun like Borderlands").

### B1. Procedural guns
- A gun is put together from **parts** (body, barrel, grip, stock, sight, accessory, element). Each part changes stats.
- **Manufacturers** give each gun family a recognizable gimmick. Examples: guns that explode instead of reloading, burst on aim, high-damage bullets, shields fired from the gun, accuracy that improves while firing, or guns that only shoot elements.
- **Rarity colors** run white → green → blue → purple → orange (legendary), with special tiers above those.
- Core stats: damage, fire rate, accuracy, reload speed, magazine size, element chance and damage, plus a short **flavor text line** on special items.

### B2. Character kit
- Each character has one **action skill** on a cooldown (turret, phaselock, dual-wield, invisibility) and **three skill trees**.
- Gear slots: shield (capacity, recharge delay and rate, with special effects), grenade mod, class mod (+skills), and relic (passive).

### B3. Loop and feel
- Kill → loot fountain → compare → equip. The fun is in the numbers going up and in odd items.
- Health has layers (shield → health, or shield → armor for some enemies), which ties back to elements.
- Humor and voice: loud, irreverent, with item names as jokes.

**RogueGB takeaways (what fits on a Game Boy)**
- **Guns as packed bytes.** A gun can be 4–6 bytes: manufacturer (3 bits), rarity (3 bits), element (2 bits), plus base stats from part indices. Generate a gun from a 16-bit seed, so it can be shown, saved and rebuilt cheaply.
- **5–6 manufacturers**, each with **one gimmick** that's readable in 2D, for example:
  - spread shot
  - burst fire
  - piercing slugs
  - ricochet
  - explode-on-reload
  - charge shot
- **Rarity** on DMG: a frame or icon around the gun name and a pickup sparkle. On GBC: colors.
- **Loot drops** should be few but meaningful. Sprite budget limits screen clutter, so drop a single "gun crate" pickup and show the full stat card on pause.
- **Stat compare screen**: up/down arrows for each stat versus the equipped gun. This is core to the Borderlands feel.
- **Action skill** on B with a cooldown bar; class trees are where this meets Rogue Legacy's classes.
- **Short item flavor text** (12–16 characters) gives the Borderlands voice cheaply.
