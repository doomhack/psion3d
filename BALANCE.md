# Combat balance reference

Numbers for level design: enemy placement, room sizing, and what a fight at a given range costs
the player. Everything here is derived from two tables in the source — regenerate it when either
changes.

- Weapons: `weapons[]` in [player.c](player.c)
- Enemies: `enemyStats[]` in [enemy.c](enemy.c)

**Weapons** were confirmed by feel 2026-09-11 and are settled. **Enemy numbers are not tuned** —
the AI state machine had a structural pass the same day (firing bands, stagger, cadence,
reposition, evade) and the values in `enemyStats[]` are placeholders that preserve prior behaviour
where they could. Tune them against the archetypes in the Enemies section.

Ammo is not yet implemented (see TASKS.md); when it lands the pistol is infinite and the SMG, AK47
and LMG each get their own pool.

## Units

- 32 game ticks per second.
- 1 map cell = 2 metres. Tables below are in cells.
- An enemy's on-screen width at `d` cells is `120 / d` pixels (`30720 / (d * 256)`).

## Formulas

- **Shots per second** = `32 / fireDelay`
- **Raw DPS** = shots per second × damage
- **Spread** = `(255 - accuracy) >> 4` columns of 4px either side of the aim point. Accuracy only
  changes spread in steps of 16; 240–255 is zero spread.
- **Hit chance at range** = fraction of the `2s + 1` spread columns whose centre lands inside the
  enemy's on-screen width. Assumes the enemy is centred in the crosshair — real hit rates at range
  run somewhat below these.
- **Effective DPS** = raw DPS × hit chance.
- **Time to kill** = `(shots - 1) × fireDelay / 32` — the first shot is instant.

## Weapons

### Raw DPS (every shot lands)

| Weapon | fireDelay | Shots/s | Dmg | **DPS** | Accuracy | Spread |
| --- | --- | --- | --- | --- | --- | --- |
| Pistol | 24 tk (0.75s) | 1.33 | 40 | **53.3** | 240 | ±0 px |
| SMG | 6 tk (0.19s) | 5.33 | 15 | **80.0** | 160 | ±20 px |
| AK47 | 8 tk (0.25s) | 4.00 | 16 | **64.0** | 208 | ±8 px |
| LMG | 5 tk (0.16s) | 6.40 | 20 | **128.0** | 192 | ±12 px |

### Time to kill, point-blank (shots needed)

| Weapon | Merc 75 | Soldier 150 | Heavy 255 |
| --- | --- | --- | --- |
| Pistol | 0.75s (2) | 2.25s (4) | 4.50s (7) |
| SMG | 0.75s (5) | 1.69s (10) | 3.00s (17) |
| AK47 | 1.00s (5) | 2.25s (10) | 3.75s (16) |
| LMG | 0.47s (4) | 1.09s (8) | 1.88s (13) |

### Hit chance vs a centred enemy

| Weapon | 1 cell | 2 cells | 4 cells | 6 cells | 8 cells |
| --- | --- | --- | --- | --- | --- |
| Pistol | 100% | 100% | 100% | 100% | 100% |
| SMG | 100% | 100% | 64% | 45% | 27% |
| AK47 | 100% | 100% | 100% | 100% | 60% |
| LMG | 100% | 100% | 100% | 71% | 43% |

### Effective DPS by range

Bold is the best of the three weapons the player usually carries. The LMG is rare and wins
everywhere by design.

| Weapon | 1 cell | 2 cells | 4 cells | 6 cells | 8 cells |
| --- | --- | --- | --- | --- | --- |
| Pistol | 53.3 | 53.3 | 53.3 | 53.3 | **53.3** |
| SMG | **80.0** | **80.0** | 50.9 | 36.4 | 21.8 |
| AK47 | 64.0 | 64.0 | **64.0** | **64.0** | 38.4 |
| LMG | 128.0 | 128.0 | 128.0 | 91.4 | 54.9 |

Sweet spots: **SMG owns 1–2 cells, AK47 owns 4–6, pistol owns 8.**

### Expected time to kill a Heavy by range (misses included)

The clearest picture of the spray-and-pray tax.

| Weapon | 1 cell | 2 cells | 4 cells | 6 cells | 8 cells |
| --- | --- | --- | --- | --- | --- |
| Pistol | 4.5s | 4.5s | 4.5s | 4.5s | 4.5s |
| SMG | 3.0s | 3.0s | 4.8s | 6.8s | 11.5s |
| AK47 | 3.8s | 3.8s | 3.8s | 3.8s | 6.4s |
| LMG | 1.9s | 1.9s | 1.9s | 2.7s | 4.6s |

### Rounds expended per kill (for when ammo lands)

Hits needed ÷ hit chance. The SMG spends 2.5× the pistol's rounds even point-blank, ~9× at 8 cells.

| Target | Weapon | 1–2 cells | 4 cells | 6 cells | 8 cells |
| --- | --- | --- | --- | --- | --- |
| Merc | SMG | 5 | 8 | 11 | 18 |
| | Pistol | 2 | 2 | 2 | 2 |
| Soldier | SMG | 10 | 16 | 22 | 37 |
| | Pistol | 4 | 4 | 4 | 4 |
| Heavy | SMG | 17 | 27 | 37 | 62 |
| | Pistol | 7 | 7 | 7 | 7 |

## Enemies

### Archetypes (the tuning brief)

- **Mercenary — busy idiot.** Medium speed. Shoots *and moves* a lot, poor accuracy. When hurt,
  occasionally panics into flight, then comes back.
- **Soldier — professional.** Fast. Very rarely panics. Evades with higher probability after being
  hit. Good accuracy.
- **Heavy — tank.** Slow, lots of HP, high damage, medium accuracy. Hard to stun-lock. Very rarely
  panics; sometimes evades when hurt.

### Current values (placeholders — see header)

| Type | HP | Dmg | Hit chance | Speed | Firing band | Stagger at | Reposition | Evade | Flee | Drops |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Civilian | 100 | — | — | 1.0 m/s | — (flees) | 0 | — | 19% | 94% | — |
| Mercenary | 75 | 10 | 4% (10/256) | 1.5 m/s | 1–2 cells | 0 | 75% | 13% | 6% | — |
| Soldier | 150 | 15 | 16% (40/256) | 2.0 m/s | 2–3 cells | 0 | 38% | 25% | 3% | AK47 |
| Heavy | 255 | 25 | 10% (25/256) | 0.5 m/s | 2–4 cells | 20 | 6% | 6% | 2% | LMG |

- Enemy accuracy *is* a hit-chance roll (unlike the player's, which is spread), so range does not
  change it.
- **Firing band** is Manhattan cells. Below the band the enemy gives ground before aiming; above it,
  it closes. The Merc's lower bound is 1, so it never retreats. Distance can never be 0.
- **Stagger at** is `staggerDamage`: a hit below it lands but does not interrupt. The Heavy's 20
  means the pistol (40) and LMG (20) rock it; SMG (15) and AK (16) rounds do not.
- HP is `u8` — 255 is the ceiling without a type change.
- Enemies give up the chase beyond 14 cells (28 m).

### Firing cycle

All three types share the shape; `aimTicks` / `attackTicks` are per type in `enemyStats[]` and
currently 16 / 32 for all.

```
aim 0.5s → fire → firing pose 1.0s → roll repositionChance
    hit:  sidestep one cell (0.5s), then aim again
    miss: aim again on the spot
```

Unsuppressed, a stationary shooter manages 13 shots in 20s. Repositioning trades shots for
movement:

| Type | shots / 20s | sidesteps / 20s | expected DPS vs player (untuned accuracy) |
| --- | --- | --- | --- |
| Mercenary | 10 | 9 | 0.20 |
| Soldier | 12 | 4 | 1.41 |
| Heavy | 13 | 0 | 1.59 |

Those DPS figures are why enemy accuracy/damage is the next tuning target: a lone Merc currently
needs minutes to kill a 100 HP player.

### Being hit

A hit at or above `staggerDamage` puts the enemy in `HURT` for 0.25s and cancels its move. Two
rules stop that becoming a lock:

- A hit on an enemy already in `HURT` applies damage but does **not** restart the flinch.
- On leaving `HURT`, an interrupted aim or firing pose **resumes with its remaining time** rather
  than restarting. Under sustained fire the enemy still gains ground toward its shot.

Enemy shots fired in 20s while being hit continuously (unsuppressed = 13):

| Enemy | Pistol | SMG | AK47 | LMG |
| --- | --- | --- | --- | --- |
| Merc / Soldier (stagger 0) | 8 | 3 | 6 | 1 |
| Heavy (stagger 20) | 8 | **13** | **13** | 1 |

The Heavy is untouched by SMG and AK fire. The pistol suppresses a Heavy better than the SMG does —
infinite ammo, engages at the Heavy's range, and staggers it — which is the pistol's anti-Heavy
niche falling out of the threshold. Before this pass every combination in that table was **0**.

On leaving `HURT` the enemy rolls `fleeChance` (panic: run 6 cells, then return) and then
`evadeChance` (0.4s pause, then a 0.5s sidestep, then back to the fight — 1.15s total, the same
for every type). Evade is **only** rolled after a hit; approaches are now the shortest path in.

## Placement notes

- **Engagement ranges line up with weapon sweet spots.** Mercs fight from 1–2 cells and never back
  off, which is where the SMG is strongest. Heavies open up at 4 cells, exactly where the SMG has
  fallen to pistol parity and the AK47 takes over — and the pistol is the weapon that staggers them.
  A room that lets the player hold Heavies at 6+ cells favours the pistol/AK; a tight room forces
  the SMG against everything, and the SMG cannot stagger a Heavy.
- **Mercs crowd.** With no minimum range and a 75% reposition rate, a group of Mercs will close to
  adjacent and shuffle around the player. Give them room to do it or they will pin the player
  against walls.
- **Corridor length sets the weapon.** Anything the player can engage from 8 cells is pistol
  territory; anything that starts inside 2 is SMG territory. Rooms of 4–6 cells across are the
  AK47's, which is also the drop the player is most likely to be holding.
- **A Heavy at range is a long fight.** 4.5s with the pistol, 6.4s with the AK, 11.5s if the player
  insists on the SMG. Enough time for flanking enemies to matter.
