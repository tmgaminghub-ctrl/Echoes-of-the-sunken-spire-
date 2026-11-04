# Echoes of the Sunken Spire

A third-person action-RPG set in **Vaeloria**, built on the free KayKit art packs by
[Kay Lousberg](https://kaylousberg.itch.io) (all CC0).

You play **Kaelen**, a cartographer's apprentice. The Aethel-Spire is sinking a handspan
a day and the colour is going out of the world with it. Kaelen does not solve this with
a big sword. He solves it by *looking*, by drawing what other people walked past, and by
using the tools an apprentice cartographer actually carries: a collapsible satchel, an
iron rapier, and a compass with a sprite in it.

---

## Running it

```bash
npm install
npm run dev          # http://localhost:5174
```

`?low=1` drops shadows and pixel ratio (used for the software-rendered test runs).

---

## Controls

| Keyboard / mouse | Action |
| --- | --- |
| `WASD` | move |
| mouse / `←` `→` | camera |
| `LMB` | rapier (3-hit combo, thrust on the third) |
| `RMB` | guard — with the **Mirror Shield** in hand this bends light |
| `SPACE` | jump · **hold while falling** to glide |
| `SPACE` (standing still) | dodge / backflip |
| `F` | interact · sketch · grapple · turn a mirror |
| `R` | temporal shift (in the Spire) |
| `J` | sketchbook |
| `TAB` / `MMB` | lock on |
| `ESC` | pause |

Controller (standard mapping): left stick move, right stick camera, **A** jump,
**X/RT** attack, **LT** guard, **B** interact, **Y** shift, **View** sketchbook,
**L3** temporal shift, **Start** pause.

---

## The world

One continuous 300 × 300 terrain, four regions, no loading screens.

| Region | What it is | The mechanic it teaches |
| --- | --- | --- |
| **Sunlit Valley** | village green, Miller's Crossing, Shrine of First Light | three stones, three plates — push the boulders onto the plates to open the shrine; the echo inside grants **Wind Glide** |
| **Whispering Canopy** | giant trunks, rope bridges, the Long Fall | **Pneumatic Line** (grapple) — five brass rings hung over a crevasse, and wooden shelves to land on |
| **Glass Sinks** | crystal desert, the Glass Flats | two turnable mirrors + one beam: align both and the Mirror Vault opens; inside is the **Mirror Shield** |
| **Aethel-Spire** | base terrace, 14 tapering rings, sealed gate, 68-step spiral stair, apex terrace at y=34 | **Temporal Shift** — rifts where the floor the Spire *used to have* becomes solid for twelve seconds |

### The Spire

* Sealed gate with three seal stones that light, one per recovered echo.
* A resonance bell on the Warden's Road (Act I objective).
* A gravity-defying spiral stair: 68 shallow steps of 0.5 m that Kaelen simply walks up —
  each step is a standable platform, so the climb needs no jump at all.
* Broken platform tiers (y 6.5 → 30) as an alternate route and as scenery.
* Apex terrace at y=34 with four memory plinths and the Warden's arena.
* A grey tide shell that lifts when the Spire is stabilised.

---

## Systems

### Sketchbook (`J`)
Twelve landmarks, each sketched by standing near it and pressing `F`. Sketches:
* mark the landmark on the hand-drawn map (ink dots go from pale to inked),
* fill the "field notes" page with the landmark's lore,
* every third sketch, Lyra reads a page back to you,
* sketching **all twelve** raises max vitality by 30.

Creatures get recorded automatically the first time you kill one — the field notes list
each species with a one-line weakness ("Stone Sentinel — thrusts, after its shield drops").

### Temporal Shifting (`R`)
Three rifts on the Spire climb. Standing near one and pressing `R` makes the ghost
geometry around it solid for 12 seconds (then a 3-second cooldown while the present
reasserts itself). Ghost platforms are skipped by both the player's and the enemies'
ground checks unless the shift is live.

### Tools, granted by echoes
| Tool | Source | Effect |
| --- | --- | --- |
| Wind Glide | Valley echo | hold jump while falling, fall at 2.6 m/s instead of 26 |
| Pneumatic Line | lying at the Canopy Gate | `F` on a ring pulls you to it with an arc |
| Wind Step | Heartwood echo | double jump |
| Mirror Sheen | Vault echo (equips the Mirror Shield) | guard reflects light |
| Temporal Shift | entering the Spire | see above |

### Combat
Inherited from the Ocarina-of-Time-feel combat of the previous build: continuous
animation flow (no pause between attacks and idle), lock-on with strafing and
backpedalling, hit-stop on contact, per-weapon reach/arc/damage, and enemies that are
**solid bodies** — `resolveBodyCollisions()` weights the push toward whichever body is
actually advancing, so you cannot bulldoze a sentinel across the valley.

---

## Act structure

1. **A Shifting Horizon** — wake on the green, meet Lyra (the sprite in the compass),
   ring the resonance bell at the Outer Ring.
2. **The Four Echoes** — the gate wants memories, not keys: recover the Valley, Heartwood
   and Vault echoes, each behind one tool and one puzzle. The gate opens on the third.
3. **Apex of Glass and Light** — climb the Spire, meet the Pale Warden. Beating him is
   not the win: the win is placing three echoes on the plinths and binding your own
   vitality to the Spire, which lifts the grey tide and ends the game.

---

## Code map

```
src/
  assets.js     loader for the six KayKit packs used (characters, enemies, nature,
                dungeon, platform, weapons/wargear) + propFrom/cloneWeapon/tintModel
  weapons.js    rapier, falchion, stiletto, sunstaff, warden edge; mirror/buckler/
                warden wall; rarity colours and stat lines
  world.js      terrain, zones, instanced scatter, village, roads, the chasm, crystals,
                the whole Spire (gate, seals, bell, stairs, tiers, apex, plinths, tide),
                canopy grapple rings, the mirror puzzle, the valley boulder puzzle,
                temporal rifts + ghost platforms, and world.update()
  actors.js     Player (with vertical physics: jump/coyote/buffer/glide/platforms),
                Enemy roster (wisp, sentinel, stalker, acolyte, warden), projectiles,
                weapon pickups
  abilities.js  Lyra's tools: glide, grapple, mirror, temporal, wind step
  quests.js     the act structure as a state machine + compass targets + idle hints
  ui.js         HUD, compass, dialogue, toasts, boss bar, sketchbook (map + notes)
  main.js       game loop, cameras, interaction, puzzles, puzzles' consequences,
                body collisions, the Warden fight, the ending
  fx.js         particles, rings, grapple cable, echo bursts
  audio.js      WebAudio synth: footsteps, blocks, bell, echo chime, shift sweep
  gamepad.js    standard-mapping controller support
```

## Art

All 3D assets are KayKit free packs (CC0) by Kay Lousberg:
Adventurers, Skeletons, Character Animations, Forest Nature, Dungeon, Platformer,
Fantasy Weapons Bits, Mixed Bag. Extracted into `public/assets/{nature,dungeon,platform,wargear}`.
