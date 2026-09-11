# Combat balance reference

Numbers for level design: enemy placement, room sizing, and what a fight at a given range costs
the player. Everything here is derived from two tables in the source — regenerate it when either
changes.

- Weapons: `weapons[]` in [player.c](player.c)
- Enemies: `enemyStats[]` in [enemy.c](enemy.c)

**Weapons** were confirmed by feel 2026-09-11 and are settled. **Enemy cadence and danger were
tuned the same day** after a structural pass on the AI (firing bands, stagger, burst fire,
reposition, evade, mobility through openings). Movement and fire rate for all three combat types
were confirmed by feel; damage and accuracy were then set to a target time-to-kill per tier. Still
untouched: `evadeChance` and `fleeChance`, which should be judged in play before moving.

The danger reference is Goldeneye 007 on Agent: one guard takes ~100s to kill a passive player from
full health, two take ~50s. The Merc is anchored there; the Soldier and Heavy halve it per tier.

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

### Current values

| Type | HP | Dmg | Hit chance | Speed | Firing band | Stagger at | Wind-up | Interval | Reposition | Evade | Flee | Drops |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Civilian | 100 | — | — | 1.0 m/s | — (flees) | 0 | — | — | — | 19% | 94% | — |
| Mercenary | 75 | 10 | 9.4% (24/256) | 1.5 m/s | 1–2 cells | 0 | 0.75s | 0.25s | 50% | 13% | 6% | — |
| Soldier | 150 | 10 | 9.8% (25/256) | 2.0 m/s | 2–3 cells | 0 | 0.50s | 0.31s | 25% | 25% | 3% | AK47 |
| Heavy | 255 | 20 | 10.9% (28/256) | 0.5 m/s | 1–4 cells | 20 | 1.00s | 0.41s | 12.5% | 6% | 2% | LMG |

Wind-up is `aimTicks`, interval is `attackTicks`, reposition is `repositionChance` — in ticks
24/8/128, 16/10/64 and 32/13/32 respectively. All three combat types sit in the same ~10% accuracy
band on purpose: the tiers are carried by things the player can see — rate of fire, burst length,
how hard a hit lands — not by whether rounds land.

- Enemy accuracy *is* a hit-chance roll (unlike the player's, which is spread), so range does not
  change it.
- **Firing band** is Manhattan cells. Above it the enemy closes; below it, only the Soldier gives
  ground (a 1s walk at 2 m/s). The Merc and Heavy have a lower bound of 1 and never retreat — the
  Heavy's old retreat was a 4s walk during which it could be kited for free. Distance can never
  be 0.
- **Stagger at** is `staggerDamage`: a hit below it lands but does not interrupt. The Heavy's 20
  means the pistol (40) and LMG (20) rock it; SMG (15) and AK (16) rounds do not.
- HP is `u8` — 255 is the ceiling without a type change.
- Enemies give up the chase beyond 14 cells (28 m). Now that doors and arches no longer stop a
  pursuit, this leash is the only thing that ends one.

### Movement and sight

Enemies use the same openings the player does.

| Cell | Enemy may enter | Blocks enemy sight |
| --- | --- | --- |
| Floor, pickup | yes | no |
| Archway | yes | no |
| Unlocked door | yes | **only while closed** |
| Locked door | no | yes |
| Window, bars, low wall, pillar | no | no |
| Solid wall, decor, another enemy | no | walls yes; decor and enemies no |

- An **unlocked door is open** while the player or any enemy is in the door cell or one of its four
  neighbours. That single test drives the door's drawn gap, enemy line of sight, and therefore what
  can be shot through in both directions. A door seen opening on its own means something is coming
  through it.
- Enemies are an **overlay** on the cell they stand on; the arch, door or pickup beneath is
  preserved and restored when they move off. A pickup under an enemy is hidden until it moves.
- **Corpses clear** about 1.3s after death (0.3s dying + 1s down). Combat types leave their weapon
  on that cell, or on an adjacent floor cell if they died in an opening. A body is an obstacle to
  other enemies only for that window, never permanently.

### Firing cycle

Fire is in bursts. `aimTicks` is the wind-up before a burst (and after every reposition or
flinch); `attackTicks` is the interval between rounds within it; and after each round
`repositionChance` decides whether that was the last — so one knob sets burst length and movement
together. Mean burst is `1 / repositionChance`, but the length is geometric: a 25% roll gives bursts
averaging 4 of which a quarter are single shots and one in eight run past 8.

```
aim (wind-up) → fire → interval → still in sight and in range?
    no:  search / chase, fresh aim next time
    yes: roll repositionChance
         hit:  sidestep one cell (0.5s), then wind up again
         miss: fire again, no re-aim
```

The muzzle-flash frame shows for `ENEMY_FLASH_TICKS` (4) after each round, aim frame between.

| Type | first shot | shots / s | mean burst | burst lasts | sidesteps / s | time split (aim / burst / move) |
| --- | --- | --- | --- | --- | --- | --- |
| Mercenary | 0.75s | 1.08 | 2 | 0.3s | 0.53 | 42 / 29 / 28 |
| Soldier | 0.50s | 1.71 | ~4 | 1.0s | 0.39 | 21 / 59 / 21 |
| Heavy | 1.00s | 1.62 | ~8 | 3.1s | 0.19 | 20 / 71 / 10 |

Read as silhouettes: the Merc winds up, double-taps, moves; the Soldier is quickest on the trigger,
works the target in short bursts and rarely moves; the Heavy gives a full second of warning, then
a long stream from where it stands.

### Danger

Player at 100 HP, standing still, one enemy in its band with line of sight. "Hit every" is the
figure that governs how dangerous an enemy *feels*; time to kill is what governs whether you live.

| Type | expected DPS | hit every | **TTK, one** | two | vs Goldeneye Agent |
| --- | --- | --- | --- | --- | --- |
| Mercenary | 1.0 | 9.8s | **~100s** | ~50s | one guard |
| Soldier | 1.7 | 6.0s | **~60s** | ~30s | |
| Heavy | 3.5 | 5.7s | **~28s** | ~14s | |

Mixed groups add: a Soldier and two Mercs are ~3.7 DPS, ~27s. Each enemy is an independent shooter,
so the group figure is just the sum.

Why each is dangerous differs, which is the point of the roster: the Merc through numbers (alone it
is a nuisance), the Soldier through rate and discipline, the Heavy through weight — 20-damage hits,
and the two close-range weapons cannot stagger it.

### Being hit

A hit at or above `staggerDamage` puts the enemy in `HURT` for 0.25s and cancels its move. Two
rules stop that becoming a lock:

- A hit on an enemy already in `HURT` applies damage but does **not** restart the flinch.
- On leaving `HURT`, an interrupted aim or firing pose **resumes with its remaining time** rather
  than restarting. Under sustained fire the enemy still gains ground toward its shot.

Enemy shots fired in 20s while being hit continuously, against the free rate in brackets:

| Enemy | free | Pistol | SMG | AK47 | LMG |
| --- | --- | --- | --- | --- | --- |
| Mercenary (stagger 0) | 22 | 13 | 6 | 10 | 2 |
| Soldier (stagger 0) | 34 | 25 | 10 | 16 | 2 |
| Heavy (stagger 20) | 32 | 17 | **29** | **29** | 2 |

Suppression is real but not total: even under SMG fire a Merc lands a few rounds back, and a flinch
mid-burst delays the burst rather than resetting it. The Heavy is barely touched by SMG and AK fire.
**The pistol suppresses a Heavy better than the SMG does** — infinite ammo, engages at the Heavy's
range, and staggers it — which is the pistol's anti-Heavy niche falling out of the threshold. The
LMG is the only weapon that genuinely shuts anything down. Before the structural pass every
combination in that table was **0**.

On leaving `HURT` the enemy rolls `fleeChance` (panic: run 6 cells, then return) and then
`evadeChance` (0.4s pause, then a 0.5s sidestep, then back to the fight — 1.15s total, the same
for every type). Evade is **only** rolled after a hit; approaches are now the shortest path in.

## Placement notes

- **Engagement ranges line up with weapon sweet spots.** Mercs fight from 1–2 cells and never back
  off, which is where the SMG is strongest. Heavies open up at 4 cells, exactly where the SMG has
  fallen to pistol parity and the AK47 takes over — and the pistol is the weapon that staggers them.
  A room that lets the player hold Heavies at 6+ cells favours the pistol/AK; a tight room forces
  the SMG against everything, and the SMG cannot stagger a Heavy.
- **Mercs crowd.** With no minimum range and a sidestep after half their shots, a group of Mercs
  will close to adjacent and shuffle around the player. Give them room to do it or they will pin
  the player against walls.
- **Budget rooms in TTK.** One Merc is ~100s, a Soldier ~60s, a Heavy ~28s, and they sum. A room
  meant to be survived standing still for ~20s can hold roughly a Soldier plus two Mercs, or one
  Heavy; anything past that requires the player to move or kill. Mercs on their own never make a
  room dangerous — they make it loud and busy, which is their job.
- **The Heavy's wind-up is the counter.** A full second of aim frame before every burst, and only
  the pistol or LMG can interrupt it. Sightlines that let the player see a Heavy raise its gun from
  4+ cells reward the switch to the pistol; a Heavy round a corner at 1 cell does not.
- **Corridor length sets the weapon.** Anything the player can engage from 8 cells is pistol
  territory; anything that starts inside 2 is SMG territory. Rooms of 4–6 cells across are the
  AK47's, which is also the drop the player is most likely to be holding.
- **A Heavy at range is a long fight.** 4.5s with the pistol, 6.4s with the AK, 11.5s if the player
  insists on the SMG. Enough time for flanking enemies to matter.
- **Heavies plant.** They never retreat, so a Heavy in a doorway holds it, and closing on one no
  longer buys silence — at 1 cell it fires for 25 and the SMG cannot stagger it. The player can
  always disengage (4 m/s against 0.5) but cannot kite.
- **Rooms are no longer islands.** Enemies follow through arches and doorways and hunt to the last
  place they saw the player, so a room is only as safe as its exits. Use the 14-cell leash to
  bound a chase: a retreat longer than that sheds pursuers, a shorter one does not.
- **Doors are a tell.** A closed door hides the player from anything behind it and vice versa. It
  opens the moment an enemy reaches the far side, so a door that opens by itself is warning of an
  arrival — and a player who wants to hold a door shut must not stand next to it.
- **Choke points are temporary.** A kill in a corridor blocks the enemies behind it for ~1.3s, then
  the body clears and the weapon drops. It buys a beat, not a barricade.
