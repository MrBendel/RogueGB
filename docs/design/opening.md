# Opening sequence: Dustwake

Status: playable opening implemented 2026-11-08. New characters wake in the desert,
find a shield, fight toward Dust Crossroads, and reach town. The first relic is a
later campaign beat; the earlier chamber/escape sequence below remains a proposal.

## Playable opening

Art rule: build from the town's Sands desert tileset and its material palettes.
Reuse complete cliff formations, painted ground edges and correctly assembled
props; keep the shared atlas instead of introducing a separate desert art style.

Choose a persistent class, then wake prone in Dustwake, a 20x24 authored desert
map. The entity contacts you under the name UNKNOWN, without revealing a portrait
or explaining why she needs you alive. Her first instruction is practical: find
the guarded supply case beyond the Cragbacks to the north.

The shield is the reward for defeating both three-creature Cragback groups.
The player fights unshielded with the starter pistol; the first transmission
teaches movement, firing, jumping and the rocks' landing warnings before combat.
The supply case sits beyond the groups, near the north exit, and stays locked
until all six creatures are defeated. Walking up then equips a guaranteed
permanent tier-one shield (25 capacity), followed by the recharge explanation.
The opening's north exit waits until that reward is collected.

The small Cragback relatives (Rocklings) have no elites or reinforcements. Each
marks a landing point, winds up, then lobs a rock. Move clear or jump the impact;
the target never homes. Their bodies use two hardware sprites each so the group
leaves room for rocks, warnings, the player and gunfire.

Continue north into Dust Crossroads, then north again into Sands Town. A final
transmission acknowledges safety and leaves the entity's identity unresolved.
START's map shows the current objective: defeat the rock throwers, claim the shield, then head north to town.
The side road to Rustyard and the town's underground lift remain available.

The opening applies only to new files; established saves keep their progress.
Before reaching town, death restarts at Dustwake's safe pocket with the found
shield retained and the existing reconstruction fee. Heard contact messages do
not replay. Arrival completes the introduction. Surface travel does not increase
underground best-depth progression. Existing story bits store this state without
changing the save format.


## Agreed experience

The game throws the player into the first world. They wake having no idea where
they are. The opening teaches the controls through play, gathering momentum until
the player is running through danger and collecting their first relic. Echo, the
mysterious guide described in [story.md](story.md), helps without explaining her
stake in the adventure.

Keep the existing choice of a persistent character/class at campaign start. A brief
selection can precede the wake-up scene without a long ship, briefing, or job-board
sequence. Being disoriented does not require a blank personal history.

## Earlier relic sequence proposal (after the shield opening)

| Beat | Player experience | What it teaches |
|---|---|---|
| Wake | Stand in a quiet, damaged chamber with a visible way forward; immediate movement control | D-pad movement and reading the environment |
| First contact | A nearby relay crackles. Echo warns about a hazard the player can see | Her advice is useful, but her identity is unknown |
| Find a path | Cross a safe practice gap, then navigate a small obstacle | B to jump; introduce the second mid-air B press for a roll only after a successful jump |
| Fight back | Obtain or ready the starting weapon and face one readable enemy | A to fire, the target reticle, and movement while shooting |
| First find | Open or discover a simple supply cache on the route | Pickups and the reward of looking around; use the eventual pickup interaction once decided |
| Build momentum | Combine movement, shooting, jumps, and rolls through connected rooms | Use learned controls together, with fewer prompts |
| The relic | Reach a chamber containing an unmistakably special object and recover it | First story discovery; a brief, clear collection payoff |
| Get clear | The relic's removal could wake defenses and open a short escape route | A proposed action climax using skills already learned |
| Breathe | Reach a safe overlook or shelter with the relic | A moment to inspect the find and hear the next lead |

The escape trigger and exact order of the final action beats are proposals. The
required outcome is momentum, successful use of the controls, and the first relic.
Avoid introducing a literal sprint mechanic merely because the sequence feels fast.

## Echo's introduction

Her first message should address an immediate problem. She earns attention before
asking the player to recover anything. Keep identity questions unanswered without
making every response evasive: she can give clear practical answers about the room.

Use a local relay for first contact, before the player owns the relic. One possible
payoff is that taking the relic carries her signal with the player. Her exact
communication rules remain open; the first-contact sequence must be consistent
with whichever rules are chosen.

Possible brief exchange after collection:

> Player: What is this thing?
>
> Echo: Still working. Keep it close.

The player should recognize that the relic matters without learning the whole
network history or Echo's secret in the tutorial.

## Teaching and pacing proposals

- Introduce one action in a low-pressure situation before combining it with combat.
- Use short contextual text suitable for the Game Boy Color screen. Pause for
  required reading or place it in safe moments; avoid dialogue over a dangerous jump.
- Let room layout and enemy tells teach as much as prompts do.
- Keep a short optional cache detour to establish curiosity and collecting early.
- Defer full inventory comparisons, weapon wheels, class abilities, and broader
  upgrade systems until the basic movement/combat loop is understood.
- Use a deliberately authored opening route; later repeatable expeditions can remix
  encounters and branches. Decide checkpoint/retry behavior separately so early
  failure does not require replaying all dialogue.
- Make the first relic an authored discovery rather than a chance drop. Its exact
  function and death-retention behavior still need a decision.

## Connection to the campaign

This first sequence is a special introduction, not proof that every expedition
starts with waking up or ends in a chase. Once the player reaches safety, introduce
the ship and the ability to choose a lead, collect, return, improve, and repeat.

The first relic can point toward a deeper site on this planet. It need not be the
planetary core itself; reaching a center should retain its campaign significance.

## Next decisions

- Which first world and chamber best communicate the setting?
- Why is the character here, and what happened just before they woke?
- Is the starting weapon already carried or recovered nearby?
- What is the relic's appearance, name, and first useful effect?
- What causes the final burst of danger, if an escape is used?
- Where is the first safe point, and how does the player reach their ship?
