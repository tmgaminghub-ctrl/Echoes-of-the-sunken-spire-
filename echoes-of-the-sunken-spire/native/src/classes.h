// ---------------------------------------------------------------------------
//  classes.h — playable character classes for Echoes of the Sunken Spire.
//
//  A class bundles the character model, weapon + shield, base stats, an attack
//  style (which animation clips and hit shape the LMB combo uses) and a single
//  signature ability on Q. Every class also carries the shared E elixir.
//
//  The table lives in classes.cpp; the engine reads it through Player::cls and
//  Game::applyClass(). Adding a class is one more row in kClasses plus (if it is
//  a genuinely new attack style) a case in the attack/ability switches.
// ---------------------------------------------------------------------------
#pragma once
#include "game.h"

enum class AttackStyle { Melee1H, Melee2H, Bow, Staff };
enum class AbilityKind { Riposte, ShieldBash, Emberbolt, PiercingShot, Whirlwind };

struct ClassDef {
  const char* id;            // stable key, also the --class CLI value
  const char* name;          // shown on the select screen / HUD
  const char* tagline;       // one line of flavour
  const char* modelPath;     // characters/*.glb
  WeaponDef weapon;
  const char* shieldProp;    // assets.prop() name, or nullptr for no shield
  const char* shieldName;
  float maxHp, maxStamina, staminaRegen;
  float moveSpeed;           // base walk speed (engine default was 5.6)
  float damageMult;          // scales the class's melee / arrow damage
  float blockDrain;          // stamina per second while holding block
  float blockAbsorb;         // stamina cost multiplier when a hit is blocked
  AttackStyle style;
  AbilityKind ability;
  const char* abilityName;
  const char* abilityDesc;
  float abilityCost;         // stamina
  float abilityCooldown;     // seconds
  V3 uiColor;                // 0..1 accent for the select card / HUD
};

extern const ClassDef kClasses[];
extern const int kClassCount;

// Returns the index for an id/name (case-insensitive), or -1.
int classIndexById(const char* id);
