// ---------------------------------------------------------------------------
//  worlds.cpp — Echoes of the Sunken Spire, built for the
//               native engine out of the same KayKit packs the web builds use.
// ---------------------------------------------------------------------------
#include "game.h"
#include <cstdio>

// ============================================================ helpers
static void scatterProps(World& w, Assets& as, const char* name, int count, float radius,
                         float minR, float maxR, float sMin, float sMax, uint32_t seed,
                         bool solid = false, float solidR = 0.6f, float avoidX = 0, float avoidZ = 0,
                         float avoidR = 0, float cull = 170.f) {
  Model* m = as.prop(name);
  if (!m) return;
  RNG rng(seed);
  for (int i = 0; i < count; i++) {
    float a = rng.range(0, 2 * PI_F);
    float r = rng.range(minR, maxR);
    float x = std::cos(a) * r, z = std::sin(a) * r;
    if (std::fabs(x) > radius || std::fabs(z) > radius) continue;
    if (avoidR > 0 && std::hypot(x - avoidX, z - avoidZ) < avoidR) continue;
    if (std::fabs(x) < 2.4f && z > 0 && z < 40) continue;   // keep the road clear
    float s = rng.range(sMin, sMax);
    float rot = rng.range(0, 2 * PI_F);
    V3 p(x, w.groundY(x, z), z);
    w.addProp(m, Mat4::translate(p) * Mat4::rotY(rot) * Mat4::scale(V3(s, s, s)), cull);
    if (solid) w.addCollider(x, z, solidR * s * 1.6f, 3.f);
  }
}

// ============================================================ ECHOES OF THE SUNKEN SPIRE
static float spireGroundY(float x, float z) {
  float h = std::sin(x * 0.035f) * std::cos(z * 0.03f) * 1.4f;
  h += std::sin(x * 0.011f + 1.7f) * 2.0f + std::cos(z * 0.013f) * 1.6f;
  // the valley floor is flat around the green, the spire sits on a plateau
  float dSpire = std::hypot(x, z + 84.f);
  if (dSpire < 40.f) h = 0.f;
  return h * 0.55f;
}

class SunkenSpire : public Game {
 public:
  int echoes = 0;
  bool bellRung = false;
  bool gateOpen = false;
  bool wardenSpawned = false;
  bool sketched[6] = {false, false, false, false, false, false};
  // landmark positions (x,z)
  float lmX[6] = {0.f, -26.f, -54.f, -66.f, -112.f, 104.f};
  float lmZ[6] = {72.f, 58.f, 96.f, 6.f, -24.f, -30.f};
  const char* lmName[6] = {"Vaeloria Green", "Miller's Crossing", "Shrine of First Light",
                           "Canopy Gate", "The Heartwood", "The Mirror Vault"};
  V3 echoPos[3] = {V3(-54, 0, 96), V3(-112, 0, -24), V3(104, 0, -30)};
  bool echoGot[3] = {false, false, false};
  const char* echoName[3] = {"Echo of the First Lesson", "Echo of the Growers", "Echo of the Architects"};
  // spire pieces
  V3 gatePos{0, 0, 33.5f};
  V3 bellPos{0, 0, -30.f};
  V3 arenaCentre{0, 34.8f, -84.f};
  bool onApex = false;

  const char* title() const override { return "Echoes of the Sunken Spire"; }
  const char* subtitle() const override { return "A tale of Vaeloria - native edition"; }
  const char* winText() const override { return "THE SPIRE REMEMBERS"; }

  void buildContent() override {
    world.name = "Vaeloria";
    world.bound = 148.f;
    world.skyTop = V3(0.30f, 0.53f, 0.82f);
    world.skyBot = V3(0.74f, 0.85f, 0.94f);
    world.fogColor = V3(0.72f, 0.83f, 0.93f);
    world.fogDensity = 0.0035f;
    world.sunDir = V3(-0.45f, 0.9f, 0.35f);
    world.sunColor = V3(1.0f, 0.96f, 0.86f);
    world.sunPower = 1.05f;
    world.ambient = V3(0.44f, 0.48f, 0.56f);
    world.spawn = V3(0, 0, 96);
    world.groundYFn = spireGroundY;

    // ---------------- clips ----------------
    assets.loadClipPack("anim/Rig_Medium_General.glb");
    assets.loadClipPack("anim/Rig_Medium_MovementBasic.glb");
    assets.loadClipPack("anim/Rig_Medium_MovementAdvanced.glb");
    assets.loadClipPack("anim/Rig_Medium_CombatMelee.glb");
    assets.loadClipPack("anim/Rig_Medium_CombatRanged.glb");
    assets.loadClipPack("anim/Rig_Medium_Special.glb");

    // ---------------- Kaelen ----------------
    // The player's path is chosen on the class picker (or --class). Default is
    // the Duelist: the original rapier-and-buckler cartographer.
    player.radius = 0.55f;
    applyClass(player.classIndex);
    player.glideUnlocked = false;
    player.pos = world.spawn;
    player.pos.y = world.groundY(player.pos.x, player.pos.z);

    // ---------------- ground ----------------
    world.buildTerrain(64, 300.f, V3(0.42f, 0.62f, 0.30f), V3(0.72f, 0.70f, 0.52f), 3);

    // ---------------- Sunlit Valley greenery ----------------
    scatterProps(world, assets, "Tree_1_A_Color1", 40, 148, 20, 140, 1.4f, 2.4f, 101, true, 0.45f, 0, -84, 34);
    scatterProps(world, assets, "Tree_2_A_Color1", 34, 148, 24, 140, 1.3f, 2.2f, 102, true, 0.4f, 0, -84, 34);
    scatterProps(world, assets, "Tree_3_A_Color1", 30, 148, 30, 140, 1.2f, 2.0f, 103, true, 0.4f, 0, -84, 34);
    scatterProps(world, assets, "Rock_1_A_Color1", 26, 148, 14, 140, 1.0f, 2.4f, 104, true, 0.6f, 0, -84, 30);
    scatterProps(world, assets, "Bush_1_A_Color1", 40, 148, 12, 140, 1.0f, 1.8f, 105, false, 0, 0, -84, 28);
    scatterProps(world, assets, "Grass_1_A_Color1", 60, 148, 10, 140, 1.0f, 1.8f, 106, false, 0, 0, -84, 28);
    scatterProps(world, assets, "Rock_3_A_Color1", 22, 148, 90, 140, 1.2f, 2.2f, 107, false, 0, 0, -84, 34);

    // ---------------- Whispering Canopy: the Long Fall ----------------
    // standing pillars + canopy trees around the crevasse at (-96, -4)
    if (Model* trunk = assets.prop("Tree_4_A_Color1")) {
      for (int i = 0; i < 14; i++) {
        float a = i / 14.f * 2 * PI_F;
        float x = -96.f + std::cos(a) * 26.f;
        float z = -4.f + std::sin(a) * 34.f;
        world.addProp(trunk, Mat4::translate(V3(x, world.groundY(x, z), z)) * Mat4::rotY(a) * Mat4::scale(V3(2.6f, 2.6f, 2.6f)), 170.f);
        world.addCollider(x, z, 1.2f, 14.f);
      }
    }

    // ---------------- Glass Sinks: crystal field ----------------
    if (Model* shard = assets.prop("column")) {
      for (int i = 0; i < 26; i++) {
        float a = i / 26.f * 2 * PI_F * 1.7f;
        float r = 14.f + (i % 7) * 5.5f;
        float x = 96.f + std::cos(a) * r, z = -4.f + std::sin(a) * r;
        world.addProp(shard, Mat4::translate(V3(x, world.groundY(x, z), z)) * Mat4::rotY(a) *
                                 Mat4::scale(V3(0.8f, 3.4f + (i % 4), 0.8f)), 170.f);
      }
    }

    // ---------------- the Spire: base, gate, bell, stair, apex ----------------
    buildSpire();

    // ---------------- landmarks (sketchable) ----------------
    // (drawn as small stone markers so they are visible in 3D)
    if (Model* marker = assets.prop("arch")) {
      for (int i = 0; i < 6; i++) {
        V3 p(lmX[i], world.groundY(lmX[i], lmZ[i]), lmZ[i]);
        world.addProp(marker, Mat4::translate(p) * Mat4::rotY(i * 1.1f) * Mat4::scale(V3(0.9f, 0.9f, 0.9f)), 120.f);
      }
    }

    // ---------------- enemies ----------------
    struct Sp { const char* t; float x, z; };
    static const Sp spawns[] = {
      {"wisp", 18, 74}, {"wisp", -14, 84}, {"sentinel", 34, 58},
      {"stalker", -74, 18}, {"stalker", -84, -18}, {"wisp", -60, -22},
      {"sentinel", -104, -38}, {"acolyte", -96, 26},
      {"stalker", 66, 32}, {"sentinel", 84, 8}, {"acolyte", 92, -18}, {"wisp", 58, -12},
      {"sentinel", 6, -12}, {"acolyte", -8, -44}, {"stalker", 12, -50},
      {nullptr, 0, 0}};
    for (const Sp* s = spawns; s->t; s++) spawnEnemy(s->t, s->x, s->z);

    setObjective("Ring the resonance bell", "North along the Warden's Road");
    addToasts("VAELORIA - ACT I", 3.2f);
  }

  void buildSpire() {
    const float arenaY = 34.f;
    // base terrace
    if (Model* base = assets.prop("floor_tile_large")) {
      for (int ring = 0; ring < 3; ring++) {
        float r = 26.f - ring * 5.f;
        int n = 14 - ring * 3;
        for (int i = 0; i < n; i++) {
          float a = i / (float)n * 2 * PI_F + ring;
          float x = std::cos(a) * r, z = -84.f + std::sin(a) * r;
          world.addProp(base, Mat4::translate(V3(x, ring * 0.5f, z)) * Mat4::rotY(a) * Mat4::scale(V3(1.4f, 1.f, 1.4f)), 170.f);
          world.platforms.push_back({x, z, 8.4f, 8.4f, ring * 0.5f + 1.0f, 0, false, true});
        }
      }
    }
    // the gate
    if (Model* arch = assets.prop("arch_gate")) {
      world.addProp(arch, Mat4::translate(V3(0, 0, gatePos.z)) * Mat4::rotY(0) * Mat4::scale(V3(2.2f, 2.2f, 2.2f)), 240.f);
    }
    if (!gateOpen) {
      world.addCollider(0, gatePos.z, 4.6f, 9.f);
    }
    // the resonance bell (a stone column with a lamp on top)
    if (Model* bell = assets.prop("column")) {
      world.addProp(bell, Mat4::translate(V3(bellPos.x, 0, bellPos.z)) * Mat4::scale(V3(1.6f, 1.6f, 1.6f)), 170.f);
      world.addCollider(bellPos.x, bellPos.z, 1.6f, 8.f);
    }
    // the spiral stair: 68 steps of 0.5 m
    if (Model* step = assets.prop("floor_tile_small")) {
      const int perTurn = 44;
      const float rise = 0.5f;
      const float r = 16.5f;
      const int total = (int)((arenaY + 0.8f) / rise);
      for (int i = 0; i < total; i++) {
        float a = (i / (float)perTurn) * 2 * PI_F;
        float y = rise * (i + 1);
        float x = std::cos(a) * r, z = -84.f + std::sin(a) * r;
        world.addProp(step, Mat4::translate(V3(x, y, z)) * Mat4::rotY(-a) * Mat4::scale(V3(1.15f, 1.f, 1.15f)), 240.f);
        world.platforms.push_back({x, z, 4.6f, 4.6f, y + 0.25f, 0, false, true});
      }
    }
    // apex terrace
    if (Model* apex = assets.prop("floor_tile_large")) {
      const int n = 16;
      for (int i = 0; i < n; i++) {
        float a = i / (float)n * 2 * PI_F;
        float x = std::cos(a) * 12.f, z = -84.f + std::sin(a) * 12.f;
        world.addProp(apex, Mat4::translate(V3(x, arenaY, z)) * Mat4::rotY(a) * Mat4::scale(V3(2.4f, 1.6f, 2.4f)), 260.f);
      }
      // fill the middle of the ring so the arena reads as a solid terrace
      world.addProp(apex, Mat4::translate(V3(0, arenaY - 0.4f, -84.f)) * Mat4::scale(V3(5.2f, 1.2f, 5.2f)), 260.f);
      world.addProp(apex, Mat4::translate(V3(7, arenaY - 0.4f, -84.f)) * Mat4::scale(V3(3.4f, 1.2f, 5.2f)), 260.f);
      world.addProp(apex, Mat4::translate(V3(-7, arenaY - 0.4f, -84.f)) * Mat4::scale(V3(3.4f, 1.2f, 5.2f)), 260.f);
      world.addProp(apex, Mat4::translate(V3(0, arenaY - 0.4f, -77.f)) * Mat4::scale(V3(5.2f, 1.2f, 3.4f)), 260.f);
      world.addProp(apex, Mat4::translate(V3(0, arenaY - 0.4f, -91.f)) * Mat4::scale(V3(5.2f, 1.2f, 3.4f)), 260.f);
      world.platforms.push_back({0, -84.f, 34.f, 34.f, arenaY + 0.8f, 20.f, false, true});
    }
  }

  std::string interactPrompt() override {
    if (dialogue.active) return "";
    // landmarks
    for (int i = 0; i < 6; i++) {
      if (!sketched[i] && distXZ(player.pos, V3(lmX[i], 0, lmZ[i])) < 12.f) {
        static char buf[96];
        std::snprintf(buf, sizeof(buf), "Sketch %s", lmName[i]);
        return buf;
      }
    }
    if (!bellRung && distXZ(player.pos, bellPos) < 6.5f) return "Ring the resonance bell";
    for (int i = 0; i < 3; i++) {
      if (!echoGot[i] && distXZ(player.pos, echoPos[i]) < 6.f) {
        static char buf[96];
        std::snprintf(buf, sizeof(buf), "Take %s", echoName[i]);
        return buf;
      }
    }
    if (!gateOpen && distXZ(player.pos, gatePos) < 7.f) return "Sealed - three echoes required";
    return "";
  }

  void onInteract() override {
    // sketches first
    for (int i = 0; i < 6; i++) {
      if (!sketched[i] && distXZ(player.pos, V3(lmX[i], 0, lmZ[i])) < 12.f) {
        sketched[i] = true;
        int n = 0;
        for (bool s : sketched) if (s) n++;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "SKETCHED - %s  (%d/6)", lmName[i], n);
        addToasts(buf);
        return;
      }
    }
    if (!bellRung && distXZ(player.pos, bellPos) < 6.5f) {
      bellRung = true;
      addToasts("THE BELL ANSWERS - THREE SEALS HOLD THE GATE", 3.4f);
      setObjective("Recover the three echoes", "Valley - Canopy - Glass Sinks");
      say("Lyra", {
        "Three notes, three seals. The gate wants memories, not keys.",
        "The valley shrine south-west, the Heartwood across the Long Fall, and the Mirror Vault out in the Sinks.",
        "Bring them here and the gate will remember how to open."
      });
      return;
    }
    for (int i = 0; i < 3; i++) {
      if (!echoGot[i] && distXZ(player.pos, echoPos[i]) < 6.f) {
        echoGot[i] = true;
        echoes++;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "ECHO RECOVERED - %d / 3", echoes);
        addToasts(buf);
        if (echoes == 1 && !player.glideUnlocked) {
          player.glideUnlocked = true;
          addToasts("LYRA AWAKENS - WIND GLIDE (HOLD SPACE)", 3.6f);
        }
        if (echoes >= 3) {
          gateOpen = true;
          for (size_t c = 0; c < world.colliders.size(); c++) {
            if (std::fabs(world.colliders[c].z - gatePos.z) < 0.5f && std::fabs(world.colliders[c].x) < 0.5f) {
              world.colliders.erase(world.colliders.begin() + c);
              break;
            }
          }
          addToasts("THE GATE REMEMBERS - THREE SEALS BROKEN", 3.4f);
          setObjective("Climb the Aethel-Spire", "Walk the spiral stair");
          say("Lyra", {"That is the sound of a door remembering it is a door. Go on then.",
                       "The Spire is still sinking, so I would climb briskly."});
        } else {
          setObjective("Recover the three echoes", "Two remain");
        }
        return;
      }
    }
    if (!gateOpen && distXZ(player.pos, gatePos) < 7.f) {
      say("Lyra", {"Three seals. Three echoes. I did mention this."});
    }
  }

  void onUpdate(float dt) override {
    // zone labels
    if (distXZ(player.pos, V3(-96, 0, -4)) < 60.f) zoneName = "Whispering Canopy";
    else if (distXZ(player.pos, V3(96, 0, -4)) < 60.f) zoneName = "Glass Sinks";
    else if (distXZ(player.pos, V3(0, 0, -84)) < 46.f) zoneName = "Aethel-Spire";
    else zoneName = "Sunlit Valley";

    // apex -> the Warden
    float dApex = std::hypot(player.pos.x - arenaCentre.x, player.pos.z - arenaCentre.z);
    if (!onApex && player.pos.y > 30.f && dApex < 22.f) {
      onApex = true;
      addToasts("THE APEX TERRACE", 3.f);
      say("Lyra", {"Something is standing at the core. It has been here since before the village had a name.",
                   "It is not a demon, Kaelen. It is a caretaker who ran out of ways to care."});
    }
    if (onApex && !wardenSpawned && dialogue.index >= 0) {
      wardenSpawned = true;
      spawnEnemy("warden", 0, -92);
      boss = &enemies.back();
      boss->pos.y = arenaCentre.y - 0.4f;
      boss->home = boss->pos;
      bossActive = true;
      setObjective("Face the Pale Warden", "Break its grip on the frozen hour");
      addToasts("THE PALE WARDEN", 3.4f);
    }
    if (boss && boss->dead && !won) {
      won = true;
      setObjective("Bind your vitality to the Spire", "Vaeloria is remembering");
      say("Lyra", {"It has not fallen. It has stopped. That is different, and better.",
                   "Put the echoes down, cartographer. Draw the last thing you have to draw."});
    }
  }

  void drawHudExtra() override {
    float ui = (float)R.H / 360.f;
    auto S = [&](float v) { return (int)(v * ui); };
    // echo diamonds
    for (int i = 0; i < 3; i++) {
      int x = R.W - S(30) - i * S(16), y = S(16);
      uint32_t c = echoGot[i] ? rgba32(143, 224, 255) : rgba32(40, 50, 60, 200);
      R.rect(x, y, S(7), S(7), c);
    }
    R.text(R.W - S(104), S(30), "ECHOES", rgba32(232, 195, 122), S(1));
  }
};

// ============================================================ factory
Game* createSunkenSpire() { return new SunkenSpire(); }
