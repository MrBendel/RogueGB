# Inspiration references

Design notes on the two games RogueGB borrows from. They are our own summaries,
written for design decisions. They aren't copies of the source FAQs, whose authors
don't allow reproduction, so follow the links for the full guides.

| File | Game | Source | What it covers |
|---|---|---|---|
| [rogue-legacy.md](rogue-legacy.md) | Rogue Legacy (Cellar Door Games, 2013) | [Complete Guide by Maxscart](https://gamefaqs.gamespot.com/pc/711433-rogue-legacy/faqs/68252) | Heir/lineage loop, classes, traits, spells, chests and runes, gear weight, room catalog, Manor skill tree and pricing, enemy tiers, NG+ |
| [borderlands-2.md](borderlands-2.md) | Borderlands 2 (Gearbox, 2012) | [Guide and Walkthrough by noz3r0](https://gamefaqs.gamespot.com/pc/638786-borderlands-2/faqs/67326) | Elements vs defense layers, boss patterns from the FAQ's boss stat blocks, enemy roles, Second Wind; plus core gun/loot systems from general knowledge, marked as such |

Every section ends with **RogueGB takeaways**, which turn the idea into something
that fits the Game Boy. The main limits behind those takeaways:

- 40 sprites, with at most 10 per scanline
- 4 shades on the original Game Boy
- 8 KB of work RAM
- a slow 8-bit CPU

## Combined picture (first pass, open for discussion)

- **Run structure (Rogue Legacy):** pick 1 of 3 heirs (class + traits). Clear stitched-together rooms in 4 zones with bosses. Gold buys permanent Manor upgrades between runs, and unspent gold is lost.
- **Moment-to-moment play (Borderlands):** run and shoot. Hold 2–4 procedurally generated guns from a few manufacturers, each with a gimmick. Elements beat matching defense layers (shield, armor, flesh). Badass elites, loot from crates, and a stat compare screen.
- **Where the two meet:**
  - Guns are found per run and lost on death. Gun parts and manufacturers unlock permanently, the way Rogue Legacy unlocks gear through blueprints.
  - Class ability on B, shoot on A, swap guns with SELECT.
  - Second Wind replaces Rogue Legacy's "defy death" upgrade.
- **Cheap, high-personality features:**
  - traits done as palette, sprite or hitbox tweaks
  - joke flavor text on guns
  - risky shrine and gamble rooms
  - a shooting gallery mini-game room
