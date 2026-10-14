# Skavengar design

Living design notes for the campaign, world, and play loop. Started 2026-10-12.

Skavengar is a space exploration action RPG with repeatable roguelite expeditions
and gun loot. Choose a character/class at the start and develop that same
character throughout a progressive campaign. Explore areas, collect worthwhile
finds, and return to discover more.

## Name

**Skavengar** is the chosen game title (2026-10-12), replacing the working title
RogueGB. Use **SKAVENGAR** for an all-caps logo or title treatment. Existing
repository, ROM filenames, and technical identifiers remain RogueGB/roguegb for
now; this design decision does not rename build outputs or hosting paths.

Working world terminology: a **Skavengar** is an independent explorer who recovers
relics and salvage from planetary depths; **Skavengars** is the plural. The name
connects the player's identity to exploring, collecting, and bringing finds home.
Pronunciation and tagline are not finalized.

## Reading order

- [Enemy families](enemies.md): infected scavengers, berserkers, mercenaries,
  robots, mutant hounds, counterplay, and future variants.
- [Bosses and minibosses](bosses.md): playable trials, reference patterns,
  hardware constraints, candidate roster, and validation plan.
- [Collector gunship](gunship.md): a larger flying boss, marked bombing runs,
  laser sweeps, and exposed-core windows.
- [Design plan](design-plan.md): the remaining design questions, content catalogs,
  dependencies, and proposed first complete playable slice.
- [Story and world](story.md): the lost civilization, navigation network,
  planetary concepts, campaign arc, Echo, and corporate antagonist known as the CEO.
- [Opening sequence](opening.md): waking on the first world, learning the controls,
  meeting the mysterious guide, and recovering the first relic.
- [Level generation, depth, and towns](level-generation.md): chunk-based generated
  layers, visual rulesets, room sizes, descent depth, and authored towns.
- [Play loop and progression](play-loop.md): expeditions, collecting, persistent
  growth, reasons to revisit, and unresolved death/reward rules.
- [Decision log](decisions.md): agreed direction, superseded ideas, and questions
  to resolve next.
- [Roadmap](../../ROADMAP.md): technical decisions and implementation milestones.

## Status convention

- **Agreed direction:** explicitly chosen or endorsed in the design conversation.
- **Working proposal:** a concrete idea to develop, not a finalized requirement.
- **Open:** no decision yet; do not implement a default as though it were agreed.

These documents describe the intended game, not features already implemented.
The roadmap tracks implementation separately. Names, exact counts, numerical
balance, and detailed plot beats remain provisional unless marked agreed.

When a decision changes, update its topic document and add a dated entry to the
decision log. Update the roadmap when that decision changes implementation scope.
The inspiration notes are historical references; these design documents take
precedence for campaign and progression direction.
