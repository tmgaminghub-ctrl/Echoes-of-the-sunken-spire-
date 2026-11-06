// ---------------------------------------------------------------------------
//  classes.cpp — the five playable paths of Vaeloria.
//
//  Stats are balanced around the original Kaelen (the Duelist): 120 vitality,
//  100 stamina, 5.6 m/s, a 16-damage rapier. Every other class trades one of
//  those axes for another, and each carries a signature ability on Q.
// ---------------------------------------------------------------------------
#include "classes.h"
#include <cstring>
#include <cctype>

// gripEuler matches the hand-slot orientation the engine already uses for
// swords; scale/comboCancel follow the same convention as kRapier.
const ClassDef kClasses[] = {
  // ---------------------------------------------------------------- Duelist
  {
    "duelist", "Duelist", "Ink and iron - the cartographer's own path.",
    "characters/Rogue.glb",
    {"Heirloom Iron Rapier", "wargear/sword_D.gltf", 16.f, 2.5f, 1.2f, V3(2.876f, 0.f, 1.84f), 1.f, 0.72f},
    "shield_B", "Apprentice's Buckler",
    120.f, 110.f, 16.f,     // hp, stamina, regen
    5.8f, 1.0f,             // move speed, damage mult
    5.f, 1.4f,              // block drain, block absorb
    AttackStyle::Melee1H, AbilityKind::Riposte,
    "Riposte", "A lunging counter-thrust with brief invulnerability.",
    20.f, 2.5f,
    V3(0.56f, 0.88f, 1.0f)
  },
  // ---------------------------------------------------------------- Warden
  {
    "warden", "Warden", "A bulwark against the grey.",
    "characters/Knight.glb",
    {"Warden's Longsword", "weapons/sword_1handed.gltf", 18.f, 2.6f, 0.95f, V3(2.876f, 0.f, 1.84f), 1.f, 0.75f},
    "shield_round", "Round Bastion",
    170.f, 100.f, 12.f,
    5.0f, 1.15f,
    3.5f, 0.9f,
    AttackStyle::Melee1H, AbilityKind::ShieldBash,
    "Shield Bash", "A wide blow that staggers and drives enemies back.",
    25.f, 4.f,
    V3(0.91f, 0.76f, 0.48f)
  },
  // ------------------------------------------------------------- Ember Mage
  {
    "mage", "Ember Mage", "The last warmth of the sun, carried in a staff.",
    "characters/Mage.glb",
    {"Cinderwood Staff", "wargear/staff_a.gltf", 10.f, 2.4f, 1.05f, V3(2.876f, 0.f, 1.84f), 1.f, 0.7f},
    nullptr, "",
    85.f, 130.f, 20.f,
    5.5f, 0.7f,
    8.f, 2.0f,
    AttackStyle::Staff, AbilityKind::Emberbolt,
    "Emberbolt", "Hurl a bolt of living fire at the lock-on target.",
    15.f, 0.9f,
    V3(1.0f, 0.55f, 0.25f)
  },
  // ---------------------------------------------------------------- Ranger
  {
    "ranger", "Ranger", "The canopy's quiet arrow.",
    "characters/Ranger.glb",
    {"Heartwood Longbow", "wargear/bow_a_withstring.gltf", 14.f, 2.0f, 1.0f, V3(2.876f, 0.f, 1.84f), 1.f, 0.7f},
    nullptr, "",
    100.f, 115.f, 15.f,
    6.1f, 1.0f,
    7.f, 1.8f,
    AttackStyle::Bow, AbilityKind::PiercingShot,
    "Piercing Shot", "An arrow that passes through every enemy in its line.",
    22.f, 2.2f,
    V3(0.6f, 0.9f, 0.5f)
  },
  // -------------------------------------------------------------- Berserker
  {
    "berserker", "Berserker", "Grief, given an axe.",
    "characters/Barbarian.glb",
    {"Greytide Greataxe", "wargear/axe_c.gltf", 24.f, 3.0f, 0.8f, V3(2.876f, 0.f, 1.84f), 1.f, 0.65f},
    nullptr, "",
    150.f, 120.f, 13.f,
    5.3f, 1.45f,
    9.f, 2.2f,
    AttackStyle::Melee2H, AbilityKind::Whirlwind,
    "Whirlwind", "A spinning cleave that strikes every foe around you.",
    30.f, 5.f,
    V3(0.95f, 0.4f, 0.35f)
  },
};

const int kClassCount = (int)(sizeof(kClasses) / sizeof(kClasses[0]));

static bool ieq(const char* a, const char* b) {
  while (*a && *b) {
    if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
    a++; b++;
  }
  return *a == *b;
}

int classIndexById(const char* id) {
  if (!id || !*id) return -1;
  for (int i = 0; i < kClassCount; i++)
    if (ieq(kClasses[i].id, id) || ieq(kClasses[i].name, id)) return i;
  // friendly aliases
  if (ieq(id, "rogue")) return 0;
  if (ieq(id, "knight")) return 1;
  if (ieq(id, "embermage") || ieq(id, "ember")) return 2;
  if (ieq(id, "barbarian")) return 4;
  return -1;
}
