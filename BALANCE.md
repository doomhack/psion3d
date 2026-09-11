# Combat balance reference

Numbers for level design: enemy placement, room sizing, and what a fight at a given range costs
the player. Everything here is derived from two tables in the source — regenerate it when either
changes.

- Weapons: `weapons[]` in [player.c](player.c)
- Enemies: `enemyStats[]` in [enemy.c](enemy.c)

Confirmed by feel 2026-09-11. Ammo is not yet implemented (see TASKS.md); when it lands the
pistol is infinite and the SMG, AK47 and LMG each get their own pool.

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

| Type | HP | Dmg/shot | Hit chance | Opens fire at | Drops |
| --- | --- | --- | --- | --- | --- |
| Civilian | 100 | — | — | — (flees) | — |
| Mercenary | 75 | 10 | 4% (10/255) | 2 cells (4 m) | — |
| Soldier | 150 | 15 | 16% (40/255) | 3 cells (6 m) | AK47 |
| Heavy | 255 | 25 | 10% (25/255) | 4 cells (8 m) | LMG |

- Enemy accuracy *is* a hit-chance roll (unlike the player's, which is spread).
- Firing cadence: aim 0.5s → fire → attack pose 1.0s → 50% back to aim. At most one shot per
  ~1.5s while chaining. Expected damage per shot: Merc 0.4, Soldier 2.4, Heavy 2.5.
- Enemies back off if closer than 2 cells (4 m) and give up the chase beyond 14 cells (28 m).
- HP is `u8` — 255 is the ceiling without a type change.

## Placement notes

- **Engagement ranges line up with weapon sweet spots.** Mercs close to 2 cells, where the SMG is
  strongest. Heavies open up at 4 cells, exactly where the SMG has fallen to pistol parity and the
  AK47 takes over. A room that lets the player hold Heavies at 6+ cells favours the pistol/AK; a
  tight room forces the SMG against everything.
- **Corridor length sets the weapon.** Anything the player can engage from 8 cells is pistol
  territory; anything that starts inside 2 is SMG territory. Rooms of 4–6 cells across are the
  AK47's, which is also the drop the player is most likely to be holding.
- **A Heavy at range is a long fight.** 4.5s with the pistol, 6.4s with the AK, 11.5s if the player
  insists on the SMG. Enough time for flanking enemies to matter.
