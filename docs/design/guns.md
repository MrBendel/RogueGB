# The gun engine

Status: direction agreed 2026-10-16 (five manufacturers); details are proposals.
Builds on loot ([progression.md](progression.md)): guns are items with a type,
level and rarity. Inspired by how Borderlands assembles guns from parts with a
manufacturer's personality; the makers, names and rules here are our own.

## A gun

> **Level** · **Rarity** · **Manufacturer** · **Type** · **Parts** · **Element** · *(Legendary trait)*

The name is built from them: "Kessler Hot Drum Shotgun". The card shows what each
piece does, so two shotguns can be a real choice.

## Manufacturers (decided: five)

Each has one rule that changes how its guns play.

| Maker | Rule |
|---|---|
| **Kessler Arms** | Reloading throws the gun: it explodes where it lands (more damage with more rounds left in it), and a fresh one is in your hands. |
| **Gemini Works** | Every shot fires two bullets, slightly spread, for one round. |
| **Bulk & Sons** | Huge magazines (about double), worse accuracy, slower reloads. |
| **Sparkworks** | Always elemental, and the element applies on every hit; lower base damage. |
| **Meridian Industries** | The best raw stats, but each reload charges 1-2 credits: a licensed firearm. (The story's company: satire with teeth.) |

Makers lean toward types: Kessler toward pistols, Bulk toward rifles and
shotguns; Meridian is rarer at low levels.

## Elements (proposal: three)

| Element | On a hit | Strong against | Look |
|---|---|---|---|
| **Fire** | Burns: 1 damage every half second for 3 s | flesh (slimes, creatures) | orange shots; the enemy flickers red |
| **Electro** | Arcs to one nearby enemy for half damage; x2 on shields | shields, groups | blue-white shots; a spark |
| **Acid** | Weakens: +50% damage taken for 3 s | armor, tough enemies, bosses | green shots; the enemy tinted green |

A hit applies the element by chance (20% common to 50% legendary; always on
Sparkworks guns). Matchups need enemy variety: deeper camps bring shielded and
armored enemies. Open: a fourth (cryo: slows).

## Parts

Three slots of four options, nudging stats in readable ways:

| Part | Options |
|---|---|
| Barrel | Long (+range, +accuracy), Short (+damage, -range), Split (+1 pellet), Smooth (+shot speed) |
| Magazine | Standard, Drum (+size), Quick (faster reload), Extended (+size, slower reload) |
| Accessory | None, Bayonet (melee bonus), Scope (each hit in a row adds crit chance; a miss resets it), Charger (+element chance) |

## Legendary traits

An orange gun gets one unique trait and a line of flavor on its card. Ideas:

- **Ricochet:** shots bounce off a wall once.
- **Homecoming:** a thrown Kessler gun flies back before it explodes.
- **Ouroboros:** kills refill the magazine.
- **Second Opinion:** every third shot is a free double.
- **Quarterly Bonus** (Meridian): reloads pay you instead.
- **Static Bloom** (Sparkworks): electro arcs to everything on screen.
- **Junk Heart:** shots can jam, or overcharge for x3 damage.

## On the Game Boy

- **Size:** about 10 bytes a gun: type, rarity, level, maker, element and parts
  packed in bits, the stat rolls and a trait id alongside.
- **Element colors:** the shots' sprite palette is recolored on a weapon switch
  (the existing palette upload): nothing per frame.
- **Maker rules** are branches on switch, reload and shot spawn, in banked code;
  the bullet loops (assembly) stay as they are. Twin shots double the bullets in
  flight: the CPU budget tests cover a Gemini gun.
- **Status effects:** a timer byte per enemy, ticked at 30 Hz like their thinking.
- **Generation** (`src/gunsmith.c`): type, then maker (weighted by type), parts,
  element (likelier on rarer and Sparkworks guns), rolls, name. Weights in a
  content file, tunable without code.

## Order

1. Makers and parts on the card; the cheap rules (Bulk, Gemini).
2. Elements: enemy status timers, shot colors, burn / arc / weaken.
3. Kessler's thrown gun (an arc and an explosion: an area-damage helper).
4. Meridian's reload fee and the legendary traits.
5. Enemy shields and armor, so the matchups matter.
