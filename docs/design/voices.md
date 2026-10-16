# Voices

Status: proposal. How each character sounds on the page, for writing dialogue.
Who they are is in [story.md](story.md); this is how they talk. Every sample line
here is also a script in `content/dialogue/voices.json`, so it can be read in the
dialogue box at real size. Lines are placeholders, not the final script.

## Hearing them in the game

The ice yard (B on the title) has one of each. Walk up to Echo's relay (the
green device near the start) and her first contact plays once. Then, beside any
of them, an "A" bubble shows and A plays their samples:

| Where | Who | Script |
|---|---|---|
| The relay, near the start | Echo | `voice_echo` |
| The post by the shack | The CEO | `voice_ceo` |
| The post at the bottom right | Meridian | `voice_meridian` |
| The broker, right of the bridge | Marro | `voice_marro` |

Portraits (`artwork/portraits/`, 32x32, 3 colors plus see-through) float above
the box: Echo's is her relay's signal, the company's is its logo (the CEO and
Meridian share it), and Marro and the player have simple placeholder busts. All
four are placeholders: redraw any PNG at the same size and the next build uses it
(colors are mapped lightest to darkest).

## Writing for the box

The box shows 2 lines of about 25 letters (the italic font, wrapped at build
time). That shapes every voice:

- **One thought per page.** A line that needs three pages is two lines.
- **Short words carry further.** "Move." reads faster than "Relocate immediately."
- **Silence is a line.** A page of "..." is a beat; use it on purpose, rarely.
- **The player reads mid-run.** Story lines go where it's safe ([opening](opening.md)):
  never over a jump or a fight.
- **Names in the box are labels:** ECHO, THE CEO, MERIDIAN, YOU. A character's
  color and sound tell you who it is before the words do.

## Echo

**For:** the guide. Saves you first, asks later. Her reasons stay hidden.

**Box:** teal name, a crackle of relay static when she starts. No face: she has
none to show (portrait: the relay's signal, not a person).

**How she talks:**
- Short, exact sentences. Warnings first, then the reason, if any.
- Precise about machines and the ruins; strangely unsure about ordinary life
  above ground (weather, food, why people leave).
- Notices small things: which girder, which door, how long ago.
- Answers practical questions plainly. Dodges questions about herself without
  sounding evasive: she changes the subject to something useful.
- Gets warmer and longer the deeper you are (closer to the network's cores).

**Never:** jokes at your expense, slang, exclamation marks, explains the network,
says what she wants.

**Samples:**
- Left corridor. The right one only looks shorter.
- Wait. Count to three. Now go.
- That isn't a wall. It's a door that forgot.
- There's a cache under the third girder. Old, but sealed.
- Something heard that. Move.
- Leave that one. Please. I'll explain when I can.
- What does wind sound like, from outside?
- You came back. I didn't know if you would.

## The CEO

**For:** the threat you never see. Polite, patient, and always collecting.

**Box:** rust-red name, the company's three-note chime before every message. No
face: the Meridian logo where a face would be.

**How they talk:**
- Customer service with the warmth taken out. Complete sentences, correct grammar.
- Business words for ugly things: acquire, asset, reserve, valued, outstanding.
- Calls you by your contractor number at first; later by name, which is worse.
- Threats arrive as courtesies, often in the second sentence.
- Composure breaks in small ways only: a repeated word, a pause, a sentence
  that stops early.

**Never:** shouts, swears, explains their plan, thanks you sincerely.

**Samples:**
- You're a Skavengar. You find things. We own things.
- Contractor 7-4417. Your recent activity has been noted with interest.
- Please don't mistake our patience for a discount.
- Every relic you recover improves our quarterly outlook.
- We've reserved that planet for you. By "for you," I mean from you.
- Interesting. ... Interesting.

## Meridian Services

**For:** the company as a machine: receipts, notices, permits. The CEO's voice
with nobody behind it. Comedy, with teeth.

**Box:** the company's red and chime, name MERIDIAN.

**How it talks:** form letters. Fees and percentages, cheerful sign-offs, no
understanding of what it just said.

**Never:** a feeling, a person's name, an apology that means anything.

**Samples:**
- Body restored. Personal effects retained. Thank you for your continued business.
- Reconstruction fee: 18%. Remaining balance: 312 credits. Have a productive day.
- Your landing permit has been revoked. Please remain on the planet.
- This site is a Meridian asset. Trespass charges are accruing.

## You (the player)

**For:** a Skavengar: a professional, not a blank slate. Speaks rarely, so it counts.

**Box:** ink-colored name, YOU, no sound, a placeholder bust (your class's face later). Class flavor can come later; this is
the shared voice.

**How you talk:** short and dry. Questions more than statements. Practical first,
curious second. Never a speech: a line or two, then back to work.

**Never:** monologues, explains feelings, agrees to anything the company says.

**Samples:**
- How are you reaching this channel?
- Who are you?
- Noted.
- Then tell me what's down there.
- I'll take it. You can explain later.

## First local: the scrap broker (proposal)

**For:** the first settlement's contact: buys salvage, sells rumors, hates the
company quietly. Name proposal: **Marro**. Not decided, like the rest of the
supporting cast ([story](story.md): "NPC identities... are open").

**Box:** a warm brown name; no sound. Has a face (a placeholder bust for now).

**How they talk:** prices and salvage metaphors. Gruff, but looks out for you.
Knows everyone's business; tells you half.

**Never:** gives anything away without a price, says the company's name kindly.

**Samples:**
- Everything's worth something. Most things are worth less than you think.
- Bring me a working coil and I'll forget what you owe. Mostly.
- Meridian came through last week. Took the good pumps. Left a receipt.
- Heard a Skavengar went down the east shaft. Didn't hear them come up.
- You smell like ozone. Find something old?

## Open

- Echo's portrait: the relay's signal (proposed above), or a face that's never quite clear?
- Does the player's class change their lines, or only their art?
- The CEO's contractor number for the player, and when they learn the name.
- The broker's name, look, and settlement.
