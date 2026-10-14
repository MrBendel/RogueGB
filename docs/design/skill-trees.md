# RogueGB — Wayfarer Class Skill Trees & Progression System

> **Design Target**: Nintendo Game Boy (DMG) / Game Boy Color (CGB).  
> **UI Spec**: 160×144 resolution, 20×18 tile window layer, single-byte bitmask flags, 60fps SM83 CPU budget.

---

## 1. Handheld UI & Technical Architecture

### 1.1 The 3×3 Tiered Triad
To fit within the **20×18 tile Game Boy screen** while maintaining instant readability, each class features a **Tiered Triad**:
- **3 Archetype Branches** per class (e.g. Defense, Firearms, Utility)
- **3 Tiers** per branch (Tier 1: Novice, Tier 2: Specialist, Tier 3: Capstone)
- Total of **9 skills per class**, allowing focused, meaningful builds without sprawling desktop-style menus.

```text
+--------------------+ (20 columns x 18 rows)
| LVL 3 SKILL SELECT |
+--------------------+
| [A]   [B]   [C]    |  <- 3 Archetype Branches
| BAST  MARK  ORDN   |
|                    |
| > [B2] HEAVY RIFLE |  <- Selected Skill
| +25% shot speed.   |
| -1 recoil step.    |
|                    |
| Points: 1 Available|
+--------------------+
| [A] Learn  [B] Back|
+--------------------+
```

### 1.2 Zero-Overhead Memory Layout
All unlocked skills for a run are packed into a single `uint16_t` bitmask in `player.h`:

```c
// 9 skill bits per class (3 branches x 3 tiers), leaving 7 bits for relics/buffs
#define SKILL_A1  (1 << 0)
#define SKILL_A2  (1 << 1)
#define SKILL_A3  (1 << 2)
#define SKILL_B1  (1 << 3)
#define SKILL_B2  (1 << 4)
#define SKILL_B3  (1 << 5)
#define SKILL_C1  (1 << 6)
#define SKILL_C2  (1 << 7)
#define SKILL_C3  (1 << 8)

extern uint16_t player_skills;
```

Checking if an upgrade is active during fast combat routines (such as bullet collision or sword swings) compiles down to a single 2-cycle hardware opcode:
```assembly
bit 4, [hl]   ; Check Tier B2: Heavy Rifling
```

---

## 2. Wayfarer Classes & Complete Skill Trees

```mermaid
graph TD
    classDef ironclad fill:#2a4d69,stroke:#4b86b4,stroke-width:2px,color:#fff;
    classDef ballist fill:#993d3d,stroke:#e06d53,stroke-width:2px,color:#fff;
    classDef volt fill:#3b6978,stroke:#20b2aa,stroke-width:2px,color:#fff;
    classDef stalker fill:#8c7832,stroke:#d4af37,stroke-width:2px,color:#fff;

    IC[Ironclad<br/>Vanguard Bulwark]::ironclad
    BL[Ballistician<br/>Heavy Ordnance]::ballist
    VW[Volt-Weaver<br/>Static Mystic]::volt
    GS[Ghost-Stalker<br/>Outland Scout]::stalker

    IC --> |Bastion| IC_A[Defense & Aegis]
    IC --> |Marksman| IC_B[Heavy Weapon Drill]
    IC --> |Ordnance| IC_C[Munitions Supply]

    BL --> |Hailfire| BL_A[Fire Rate & Shrapnel]
    BL --> |Heavy Munitions| BL_B[Thermite & Drones]
    BL --> |Blitz Drill| BL_C[Athletics & Momentum]

    VW --> |Arc Mastery| VW_A[Chain Lightning & Sparks]
    VW --> |Warp Mobility| VW_B[Blink & Decoys]
    VW --> |Resonant Field| VW_C[Siphon & Stun Waves]

    GS --> |Dead-Eye| GS_A[Critical Aim & Sniping]
    GS --> |Phantom Blade| GS_B[Sweeping Ripostes]
    GS --> |Veilcraft| GS_C[HUD Intel & Camouflage]
```

---

### Class 1: The Ironclad (Vanguard Bulwark)
> *Heavily armored shock trooper engineered for sustained frontline survival and counter-offensives.*  
> **Sprite**: `wayfarer_ironclad` (`16×24`, cobalt-plated armor, `gbc_guard_blue`).

| Branch | Tier 1 (Novice) | Tier 2 (Specialist) | Tier 3 (Capstone) |
| :--- | :--- | :--- | :--- |
| **Branch A: Bastion** *(Defense)* | **Plated Mesh**<br/>Enemy contact damage reduced by 1; Max HP +15. | **Capacitor Surge**<br/>Shield recovery delay reduced by 30 frames; recharges while walking. | **Juggernaut Shockwave**<br/>When shield breaks, emit a 360° pulse knocking enemies back 24px and clearing enemy bullets. |
| **Branch B: Marksmanship** *(Weapon Handling)* | **Hair-Pin Draw**<br/>Halves reload duration (`reload`); weapon swap delay removed. | **Heavy Rifling**<br/>+25% shot velocity (`speed`); recoil kick reduced by 1 step. | **Breach & Clear**<br/>Final round in each magazine deals 2× damage and instantly refunds 1 bonus shot. |
| **Branch C: Ordnance** *(Ammunition & Supply)* | **Expanded Drums**<br/>All firearm magazine capacities (`mag`) +2 rounds. | **Alloy Penetrators**<br/>Shots pierce through 1 enemy or destructible obstacle. | **Resupply Beacon**<br/>Defeating an elite/miniboss drops an instant combat supply crate (Shield + Full Ammo). |

---

### Class 2: The Volt-Weaver (Static Mystic)
> *Kinetic psion channeled through superconductor circuitry, commanding room-wide lightning and space warp.*  
> **Sprite**: `wayfarer_volt_weaver` (`16×24`, electric robes, `gbc_volt_blade`).

| Branch | Tier 1 (Novice) | Tier 2 (Specialist) | Tier 3 (Capstone) |
| :--- | :--- | :--- | :--- |
| **Branch A: Arc Mastery** *(Electric DPS)* | **Galvanic Charge**<br/>Moving accumulates static electricity; next weapon attack deals +2 bonus Volt shock damage. | **Chain Discharge**<br/>Projectiles arc to a secondary nearby foe for 50% damage. | **Tesla Storm**<br/>Melee swings release 3 homing ball-lightning sparks that seek enemy targets. |
| **Branch B: Warp Mobility** *(Movement)* | **Blink Dash**<br/>Dodge roll distance +8px; grants 4 additional invulnerability frames. | **Kinetic Decoy**<br/>Dashing leaves a static afterimage clone that draws enemy aim for 45 frames. | **Fracture Warp**<br/>Re-pressing Dash mid-jump teleports forward 24px, dealing 3 shock damage to all enemies crossed. |
| **Branch C: Resonant Field** *(Energy Shields)* | **Flux Siphon**<br/>Absorbing or grazing projectile damage restores 1 shield point. | **Reflective Edge**<br/>Swinging a melee weapon against an enemy bullet deflects it at 2× speed toward auto-aim. | **Superconductor**<br/>When HP falls below 25%, unleash an EMP pulse that freezes all room enemies for 90 frames. |

---

### Class 3: The Ballistician (Heavy Ordnance)
> *Frontline gunner utilizing explosive munitions, automated support emplacements, and rolling mobility.*  
> **Sprite**: `wayfarer_ballistician` (`16×24`, crimson flak vest & bandolier, `gbc_hero_red`).

| Branch | Tier 1 (Novice) | Tier 2 (Specialist) | Tier 3 (Capstone) |
| :--- | :--- | :--- | :--- |
| **Branch A: Hailfire** *(Saturation)* | **Rapid Cycle**<br/>Fire rate delay (`fire_interval`) reduced by 2 frames on all guns. | **Cluster Shot**<br/>Adds +1 pellet to shotguns and burst rifles without consuming additional ammunition. | **Shrapnel Bloom**<br/>Critical hits detonate on impact into 3 explosive flechettes. |
| **Branch B: Heavy Munitions** *(Explosives)* | **Bandolier**<br/>Total reserve ammo capacity increased by 50%. | **Thermite Rounds**<br/>Projectiles ignite targets, burning them for 1 damage/sec over 3 seconds. | **Sentry Pod**<br/>Pressing `SELECT+A` deploys an autoturret that fires 8-shot suppressive bursts at nearest targets. |
| **Branch C: Blitz Drill** *(Combat Roll)* | **Tactical Roll**<br/>Movement speed increases by +15% for 45 frames after executing a dodge dash. | **Momentum**<br/>Firing weapons no longer slows down movement speed (`slow = 0`). | **Overdrive**<br/>Slaying 3 enemies within 2 seconds grants infinite ammunition for the subsequent 3 seconds. |

---

### Class 4: The Ghost-Stalker (Outland Scout)
> *Covert ranger specializing in sniper precision, lethal parry counters, and optical cloaking.*  
> **Sprite**: `wayfarer_ghost_stalker` (`16×24`, gold hooded optic cloak, `gbc_monk_gold`).

| Branch | Tier 1 (Novice) | Tier 2 (Specialist) | Tier 3 (Capstone) |
| :--- | :--- | :--- | :--- |
| **Branch A: Dead-Eye** *(Precision Gunplay)* | **Hawk Sight**<br/>Auto-aim target acquisition range +32px; lock-on transition is instant. | **Ambush Strike**<br/>First attack on any enemy above 75% HP deals +100% damage. | **Fatal Horizon**<br/>Sniping a target below 30% HP triggers an instant kill and resets weapon reload cooldown. |
| **Branch B: Phantom Blade** *(Melee Combos)* | **Sweeping Edge**<br/>Melee slash arc widened from 90° to a full 180° frontal cone. | **Deflection Parry**<br/>Striking an enemy during their attack telegraph stuns them for 1 second. | **Cyclone Slash**<br/>3-hit melee combo finishes with an omnidirectional 360° spin slash. |
| **Branch C: Veilcraft** *(Infiltration)* | **Sixth Sense**<br/>Offscreen enemy coordinates are projected onto the HUD border. | **Mirage Cloak**<br/>Defeating an enemy grants 1 second of optical camouflage (untargetable by enemy AI). | **Shadow Decoy**<br/>Dodge roll spawns a decoy silhouette that replicates your next weapon attack. |

---

## 3. Run Progression & Acquisition Loop

### 3.1 Relic Cores & Selection Points
- **Point Triggers**:
  - Clearing Room 3 of a floor (Combat Trial).
  - Defeating Floor Minibosses or Chamber Guardians.
  - Opening locked Golden Treasure Altar chests (`pickup_chest`).
- **Level-Up Sequence**:
  1. Relic Beacon activates in room center.
  2. Player steps onto beacon: audio fanfare plays, game pauses.
  3. Window Layer (`WY`) slides up over the screen:
     - 3 Skill Cards are offered based on current class and unlocked prerequisite tiers.
     - D-Pad Left/Right selects card; `A` button confirms.
     - Window Layer slides down; player flashes with power-up palette.

### 3.2 Synergies with Weapon Arsenal
Skills directly modify the core weapons available in the dungeon:
- **Volt Sabre (`melee_sword`)** + *Tesla Storm* = 3 Homing lightning sparks per swing.
- **Heavy Cleaver (`battle_axe`)** + *Cyclone Slash* = 360° armor-cleaving spin.
- **Pulsar Carbine (`laser_carbine`)** + *Alloy Penetrators* = Piercing hallway laser grid.
- **Plasma Scepter (`plasma_staff`)** + *Chain Discharge* = Arcing electric blast webs.
