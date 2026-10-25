# Design decision log

## 2026-10-12 — The CEO needs the player alive

**Agreed:** the corporation rebuilds the player because they pay for it and because
it needs a Skavengar to open the last planet. Continued reconstruction is deliberate.
This resolves the earlier question of why the service supports an opponent and
supersedes the suggestion that separate corporate departments merely overlook it.

**Open:** what a Skavengar can do that corporate forces cannot, whether Echo makes
this player essential, and how reconstruction continues after the final route opens.
The Echo/player cooperation mechanism in [story.md](story.md) is a proposal, not
an established explanation.

## 2026-10-12 — Persistent character and space campaign

**Agreed:** choose a character/class at the start and continue building that one
character. Combine a progressive campaign with repeat exploration and collection
that provides the grinding appeal of a roguelite.

**Supersedes:** selecting a new heir after death, generational replacement, and
the Manor as the campaign's progression framework. The old entry tax and automatic
loss of all guns on death are no longer assumed requirements.

**Open:** leveling, skill progression, death penalties, gear retention, and the
precise boundary between persistent and expedition-only rewards.

## 2026-10-12 — Exploration and the ancient network

**Agreed direction:** space exploration and collecting the scattered remains of a
lost civilization, gradually uncovering/restoring an ancient navigation network.
Preserve the excitement of treasure hunting with an original premise.

**Working proposals:** planets as campaign destinations; a ship workshop and
collection hub; artifacts assembling into functional devices; persistent landmarks
with remixed expedition routes; four initial planet concepts. Detailed missions,
named locations, rewards, and the network's history remain open.

## 2026-10-12 — Corporate antagonist

**Agreed direction:** a megacorporation trying to control the galaxy, led by a
concealed villain known as **the CEO**. The hidden recurring presence draws on
Claw as a reference; appearance, identity, and characterization must be original.

**Working proposals:** control of the ancient network as the means to control
travel and supplies; Meridian Industries as the company name; polite corporate
threats; an initial contractor relationship; a visible regional director.
Director Voss is a provisional name, not an established CEO identity.

## 2026-10-12 — Planetary descent direction

**Working proposal from the user:** each expedition attempts to reach a planet's
center. Develop the exploration loop around descending through increasingly
dangerous layers and recovering relics along the way.

**Supporting proposals:** ancient machines at planetary centers serve as network
nodes; reaching one is a major campaign event; unlocked access to deeper layers
could reduce repetition. These details and repeat rewards are not finalized.
Corehunter was considered for the title and set aside after the user identified
an existing game with that name.

## 2026-10-12 — Game title: Skavengar

**Agreed:** the game's name is **Skavengar**, replacing the working title RogueGB.
Use that exact spelling; an all-caps title treatment is **SKAVENGAR**.

**Working terminology:** Skavengars are independent relic and salvage explorers
who venture into planetary depths. The profession can encompass all player classes.
Pronunciation, tagline, and logo artwork remain open.

**Naming research:** preliminary web searches did not surface an obvious exact
game-title match. A similarly named AR platform,
[Scavengar](https://www.scavengar.world/), was identified. This records a limited
search result, not confirmation of name availability.

**Implementation scope:** update project-facing design documentation now. Repository
name, build identifiers, ROM filenames, hosting URLs, and the in-game title screen
have not been renamed by this documentation change.

## Next decisions

See the latest opening and guide decisions below when developing these questions.

1. First expedition and its complete collect/return/reward loop.
2. Reconstruction fee details, items outside the loadout, and voluntary return.
3. Starting character/class identities and how builds grow.
4. First planet, first collection, and the CEO's first consequential action.
5. Ancient builders, network shutdown, and the campaign's final revelation.

## 2026-10-12 — Mysterious guide and wake-up opening

**Agreed:** include the mysterious female guide who communicates with and helps
the player for undisclosed reasons. Add the Echo treatment to the story, preserving
her name, true identity, and detailed reveal as working proposals.

**Agreed opening:** throw the player into the first world, waking somewhere they
do not recognize. Teach the controls through play and build to fast action and
collecting the first relic. This supersedes starting with an assigned salvage job.
The player still chooses a continuing character/class at campaign start.

**Working proposals:** Echo first speaks through a nearby relay; the relic may
make her connection portable; core activations may reconstruct her distributed
consciousness. A defense activation and escape could provide the tutorial climax.
Not knowing the location does not establish total amnesia. Neither Echo's allegiance
nor the circumstances of waking have been settled.

The room sequence and pacing treatment are in [opening.md](opening.md); the guide's
role and possible reveal are in [story.md](story.md).

## 2026-10-12 — Death and corporate reconstruction

**Agreed:** death returns the player to the ship after the corporation rebuilds
their body. Everything in the player's loadout is retained. The corporation takes
a percentage of their money as the rebuilding fee.

**Supersedes:** the earlier rescue/lost-salvage suggestion and the unresolved choice
of where the player resumes. A specific percentage has not been chosen.

**Open:** the charged balance, rounding, zero-money behavior, definition of loadout,
unequipped finds and relics, expedition XP, mission/encounter resets, and how the
service operates during the opening and after the CEO becomes openly hostile.
No extra item-loss penalty, minimum fee, or debt system has been approved.

## 2026-10-12 — Generated levels, depth, and towns

**Agreed:** expedition levels are generated in game from the run's seed, so each
trip is unique. Rooms are assembled from authored **chunks** with wildcards, and
**visual rulesets** per theme decide how tiles, palettes, and decorations group
together. Each planet also has authored **towns** with NPCs, shops, bars, missions,
and more.

**Working numbers:** chunks are a quarter screen (5×4 blocks); rooms are 1–2
screens, set pieces 2×2; a planet's descent is about 4 layers of 8–12 rooms
(6–10 minutes each) ending at the core's guardian; beacons end each layer and
become later landing sites; the opening is a shortened layer on a fixed seed.

**Supersedes:** the roadmap's hand-made rooms with a seeded floor of three test
floors (old M2 plan).

**Open:** planet count, authoring tool for chunks and towns, town size, currency
and shop contents, mission types. Details in [level-generation.md](level-generation.md).

## 2026-10-13 — Open areas of linked maps, loot-driven progression

**Agreed:** expeditions are explored, not cleared room by room. An **area** is a
handful of linked **maps**; each map is open ground you roam freely (up to about
3×3 screens; 3×4 until 2026-10-24, when maps went to 24 rows for 128 visual blocks), and its edges open onto neighbouring maps. Progression feels like
Borderlands: **guns are the real level-up mechanic**, and play is about roaming,
looting chests and killing enemies.

**Working proposals:** 6–9 maps per area, grown from the start with a few extra
links for loops; enemy camps that wake as the player approaches and guard chests;
hidden caches; the way onward in the map farthest from the start; uncleared camps
reset when the player leaves a map, cleared ones stay cleared for the expedition.
Gun and loot generation moves up to follow the area work.

**Supersedes:** the Enter-the-Gungeon-style room grid with doors and per-room
clearing (2026-10-12 working numbers for rooms).

**Open:** whether characters level at all (XP and skills alongside gear, or gear
only); area count per descent; map transitions (fade or scroll).
