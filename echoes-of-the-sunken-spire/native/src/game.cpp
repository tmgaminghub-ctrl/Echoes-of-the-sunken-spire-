// ---------------------------------------------------------------------------
//  game.cpp — shared systems: assets, skeletal animation, player, enemies,
//             camera, rasterised HUD
// ---------------------------------------------------------------------------
#include "game.h"
#include "font_data.h"
#include <cstdio>

// ============================================================ enemy table
static const EnemyCfg kEnemies[] = {
  // Hollowmere
  {"minion", "enemies/Skeleton_Minion.glb", "weapons/Skeleton_Blade.gltf", nullptr,
   46, 3.7f, 8, 2.1f, 0.42f, 0.55f, 1.5f, 15.f, 0.95f, false, false, "Melee_Unarmed_Attack_Punch_A"},
  {"warrior", "enemies/Skeleton_Warrior.glb", "weapons/Skeleton_Axe.gltf", "weapons/Skeleton_Shield_Small_A.gltf",
   112, 2.95f, 15, 2.6f, 0.6f, 0.75f, 2.2f, 16.f, 1.02f, false, false, "Melee_1H_Attack_Chop"},
  {"rogue", "enemies/Skeleton_Rogue.glb", "weapons/Skeleton_Blade.gltf", nullptr,
   68, 4.7f, 11, 2.2f, 0.3f, 0.35f, 1.1f, 17.f, 0.98f, false, false, "Melee_Dualwield_Attack_Stab"},
  {"mage", "enemies/Skeleton_Mage.glb", "weapons/Skeleton_Staff.gltf", nullptr,
   74, 2.3f, 14, 16.f, 0.85f, 0.6f, 2.6f, 20.f, 1.0f, true, false, "Ranged_Magic_Spellcasting"},
  {"boss", "enemies/Skeleton_Warrior.glb", "weapons/Skeleton_Axe.gltf", "weapons/Skeleton_Shield_Large_A.gltf",
   1100, 3.35f, 26, 3.6f, 0.7f, 0.6f, 1.9f, 40.f, 1.85f, false, true, "Melee_2H_Attack_Slice"},

  // Echoes of the Sunken Spire
  {"wisp", "enemies/Skeleton_Minion.glb", "weapons/Skeleton_Blade.gltf", nullptr,
   40, 3.9f, 7, 2.0f, 0.4f, 0.5f, 1.4f, 15.f, 0.9f, false, false, "Melee_Unarmed_Attack_Punch_A"},
  {"sentinel", "enemies/Skeleton_Warrior.glb", "weapons/Skeleton_Axe.gltf", "weapons/Skeleton_Shield_Small_A.gltf",
   120, 2.9f, 16, 2.7f, 0.6f, 0.75f, 2.1f, 18.f, 1.04f, false, false, "Melee_1H_Attack_Chop"},
  {"stalker", "enemies/Skeleton_Rogue.glb", "weapons/Skeleton_Blade.gltf", nullptr,
   70, 4.9f, 12, 2.2f, 0.28f, 0.32f, 1.0f, 20.f, 0.98f, false, false, "Melee_Dualwield_Attack_Stab"},
  {"acolyte", "enemies/Skeleton_Mage.glb", "weapons/Skeleton_Staff.gltf", nullptr,
   80, 2.2f, 15, 16.f, 0.8f, 0.6f, 2.5f, 22.f, 1.0f, true, false, "Ranged_Magic_Spellcasting"},
  {"warden", "enemies/Skeleton_Warrior.glb", "weapons/Skeleton_Axe.gltf", "weapons/Skeleton_Shield_Large_A.gltf",
   1500, 3.2f, 28, 3.9f, 0.72f, 0.62f, 1.8f, 60.f, 2.05f, false, true, "Melee_2H_Attack_Slice"},
  {nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 1, false, false, nullptr}
};

const EnemyCfg* Game::enemyCfg(const char* name) {
  for (const EnemyCfg* c = kEnemies; c->name; c++)
    if (std::strcmp(c->name, name) == 0) return c;
  return &kEnemies[0];
}

// ============================================================ assets
Model* Assets::load(const std::string& rel) {
  auto it = cache.find(rel);
  if (it != cache.end()) return it->second;
  std::string full = root + "/" + rel;
  Model* m = new Model(loadModel(full));
  cache[rel] = m;
  return m;
}
Model* Assets::tryLoad(const std::string& rel) {
  auto it = cache.find(rel);
  if (it != cache.end()) return it->second;
  std::string full = root + "/" + rel;
  if (!fileExists(full)) return nullptr;
  Model* m = new Model(loadModel(full));
  cache[rel] = m;
  if (m->nodes.empty() && m->meshes.empty()) return m;
  return m;
}

static const char* kPropFolders[] = {
  "props/", "weapons/", "nature/", "dungeon/", "wargear/", "" };

Model* Assets::prop(const char* name) {
  std::string base(name);
  static const char* exts[] = {".gltf", ".glb"};
  for (const char* folder : kPropFolders) {
    for (const char* ext : exts) {
      std::string rel = std::string(folder) + base + ext;
      auto it = cache.find(rel);
      if (it != cache.end()) return it->second;
      if (fileExists(root + "/" + rel)) return load(rel);
    }
  }
  return nullptr;
}

void Assets::loadClipPack(const char* rel) {
  Model* m = tryLoad(rel);
  if (!m) return;
  for (auto& c : m->clips) clips[c.name] = &c;
}

Assets::~Assets() {
  for (auto& kv : cache) delete kv.second;
  cache.clear();
}

// ============================================================ animator
void Animator::bind(const Model* m) {
  model = m;
  if (!m) return;
  size_t n = m->nodes.size();
  world.assign(n, Mat4());
  rot.assign(n, Quat());
  rotPrev.assign(n, Quat());
  trans.assign(n, V3());
  transPrev.assign(n, V3());
  scl.assign(n, V3(1, 1, 1));
  sclPrev.assign(n, V3(1, 1, 1));
  order.clear();
  // breadth-first from roots so parents always come first
  std::vector<int> stack = m->roots;
  while (!stack.empty()) {
    int i = stack.back();
    stack.pop_back();
    order.push_back(i);
    for (int c : m->nodes[i].children) stack.push_back(c);
  }
  if (order.size() != n) {  // orphan safety
    for (size_t i = 0; i < n; i++) {
      bool found = false;
      for (int o : order) if ((size_t)o == i) { found = true; break; }
      if (!found) order.push_back((int)i);
    }
  }
  for (size_t i = 0; i < n; i++) {
    rot[i] = m->nodes[i].r;
    trans[i] = m->nodes[i].t;
    scl[i] = m->nodes[i].s;
    rotPrev[i] = rot[i];
    transPrev[i] = trans[i];
    sclPrev[i] = scl[i];
  }
  bindCache.clear();
  jointMat.clear();
  for (auto& s : m->skins) jointMat.assign(s.joints.size(), Mat4());
  recompute:
  ;
  // build initial world matrices
  for (int i : order) {
    const Node& nd = m->nodes[i];
    Mat4 local = Mat4::fromQuatPosScale(rot[i], trans[i], scl[i]);
    world[i] = nd.parent >= 0 ? world[nd.parent] * local : local;
  }
}

void Animator::play(const Clip* c, float fadeDurIn, bool loopIn, float speedIn) {
  if (!c) return;
  if (c == cur) { loop = loopIn; speed = speedIn; return; }
  prev = cur;
  tPrev = tCur;
  rotPrev = rot;
  transPrev = trans;
  sclPrev = scl;
  cur = c;
  tCur = 0;
  fadeDur = std::max(0.001f, fadeDurIn);
  fade = 0;
  loop = loopIn;
  speed = speedIn;
}

void Animator::update(float dt) {
  if (!model || !cur) return;
  tCur += dt * speed;
  if (loop && cur->dur > 0.001f) {
    while (tCur > cur->dur) tCur -= cur->dur;
  } else if (tCur > cur->dur) {
    tCur = cur->dur;
  }
  if (prev) {
    tPrev += dt * speed;
    if (loop && prev->dur > 0.001f) {
      while (tPrev > prev->dur) tPrev -= prev->dur;
    }
  }

  // resolve channel targets once per clip
  auto bindFor = [&](const Clip* c) -> const std::vector<int>& {
    auto it = bindCache.find(c);
    if (it != bindCache.end()) return it->second;
    std::vector<int> idx(c->chans.size(), -1);
    for (size_t i = 0; i < c->chans.size(); i++) idx[i] = model->nodeIndex(c->chans[i].node.c_str());
    bindCache[c] = idx;
    return bindCache[c];
  };

  auto sample = [&](const Clip* c, float t, std::vector<Quat>& R, std::vector<V3>& T, std::vector<V3>& S) {
    const std::vector<int>& idx = bindFor(c);
    for (size_t ci = 0; ci < c->chans.size(); ci++) {
      int ni = idx[ci];
      if (ni < 0) continue;
      const Chan& ch = c->chans[ci];
      size_t keys = ch.times.size();
      if (!keys) continue;
      size_t nc = ch.path == 1 ? 4 : 3;
      // find key pair
      size_t k0 = 0;
      while (k0 + 1 < keys && ch.times[k0 + 1] < t) k0++;
      size_t k1 = std::min(k0 + 1, keys - 1);
      float t0 = ch.times[k0], t1 = ch.times[k1];
      float u = (t1 > t0) ? clampf((t - t0) / (t1 - t0), 0.f, 1.f) : 0.f;
      if (ch.path == 1) {
        Quat a(ch.vals[k0 * 4], ch.vals[k0 * 4 + 1], ch.vals[k0 * 4 + 2], ch.vals[k0 * 4 + 3]);
        Quat b(ch.vals[k1 * 4], ch.vals[k1 * 4 + 1], ch.vals[k1 * 4 + 2], ch.vals[k1 * 4 + 3]);
        R[ni] = qSlerp(a, b, u);
      } else if (ch.path == 0) {
        V3 a(ch.vals[k0 * 3], ch.vals[k0 * 3 + 1], ch.vals[k0 * 3 + 2]);
        V3 b(ch.vals[k1 * 3], ch.vals[k1 * 3 + 1], ch.vals[k1 * 3 + 2]);
        T[ni] = lerp(a, b, u);
      } else {
        V3 a(ch.vals[k0 * 3], ch.vals[k0 * 3 + 1], ch.vals[k0 * 3 + 2]);
        V3 b(ch.vals[k1 * 3], ch.vals[k1 * 3 + 1], ch.vals[k1 * 3 + 2]);
        S[ni] = lerp(a, b, u);
      }
      (void)nc;
    }
  };

  if (prev && fade < fadeDur) fade = std::min(fadeDur, fade + dt);
  if (prev) {
    sample(cur, tCur, rot, trans, scl);
    sample(prev, tPrev, rotPrev, transPrev, sclPrev);
    float w = fadeDur > 0.f ? clampf(fade / fadeDur, 0.f, 1.f) : 1.f;
    for (size_t i = 0; i < model->nodes.size(); i++) {
      rot[i] = qSlerp(rotPrev[i], rot[i], w);
      trans[i] = lerp(transPrev[i], trans[i], w);
      scl[i] = lerp(sclPrev[i], scl[i], w);
    }
  } else {
    for (size_t i = 0; i < model->nodes.size(); i++) {
      rot[i] = model->nodes[i].r;
      trans[i] = model->nodes[i].t;
      scl[i] = model->nodes[i].s;
    }
    sample(cur, tCur, rot, trans, scl);
  }

  for (int i : order) {
    const Node& nd = model->nodes[i];
    Mat4 local = Mat4::fromQuatPosScale(rot[i], trans[i], scl[i]);
    world[i] = nd.parent >= 0 ? world[nd.parent] * local : local;
  }
  // skinning matrices
  for (size_t s = 0; s < model->skins.size(); s++) {
    const Skin& sk = model->skins[s];
    if (jointMat.size() < sk.joints.size()) jointMat.resize(sk.joints.size());
    for (size_t j = 0; j < sk.joints.size(); j++) {
      int node = sk.joints[j];
      jointMat[j] = (node >= 0 && (size_t)node < world.size()) ? world[node] * sk.invBind[j] : Mat4();
    }
  }
}

Mat4 Animator::nodeWorld(const char* name) const {
  if (!model) return Mat4();
  int i = model->nodeIndex(name);
  if (i < 0 || (size_t)i >= world.size()) return Mat4();
  return world[i];
}

// ============================================================ game: init
void Game::init(const std::string& dataRoot, int rw, int rh, bool headless) {
  (void)headless;
  R.resize(rw, rh);
  assets.root = dataRoot;
  camPos = V3(0, 6, 20);
}

void Game::setObjective(const char* main, const char* sub) {
  objective = main;
  objectiveSub = sub;
}

void Game::addToasts(const char* text, float secs) {
  toasts.push_back({text, secs});
  if (toasts.size() > 4) toasts.erase(toasts.begin());
}

void Game::say(const char* speaker, std::vector<std::string> lines) {
  dialogue.speaker = speaker;
  dialogue.lines = std::move(lines);
  dialogue.index = 0;
  dialogue.active = !dialogue.lines.empty();
}

void Game::advanceDialogue() {
  if (!dialogue.active) return;
  dialogue.index++;
  if (dialogue.index >= (int)dialogue.lines.size()) dialogue.active = false;
}

void Game::beginPlay() {
  titleScreen = false;
  player.pos = world.spawn;
  player.facing = PI_F;
  player.hp = player.maxHp;
  player.state = ActorState::Idle;
  camYaw = 0.f;
  camPos = player.pos + V3(std::sin(camYaw) * camDist, 4.f, std::cos(camYaw) * camDist);
  if (player.anim.model) player.anim.play(assets.clipAny({"Idle_A", "Skeletons_Idle"}), 0.1f, true);
}

void Game::startTitle() { titleScreen = true; }

// ============================================================ gear
WeaponDef kRapier{"Heirloom Iron Rapier", "wargear/sword_D.gltf", 1.05f, 2.5f, 1.15f, V3(2.876f, 0.f, 1.84f), 1.f, 0.72f};
WeaponDef kLongsword{"Hollowmere Longsword", "weapons/sword_1handed.gltf", 1.0f, 2.4f, 1.0f, V3(2.876f, 0.f, 1.84f), 1.f, 0.75f};

void Game::equipWeapon(const WeaponDef& def) {
  Model* m = assets.prop(def.modelPath);
  if (!m) {
    // fall back to any sword we can find
    m = assets.prop("sword_1handed");
  }
  player.weaponModel = m;
  player.weaponGrip = def.gripEuler;
  player.weaponScale = def.scale;
  player.weaponName = def.name;
}

// ============================================================ terrain
void World::buildTerrain(int seg, float size, const V3& colorA, const V3& colorB, uint32_t seed) {
  terrain = new Model();
  Mesh m;
  RNG rng(seed);
  float half = size * 0.5f;
  float step = size / (float)seg;
  for (int zi = 0; zi <= seg; zi++) {
    for (int xi = 0; xi <= seg; xi++) {
      float x = -half + xi * step;
      float z = -half + zi * step;
      float y = groundY(x, z);
      m.pos.push_back(V3(x, y, z));
      // analytic-ish normal from finite differences
      float e = 1.0f;
      float hL = groundY(x - e, z), hR = groundY(x + e, z);
      float hD = groundY(x, z - e), hU = groundY(x, z + e);
      V3 n = normalize(V3(hL - hR, 2.0f * e, hD - hU));
      m.nrm.push_back(n);
      m.uv.push_back(V2(x * 0.05f, z * 0.05f));
      // patchy colour blend so the ground is not a flat wash
      float t = 0.5f + 0.5f * std::sin(x * 0.09f) * std::cos(z * 0.07f);
      float n2 = rng.range(0.86f, 1.08f);
      V3 c = lerp(colorA, colorB, clampf(t, 0, 1)) * n2;
      m.col.push_back(c);
      m.bounds.add(m.pos.back());
    }
  }
  for (int zi = 0; zi < seg; zi++) {
    for (int xi = 0; xi < seg; xi++) {
      uint32_t a = (uint32_t)(zi * (seg + 1) + xi);
      uint32_t b = a + 1;
      uint32_t c = (uint32_t)((zi + 1) * (seg + 1) + xi);
      uint32_t d = c + 1;
      // counter-clockwise when seen from above
      m.idx.push_back(a); m.idx.push_back(c); m.idx.push_back(b);
      m.idx.push_back(b); m.idx.push_back(c); m.idx.push_back(d);
    }
  }
  terrain->meshes.push_back(std::move(m));
  Material mat;
  mat.color = V3(1, 1, 1);
  mat.roughness = 1.f;
  terrain->mats.push_back(mat);
  terrain->meshes.back().mat = 0;
  terrain->finalize();
  props.push_back({terrain, Mat4(), 1e9f});
}

// ============================================================ collision
void Game::resolveStatic(V3& pos, float radius) {
  for (auto& c : world.colliders) {
    if (pos.y > (c.h > 0 ? c.h : 2.2f)) continue;
    float dx = pos.x - c.x, dz = pos.z - c.z;
    float minD = radius + c.r;
    if (std::fabs(dx) > minD || std::fabs(dz) > minD) continue;
    float d2 = dx * dx + dz * dz;
    if (d2 >= minD * minD) continue;
    float d = std::sqrt(std::max(1e-6f, d2));
    float push = minD - d;
    pos.x += dx / d * push;
    pos.z += dz / d * push;
  }
  float b = world.bound - radius;
  pos.x = clampf(pos.x, -b, b);
  pos.z = clampf(pos.z, -b, b);
}

bool Game::groundSupport(float& outY, bool& onPlat, float x, float z, float currentY, float vy) {
  float support = world.groundY(x, z);
  bool plat = false;
  for (auto& p : world.platforms) {
    if (p.ghost && !p.active) continue;
    bool inside;
    if (p.roundR > 0) inside = (std::hypot(x - p.x, z - p.z) <= p.roundR);
    else inside = (std::fabs(x - p.x) <= p.w * 0.5f && std::fabs(z - p.z) <= p.d * 0.5f);
    if (!inside) continue;
    if (vy <= 0.001f && currentY >= p.top - 0.75f && p.top > support) {
      support = p.top;
      plat = true;
    }
  }
  outY = support;
  onPlat = plat;
  return plat;
}

void Game::resolveBodies() {
  if (player.dead) return;
  for (int pass = 0; pass < 2; pass++) {
    for (auto& e : enemies) {
      if (e.dead) continue;
      if (std::fabs(e.pos.y - player.pos.y) > 2.4f) continue;
      float dx = player.pos.x - e.pos.x, dz = player.pos.z - e.pos.z;
      float minD = player.radius + e.radius;
      float d2 = dx * dx + dz * dz;
      if (d2 >= minD * minD) continue;
      if (d2 < 1e-6f) { dx = std::sin(player.facing); dz = std::cos(player.facing); d2 = 1; }
      float d = std::sqrt(d2);
      float overlap = minD - d;
      if (overlap <= 0.001f) continue;
      float nx = dx / d, nz = dz / d;
      float playerInto = -(player.vel.x * nx + player.vel.z * nz);
      float enemyInto = (e.eVelX * nx + e.eVelZ * nz);
      float share;
      if (playerInto > 0.05f && enemyInto <= 0.05f) share = 1.f;
      else if (enemyInto > 0.05f && playerInto <= 0.05f) share = 0.f;
      else if (enemyInto > 0.05f && playerInto > 0.05f) share = 0.5f;
      else share = 0.6f;
      if (e.isBoss()) share = std::min(1.f, share + 0.25f);
      player.pos.x += nx * overlap * share;
      player.pos.z += nz * overlap * share;
      if (share < 1.f) {
        e.pos.x -= nx * overlap * (1 - share);
        e.pos.z -= nz * overlap * (1 - share);
        resolveStatic(e.pos, e.radius);
      }
      float into = player.vel.x * nx + player.vel.z * nz;
      if (into < 0) { player.vel.x -= nx * into; player.vel.z -= nz * into; }
      resolveStatic(player.pos, player.radius);
    }
  }
}

void Game::meleeArc(const V3& origin, const V3& dir, float range, float cosLimit, float damage, bool crit) {
  for (auto& e : enemies) {
    if (e.dead) continue;
    if (std::fabs(e.pos.y - origin.y) > 3.f) continue;
    V3 to = e.pos - origin;
    to.y = 0;
    float d = length(to);
    if (d > range + e.radius) continue;
    if (d > 0.7f) {
      V3 nd = normalize(to);
      if (dot(nd, dir) < cosLimit) continue;
    }
    e.hp -= damage * (crit ? 1.6f : 1.f);
    e.flashTint = 0.18f;
    e.hurtT = 0.28f;
    if (e.hp <= 0) { e.hp = 0; e.dead = true; e.deathT = 0; }
    hitstop = std::max(hitstop, crit ? 0.075f : 0.05f);
    shakeAmt = std::max(shakeAmt, 0.35f);
  }
}

// ============================================================ player
void Game::updatePlayer(float dt) {
  Player& p = player;
  if (p.dead) {
    p.anim.update(dt);
    return;
  }
  p.invuln = std::max(0.f, p.invuln - dt);
  p.attackCooldown = std::max(0.f, p.attackCooldown - dt);
  p.comboWindow = std::max(0.f, p.comboWindow - dt);
  p.stamina = std::min(p.maxStamina, p.stamina + 14.f * dt);

  bool dlg = dialogue.active;
  V3 move(0, 0, 0);
  if (!dlg && !paused) {
    float f = (in.held(KEY_W) ? 1.f : 0.f) - (in.held(KEY_S) ? 1.f : 0.f);
    float s = (in.held(KEY_D) ? 1.f : 0.f) - (in.held(KEY_A) ? 1.f : 0.f);
    V3 camFwd(-std::sin(camYaw), 0, -std::cos(camYaw));
    V3 camRight(std::cos(camYaw), 0, -std::sin(camYaw));
    move = camFwd * f + camRight * s;
    if (length(move) > 1.f) move = normalize(move);
  }

  // ---- dodge ----
  if (p.state == ActorState::Dodge) {
    p.dodgeT += dt;
    p.pos += p.dodgeDir * (12.5f * dt);
    if (p.dodgeT > p.dodgeDur) p.state = ActorState::Idle;
  }

  // ---- attack ----
  if (p.state == ActorState::Attack) {
    p.attackT += dt;
    float prog = p.attackDur > 0.f ? p.attackT / p.attackDur : 1.f;
    if (!p.hitApplied && prog >= 0.42f) {
      p.hitApplied = true;
      V3 fwd(std::sin(p.facing), 0, std::cos(p.facing));
      meleeArc(p.pos + V3(0, 1.0f, 0), fwd, 2.6f, 0.35f, 16.f * (p.combo >= 2 ? 1.5f : 1.f), p.combo >= 2);
      // lunge
      p.vel += fwd * 3.0f;
    }
    if (prog >= 1.f) p.state = ActorState::Idle;
  }

  // ---- vertical ----
  if (p.grounded) p.coyote = 0.14f;
  else p.coyote = std::max(0.f, p.coyote - dt);
  if (in.hit(KEY_SPACE) && !dlg && !paused) p.jumpBuffer = 0.16f;
  else p.jumpBuffer = std::max(0.f, p.jumpBuffer - dt);
  if (p.jumpBuffer > 0 && !p.dead) {
    bool groundJump = p.coyote > 0;
    bool airJump = !groundJump && p.jumpsLeft > 0 && p.glideUnlocked;
    if (groundJump || airJump) {
      p.vy = airJump ? 8.0f : 8.6f;
      p.grounded = false;
      p.coyote = 0;
      p.jumpBuffer = 0;
      if (airJump) p.jumpsLeft--;
    }
  }
  p.gliding = false;
  if (p.glideUnlocked && !p.grounded && p.vy < 0 && in.held(KEY_SPACE) && p.airTime > 0.28f) {
    p.vy = std::max(p.vy, -2.6f);
    p.gliding = true;
  }
  if (!p.grounded) {
    p.vy -= 26.f * dt;
    p.airTime += dt;
  } else {
    p.airTime = 0;
  }
  p.pos.y += p.vy * dt;

  float support; bool onPlat;
  groundSupport(support, onPlat, p.pos.x, p.pos.z, p.pos.y, p.vy);
  if (p.pos.y <= support + 0.001f) {
    p.pos.y = support;
    p.vy = 0;
    p.grounded = true;
    p.jumpsLeft = p.glideUnlocked ? 1 : 0;
    p.onPlatform = onPlat;
    p.platformY = support;
  } else {
    p.grounded = false;
    p.onPlatform = false;
  }

  // ---- block / attack / dodge inputs ----
  p.blocking = in.mouseHeld(1) && p.state != ActorState::Attack && p.state != ActorState::Dodge && p.stamina > 4.f;
  if (p.blocking) {
    p.state = ActorState::Block;
    p.stamina -= 6.f * dt;
  } else if (p.state == ActorState::Block) {
    p.state = ActorState::Idle;
  }

  if (!dlg && !paused) {
    if (in.mouseHit(0) && (p.state == ActorState::Idle || p.state == ActorState::Move || (p.state == ActorState::Attack && p.comboWindow > 0))) {
      if (p.state == ActorState::Attack && p.comboWindow > 0) {
        p.combo = (p.combo + 1) % 3;
        p.attackT = 0;
        p.hitApplied = false;
        const char* clips[3] = {"Melee_1H_Attack_Slice_Diagonal", "Melee_1H_Attack_Slice_Horizontal", "Melee_1H_Attack_Stab"};
        const Clip* c = assets.clipAny({clips[p.combo], "Melee_1H_Attack_Chop", "Melee_Unarmed_Attack_Punch_A"});
        if (c) {
          p.anim.play(c, 0.08f, false, 1.25f);
          p.attackDur = c->dur / 1.25f;
        }
      } else {
        p.combo = 0;
        p.attackT = 0;
        p.hitApplied = false;
        p.state = ActorState::Attack;
        const Clip* c = assets.clipAny({"Melee_1H_Attack_Slice_Diagonal", "Melee_1H_Attack_Chop", "Melee_Unarmed_Attack_Punch_A"});
        if (c) {
          p.anim.play(c, 0.08f, false, 1.25f);
          p.attackDur = c->dur / 1.25f;
        } else {
          p.attackDur = 0.45f;
        }
      }
      p.comboWindow = p.attackDur * 0.55f;
    }
    if (in.hit(KEY_SPACE) && length(move) < 0.2f && p.grounded && p.stamina > 12.f) {
      p.state = ActorState::Dodge;
      p.dodgeT = 0;
      p.stamina -= 18.f;
      p.invuln = 0.34f;
      p.dodgeDir = length(move) > 0.1f ? normalize(move) : V3(-std::sin(p.facing), 0, -std::cos(p.facing));
      p.anim.play(assets.clipAny({"Dodge_Forward", "Jump_Start", "Idle_A"}), 0.06f, false, 1.4f);
    }
    if (in.hit(KEY_TAB)) {
      if (p.lockOn) { p.lockOn = false; p.lockTarget = nullptr; }
      else {
        Actor* t = nearestEnemyTarget(20.f);
        if (t) { p.lockOn = true; p.lockTarget = t; }
      }
    }
  }

  // ---- locomotion ----
  float speed = 5.6f;
  bool moving = length(move) > 0.05f;
  if (p.state == ActorState::Attack) speed *= 0.15f;
  else if (p.blocking) speed *= 0.42f;
  else if (p.state == ActorState::Dodge) speed = 0.f;
  if (in.held(KEY_SHIFT) && !p.blocking && p.state != ActorState::Attack) speed *= 1.35f;

  if (moving && p.state != ActorState::Dodge) {
    // face the movement direction
    float want = std::atan2(move.x, move.z);
    if (p.lockOn && p.lockTarget) {
      V3 to = p.lockTarget->pos - p.pos;
      want = std::atan2(to.x, to.z);
    }
    float d = want - p.facing;
    while (d > PI_F) d -= 2 * PI_F;
    while (d < -PI_F) d += 2 * PI_F;
    p.facing += d * std::min(1.f, dt * 12.f);
  }
  V3 wantVel = move * speed;
  p.vel.x = lerpf(p.vel.x, wantVel.x, std::min(1.f, dt * 10.f));
  p.vel.z = lerpf(p.vel.z, wantVel.z, std::min(1.f, dt * 10.f));
  p.pos.x += p.vel.x * dt;
  p.pos.z += p.vel.z * dt;
  p.speedSmooth = lerpf(p.speedSmooth, std::sqrt(p.vel.x * p.vel.x + p.vel.z * p.vel.z), std::min(1.f, dt * 12.f));

  resolveStatic(p.pos, p.radius);

  // ---- animation state ----
  if (p.state == ActorState::Idle || p.state == ActorState::Move || p.state == ActorState::Block) {
    if (p.blocking) {
      p.anim.play(assets.clipAny({"Melee_Blocking", "Melee_Block", "Idle_A"}), 0.12f, true);
    } else if (p.speedSmooth > 5.0f) {
      p.anim.play(assets.clipAny({"Running_A", "Walking_A"}), 0.15f, true, 1.0f + p.speedSmooth * 0.02f);
    } else if (p.speedSmooth > 0.4f) {
      p.anim.play(assets.clipAny({"Walking_A", "Running_A"}), 0.18f, true, 1.15f);
    } else if (!p.grounded) {
      p.anim.play(assets.clipAny({"Jump_Idle", "Idle_A"}), 0.12f, true);
    } else {
      p.anim.play(assets.clipAny({"Idle_A", "Skeletons_Idle"}), 0.22f, true);
    }
    p.state = p.speedSmooth > 0.4f ? ActorState::Move : ActorState::Idle;
    if (p.blocking) p.state = ActorState::Block;
  }
  p.anim.update(dt);

  // ---- camera follow ----
  bool manual = false;
  if (in.held(KEY_LEFT)) { camYaw += dt * 1.9f; manual = true; }
  if (in.held(KEY_RIGHT)) { camYaw -= dt * 1.9f; manual = true; }
  if (std::fabs(in.mouseDX) > 0.5f || std::fabs(in.mouseDY) > 0.5f) {
    camYaw -= in.mouseDX * 0.0032f;
    camPitch = clampf(camPitch + in.mouseDY * 0.0026f, 0.02f, 1.05f);
    manual = true;
  }
  // the camera drifts in behind Kaelen when he is walking and the mouse is idle
  if (manual) camIdle = 0.f;
  else camIdle += dt;
  if (camIdle > 0.6f && p.speedSmooth > 1.2f) {
    float want = p.facing + PI_F;
    float d = want - camYaw;
    while (d > PI_F) d -= 2 * PI_F;
    while (d < -PI_F) d += 2 * PI_F;
    camYaw += d * std::min(1.f, dt * 1.2f);
  }
  // lock-on framing: sit behind the shoulder looking at the target
  if (p.lockOn && p.lockTarget && !p.lockTarget->dead) {
    V3 to = p.lockTarget->pos - p.pos;
    float want = std::atan2(to.x, to.z) + PI_F;
    float d = want - camYaw;
    while (d > PI_F) d -= 2 * PI_F;
    while (d < -PI_F) d += 2 * PI_F;
    camYaw += d * std::min(1.f, dt * 3.4f);
    camPitch += (0.30f - camPitch) * std::min(1.f, dt * 1.6f);
  }
}

Actor* Game::nearestEnemyTarget(float maxRange) {
  Actor* best = nullptr;
  float bestD = maxRange;
  for (auto& e : enemies) {
    if (e.dead) continue;
    float d = distXZ(e.pos, player.pos);
    if (d < bestD) { bestD = d; best = &e; }
  }
  return best;
}

void Game::damagePlayer(float dmg, const V3& fromDir) {
  Player& p = player;
  if (p.dead || p.invuln > 0) return;
  if (p.blocking) {
    // frontal block only
    V3 fwd(std::sin(p.facing), 0, std::cos(p.facing));
    if (dot(fwd, normalize(fromDir)) > 0.1f) {
      p.stamina = std::max(0.f, p.stamina - dmg * 1.6f);
      return;
    }
  }
  p.hp -= dmg;
  p.flashTint = 0.25f;
  shakeAmt = std::max(shakeAmt, 0.6f);
  if (p.hp <= 0) { p.hp = 0; p.dead = true; p.state = ActorState::Dead; }
}

// ============================================================ enemies
void Game::spawnEnemy(const char* cfgName, float x, float z) {
  const EnemyCfg* cfg = enemyCfg(cfgName);
  Enemy e;
  e.cfg = cfg;
  e.model = assets.tryLoad(cfg->modelPath);
  if (!e.model) return;
  e.anim.bind(e.model);
  e.pos = V3(x, world.groundY(x, z), z);
  e.home = e.pos;
  e.hp = e.maxHp = cfg->hp;
  e.radius = 0.55f * cfg->scale;
  e.facing = std::rand() % 628 / 100.f;
  e.state = ActorState::Idle;
  const Clip* idle = assets.clipAny({"Skeletons_Idle", "Idle_A"});
  if (idle) e.anim.play(idle, 0.1f, true);
  enemies.push_back(std::move(e));
}

void Game::updateEnemies(float dt) {
  for (auto& e : enemies) {
    if (e.dead) {
      e.deathT += dt;
      e.anim.update(dt);
      continue;
    }
    const EnemyCfg& c = *e.cfg;
    e.flashTint = std::max(0.f, e.flashTint - dt);
    float d = distXZ(player.pos, e.pos);
    V3 toPlayer = player.pos - e.pos;
    toPlayer.y = 0;
    if (d > 0.001f) toPlayer = normalize(toPlayer);

    if (!e.aggroed && d < c.aggro && !player.dead) {
      e.aggroed = true;
      if (e.isBoss()) { bossActive = true; }
    }
    if (player.dead) e.aggroed = false;

    if (e.hurtT > 0) {
      e.hurtT -= dt;
      e.anim.update(dt);
      continue;
    }

    if (!e.aggroed) {
      // gentle idle wander
      e.wanderT -= dt;
      if (e.wanderT <= 0) {
        e.wanderT = 2.5f + (std::rand() % 400) / 100.f;
        e.wanderDir = (std::rand() % 100 < 60) ? V3(0, 0, 0)
                                              : normalize(V3((std::rand() % 200 - 100) / 100.f, 0, (std::rand() % 200 - 100) / 100.f));
      }
      if (length(e.wanderDir) > 0.01f) {
        e.pos += e.wanderDir * (c.speed * 0.24f * dt);
        e.eVelX = e.wanderDir.x * c.speed * 0.24f;
        e.eVelZ = e.wanderDir.z * c.speed * 0.24f;
        e.facing = std::atan2(e.wanderDir.x, e.wanderDir.z);
        e.anim.play(assets.clipAny({"Skeletons_Walking", "Walking_A"}), 0.25f, true, 0.8f);
        if (distXZ(e.pos, e.home) > 6.f) e.wanderDir = normalize(e.home - e.pos);
      } else {
        e.eVelX = e.eVelZ = 0;
        e.anim.play(assets.clipAny({"Skeletons_Idle", "Idle_A"}), 0.25f, true);
      }
      e.anim.update(dt);
      continue;
    }

    // ---- engaged ----
    e.facing = std::atan2(toPlayer.x, toPlayer.z);
    float wantDist = c.ranged ? 9.5f : c.reach * 0.72f;
    e.cooldownT = std::max(0.f, e.cooldownT - dt);
    e.circleT -= dt;
    if (e.circleT <= 0) { e.circleT = 2.2f + (std::rand() % 240) / 100.f; if (std::rand() % 100 < 45) e.circleDir = -e.circleDir; }
    V3 side(-toPlayer.z * e.circleDir, 0, toPlayer.x * e.circleDir);

    V3 move(0, 0, 0);
    if (d > wantDist + 3.4f) move = toPlayer;
    else if (d > wantDist + 1.2f) move = toPlayer * 0.6f + side * 0.4f;
    else if (d < wantDist - 1.0f) move = toPlayer * -0.7f + side * 0.3f;
    else move = side * 0.55f;

    if (e.state == ActorState::Attack) {
      e.attackT += dt;
      float prog = e.attackT / std::max(0.001f, (c.windup + c.recover));
      if (!e.hitApplied && e.attackT >= c.windup) {
        e.hitApplied = true;
        if (c.ranged) {
          Projectile pr;
          pr.pos = e.pos + V3(0, 1.3f, 0) + toPlayer * 0.6f;
          pr.vel = toPlayer * 11.f;
          pr.damage = c.damage;
          pr.fromPlayer = false;
          projectiles.push_back(pr);
        } else if (distXZ(player.pos, e.pos) < c.reach + 0.6f) {
          damagePlayer(c.damage, e.pos - player.pos);
        }
      }
      if (prog >= 1.f) {
        e.state = ActorState::Idle;
        e.cooldownT = c.cooldown;
      }
      e.anim.update(dt);
      continue;
    }

    if (e.cooldownT <= 0.f && d < c.reach + (c.ranged ? 3.f : 0.6f) && d > 0.3f) {
      e.state = ActorState::Attack;
      e.attackT = 0;
      e.hitApplied = false;
      e.anim.play(assets.clipAny({c.attackClip, "Melee_1H_Attack_Chop"}), 0.08f, false, 1.0f);
    } else {
      float sp = c.speed * (e.state == ActorState::Attack ? 0.2f : 1.f);
      if (length(move) > 0.05f) {
        move = normalize(move);
        e.pos += move * sp * dt;
        e.eVelX = move.x * sp;
        e.eVelZ = move.z * sp;
        e.anim.play(assets.clipAny({"Skeletons_Walking", "Walking_A"}), 0.2f, true, 0.9f);
      } else {
        e.eVelX = e.eVelZ = 0;
        e.anim.play(assets.clipAny({"Skeletons_Idle", "Idle_A"}), 0.2f, true);
      }
    }
    resolveStatic(e.pos, e.radius);
    // enemies stand on terrain *or* on raised platforms (the apex arena, stair tops)
    {
      float sup = 0.f;
      bool onPlat = false;
      groundSupport(sup, onPlat, e.pos.x, e.pos.z, e.pos.y, -1.f);
      e.pos.y = sup;
    }
    e.anim.update(dt);
  }

  // clear out corpses after the death animation
  enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                               [](const Enemy& e) { return e.dead && e.deathT > 3.0f; }),
                enemies.end());
}

void Game::updateProjectiles(float dt) {
  for (auto& p : projectiles) {
    if (p.dead) continue;
    p.pos += p.vel * dt;
    p.life -= dt;
    if (p.life <= 0) { p.dead = true; continue; }
    if (!p.fromPlayer && distXZ(p.pos, player.pos) < 0.9f && std::fabs(p.pos.y - (player.pos.y + 1.f)) < 1.6f) {
      damagePlayer(p.damage, p.vel * -1.f);
      p.dead = true;
    }
    if (p.pos.y < world.groundY(p.pos.x, p.pos.z) - 0.2f) p.dead = true;
  }
  projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                   [](const Projectile& p) { return p.dead; }),
                    projectiles.end());
}

// ============================================================ drawing
void Game::drawModel(const Model* m, const Mat4& xform, V3 tint, float extraLight, const std::vector<Mat4>* joints) {
  if (!m) return;
  for (auto& mesh : m->meshes) {
    DrawItem d;
    d.mesh = &mesh;
    d.mat = mesh.mat >= 0 && mesh.mat < (int)m->mats.size() ? &m->mats[mesh.mat] : nullptr;
    d.tex = (d.mat && d.mat->tex >= 0 && d.mat->tex < (int)m->texs.size()) ? &m->texs[d.mat->tex] : nullptr;
    d.model = xform;
    d.tint = tint;
    d.extraLight = extraLight;
    d.joints = (mesh.skinned && joints) ? joints : nullptr;
    R.submit(d);
  }
}

void Game::drawScene() {
  R.skyTop = world.skyTop;
  R.skyBot = world.skyBot;
  R.fog = world.fogColor;
  R.fogDensity = world.fogDensity;
  R.sunDir = normalize(world.sunDir);
  R.sunColor = world.sunColor;
  R.sunPower = world.sunPower;
  R.ambient = world.ambient;

  // camera (KK_CAMYAW / KK_CAMPITCH / KK_CAMDIST force a fixed framing for screenshots)
  static const char* dbgYaw = getenv("KK_CAMYAW");
  static const char* dbgPitch = getenv("KK_CAMPITCH");
  if (dbgYaw) camYaw = (float)atof(dbgYaw);
  if (dbgPitch) camPitch = (float)atof(dbgPitch);
  V3 lookAt = player.pos + V3(0, 1.65f, 0);
  V3 dir(std::sin(camYaw) * std::cos(camPitch), std::sin(camPitch), std::cos(camYaw) * std::cos(camPitch));
  V3 desired = lookAt + dir * camDist;
  if (desired.y < lookAt.y - 1.0f) desired.y = lookAt.y - 1.0f;
  camPos = camPos + (desired - camPos) * 0.35f;
  R.cam.pos = camPos;
  R.cam.target = lookAt;
  if (getenv("KK_GEAR_DEBUG") && (R.frames % 30) == 0) {
    std::printf("[gear] %s weapon=%p shield=%p facing=%.2f camYaw=%.2f dist=%.2f title=%d\n",
                player.weaponName, (void*)player.weaponModel, (void*)player.shieldModel,
                player.facing, camYaw, camDist, (int)titleScreen);
  }
  if (getenv("KK_CAM_DEBUG")) {
    std::printf("[cam] pos=%.2f %.2f %.2f target=%.2f %.2f %.2f player=%.2f %.2f %.2f\n", camPos.x,
                camPos.y, camPos.z, lookAt.x, lookAt.y, lookAt.z, player.pos.x, player.pos.y,
                player.pos.z);
    if (player.model) {
      Mat4 x = Mat4::translate(player.pos) * Mat4::rotY(player.facing);
      for (auto& mesh : player.model->meshes) {
        V3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
        for (size_t vi = 0; vi < mesh.pos.size(); vi++) {
          V3 local = mesh.pos[vi];
          if (mesh.skinned && mesh.joints.size() > vi && mesh.weights.size() > vi &&
              vi < player.anim.jointMat.size() + 1 && !player.anim.jointMat.empty()) {
            const V4& ji = mesh.joints[vi];
            const V4& w = mesh.weights[vi];
            V3 acc(0, 0, 0);
            for (int k = 0; k < 4; k++) {
              int idx = (int)(k == 0 ? ji.x : k == 1 ? ji.y : k == 2 ? ji.z : ji.w);
              float weight = k == 0 ? w.x : k == 1 ? w.y : k == 2 ? w.z : w.w;
              if (weight <= 0.f || idx < 0 || idx >= (int)player.anim.jointMat.size()) continue;
              acc += player.anim.jointMat[idx].mulPoint(mesh.pos[vi]) * weight;
            }
            local = acc;
          }
          V3 wp = x.mulPoint(local);
          mn.x = std::min(mn.x, wp.x); mn.y = std::min(mn.y, wp.y); mn.z = std::min(mn.z, wp.z);
          mx.x = std::max(mx.x, wp.x); mx.y = std::max(mx.y, wp.y); mx.z = std::max(mx.z, wp.z);
        }
        // screen-space bbox
        float sx0 = 1e9f, sy0 = 1e9f, sx1 = -1e9f, sy1 = -1e9f;
        for (size_t vi = 0; vi < mesh.pos.size(); vi++) {
          V3 local = mesh.pos[vi];
          if (mesh.skinned && vi < mesh.joints.size() && !player.anim.jointMat.empty()) {
            const V4& ji = mesh.joints[vi];
            const V4& w = mesh.weights[vi];
            V3 acc(0, 0, 0);
            for (int k = 0; k < 4; k++) {
              int idx = (int)(k == 0 ? ji.x : k == 1 ? ji.y : k == 2 ? ji.z : ji.w);
              float weight = k == 0 ? w.x : k == 1 ? w.y : k == 2 ? w.z : w.w;
              if (weight <= 0.f || idx < 0 || idx >= (int)player.anim.jointMat.size()) continue;
              acc += player.anim.jointMat[idx].mulPoint(mesh.pos[vi]) * weight;
            }
            local = acc;
          }
          V3 wp = x.mulPoint(local);
          V4 clip = R.cam.proj.mulVec4(R.cam.view.mulVec4(V4(wp.x, wp.y, wp.z, 1.f)));
          if (clip.w <= 0.001f) continue;
          float nx = clip.x / clip.w, ny = clip.y / clip.w;
          float px = (nx * 0.5f + 0.5f) * R.W, py = (0.5f - ny * 0.5f) * R.H;
          sx0 = std::min(sx0, px); sy0 = std::min(sy0, py);
          sx1 = std::max(sx1, px); sy1 = std::max(sy1, py);
        }
        std::printf("[screen] mesh y=%.2f..%.2f x=%.2f..%.2f px=%.0f..%.0f py=%.0f..%.0f (h=%.0f)\n",
                    mn.y, mx.y, mn.x, mx.x, sx0, sx1, sy0, sy1, sy1 - sy0);
        int mi = (int)(&mesh - &player.model->meshes[0]);
        std::printf("[mesh %d] skinned=%d n=%d y=%.2f..%.2f x=%.2f..%.2f\n", mi,
                    (int)mesh.skinned, (int)mesh.pos.size(), mn.y, mx.y, mn.x, mx.x);
      }
    }
  }
  R.beginFrame();

  static int noTerrain = getenv("KK_NOTERRAIN") ? 1 : 0;
  // ---- static props ----
  for (auto& p : world.props) {
    if (noTerrain && world.terrain && p.model == world.terrain) continue;
    if (!p.model) continue;
    V3 c = p.xform.translation();
    float d = length(c - cam.pos);
    if (d > p.cullDist) continue;
    // cheap bounding-sphere style reject behind the camera
    V3 toC = c - cam.pos;
    V3 viewDir = normalize(cam.target - cam.pos);
    if (dot(normalize(toC), viewDir) < -0.35f && d > 6.f) continue;
    drawModel(p.model, p.xform, V3(1, 1, 1), 0.f);
  }

  if (getenv("KK_DEBUG_PROPS") && R.frames > 25) {
    static bool once = false;
    if (!once) {
      once = true;
      int shown = 0;
      std::printf("[props] count=%d cam=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f) proj0=%.3f\n",
                  (int)world.props.size(), R.cam.pos.x, R.cam.pos.y, R.cam.pos.z,
                  R.cam.target.x, R.cam.target.y, R.cam.target.z, R.cam.proj.m[0]);
      for (auto& p : world.props) {
        if (shown++ > 4 || !p.model || p.model->meshes.empty()) continue;
        const Mesh& mm = p.model->meshes[0];
        V3 c = p.xform.translation();
        V4 clip = (R.cam.proj * R.cam.view * p.xform).mulVec4(V4(mm.bounds.mn, 1.f));
        V4 clip2 = (R.cam.proj * R.cam.view * p.xform).mulVec4(V4(mm.bounds.mx, 1.f));
        std::printf("  prop %s xf=(%.1f,%.1f,%.1f) tris=%d  ndc1=(%.2f,%.2f,%.2f,%.2f) ndc2=(%.2f,%.2f,%.2f,%.2f)\n",
                    p.model->path.c_str(), c.x, c.y, c.z, (int)(mm.idx.size() / 3),
                    clip.x, clip.y, clip.z, clip.w, clip2.x, clip2.y, clip2.z, clip2.w);
      }
    }
  }

  if (getenv("KK_DEBUG_PROPS") && R.frames == 26) {
    const Mesh& pm = player.model->meshes[0];
    V4 a = (R.cam.proj * R.cam.view).mulVec4(V4(player.pos + V3(0, 0, 0), 1.f));
    V4 b = (R.cam.proj * R.cam.view).mulVec4(V4(player.pos + V3(0, 1.8f, 0), 1.f));
    {
      int mi = 0;
      for (auto& mm : player.model->meshes) {
        AABB bb;
        int nmesh = 0;
        for (size_t i = 0; i < mm.pos.size(); i += 37) {
          V3 p(0, 0, 0);
          if (mm.skinned && player.anim.jointMat.size()) {
            const V4& ji = mm.joints[i];
            const V4& w = mm.weights[i];
            int idx[4] = {(int)ji.x, (int)ji.y, (int)ji.z, (int)ji.w};
            float wt[4] = {w.x, w.y, w.z, w.w};
            for (int k = 0; k < 4; k++) {
              if (wt[k] <= 0.f) continue;
              if (idx[k] < 0 || idx[k] >= (int)player.anim.jointMat.size()) continue;
              p += player.anim.jointMat[idx[k]].mulPoint(mm.pos[i]) * wt[k];
            }
          } else p = mm.pos[i];
          bb.add(p);
        }
        std::printf("   mesh%d skinned=%d verts=%d joints=%d  worldY=[%.2f,%.2f] worldX=[%.2f,%.2f]\n",
                    mi++, (int)mm.skinned, (int)mm.pos.size(), (int)player.anim.jointMat.size(),
                    bb.mn.y, bb.mx.y, bb.mn.x, bb.mx.x);
        if (mi > 3) break;
        nmesh++;
      }
    }
    std::printf("[player] pos=(%.2f,%.2f,%.2f) campos=(%.1f,%.1f,%.1f) clipw=%.2f feetNDC=(%.3f,%.3f) headNDC=(%.3f,%.3f) screenH=%.1fpx meshbounds y=[%.2f,%.2f]\n",
                player.pos.x, player.pos.y, player.pos.z, R.cam.pos.x, R.cam.pos.y, R.cam.pos.z, a.w,
                a.x / a.w, a.y / a.w, b.x / b.w, b.y / b.w,
                std::fabs(b.y / b.w - a.y / a.w) * 0.5f * R.H, pm.bounds.mn.y, pm.bounds.mx.y);
  }

  // ---- enemies ----
  for (auto& e : enemies) {
    if (length(e.pos - R.cam.pos) > 120.f) continue;
    float sq = e.cfg ? e.cfg->scale : 1.f;
    Mat4 x = Mat4::translate(e.pos) * Mat4::rotY(e.facing) * Mat4::scale(V3(sq, sq, sq));
    V3 tint(1, 1, 1);
    if (e.flashTint > 0.01f) tint = V3(1.f, 1.f - e.flashTint * 2.f, 1.f - e.flashTint * 2.f);
    drawModel(e.model, x, tint, 0.f, &e.anim.jointMat);
    // weapon in hand
    if (e.cfg && e.cfg->weaponProp) {
      Model* wm = const_cast<Assets&>(assets).prop(e.cfg->weaponProp);
      if (wm) {
        Mat4 hand = e.anim.nodeWorld("handslot.r");
        Mat4 grip = Mat4::fromQuatPosScale(qFromEulerXYZ(2.876f, 0.f, 1.84f), V3(0, 0, 0), V3(1, 1, 1));
        drawModel(wm, x * hand * grip, tint, 0.f);
      }
    }
  }

  // ---- player + his gear ----
  static int onlyPlayer = getenv("KK_ONLYPLAYER") ? 1 : 0;
  if (onlyPlayer) {
    Mat4 x = Mat4::translate(player.pos) * Mat4::rotY(player.facing);
    drawModel(player.model, x, V3(1, 1, 1), 0.f, &player.anim.jointMat);
    if (player.weaponModel) {
      Mat4 hand = player.anim.nodeWorld("handslot.r");
      Mat4 grip = Mat4::fromQuatPosScale(qFromEulerXYZ(player.weaponGrip.x, player.weaponGrip.y,
                                                       player.weaponGrip.z), V3(0, 0, 0),
                                         V3(player.weaponScale, player.weaponScale, player.weaponScale));
      drawModel(player.weaponModel, x * hand * grip, V3(1, 1, 1), 0.05f);
    }
    return;
    return;
  }
  {
    float sq = 1.f;
    Mat4 x = Mat4::translate(player.pos) * Mat4::rotY(player.facing) * Mat4::scale(V3(sq, sq, sq));
    V3 tint(1, 1, 1);
    if (player.flashTint > 0.01f) tint = V3(1.f, 1.f - player.flashTint * 2.f, 1.f - player.flashTint * 2.f);
    drawModel(player.model, x, tint, 0.f, &player.anim.jointMat);

    if (player.weaponModel) {
      Mat4 hand = player.anim.nodeWorld("handslot.r");
      Mat4 grip = Mat4::fromQuatPosScale(qFromEulerXYZ(player.weaponGrip.x, player.weaponGrip.y, player.weaponGrip.z),
                                         V3(0, 0, 0), V3(player.weaponScale, player.weaponScale, player.weaponScale));
      drawModel(player.weaponModel, x * hand * grip, V3(1, 1, 1), 0.05f);
    }
    if (player.shieldModel) {
      Mat4 hand = player.anim.nodeWorld("handslot.l");
      Mat4 grip = Mat4::fromQuatPosScale(qFromEulerXYZ(-1.571f, 0.f, -1.571f), V3(0, 0, 0), V3(1, 1, 1));
      drawModel(player.shieldModel, x * hand * grip, V3(1, 1, 1), 0.02f);
    }
  }

  drawExtra3D();

  // ---- projectiles ----
  for (auto& pr : projectiles) {
    // a tiny glowing marker drawn as an overlay dot in screen space
    (void)pr;
  }

  R.rasterize();
  R.endFrame();
}

// ============================================================ HUD
static std::string toUpper(const std::string& s) {
  std::string o = s;
  for (auto& c : o) c = (char)std::toupper((unsigned char)c);
  return o;
}

void Game::drawHud() {
  float ui = (float)R.H / 360.f;
  auto S = [&](float v) { return (int)(v * ui); };

  if (titleScreen) {
    R.rect(0, 0, R.W, R.H, rgba32(6, 10, 18, 165));
    R.textCenter(R.W / 2, S(110), toUpper(title()).c_str(), rgba32(246, 240, 224), S(3));
    R.textCenter(R.W / 2, S(150), subtitle(), rgba32(232, 195, 122), S(1));
    R.textCenter(R.W / 2, S(196), "PRESS ENTER TO BEGIN", rgba32(220, 230, 240), S(1));
    R.textCenter(R.W / 2, S(226), "WASD MOVE   MOUSE LOOK   LMB ATTACK   RMB BLOCK", rgba32(190, 200, 215), S(1));
    R.textCenter(R.W / 2, S(240), "SPACE DODGE/JUMP   F INTERACT   TAB LOCK ON   ESC PAUSE", rgba32(190, 200, 215), S(1));
    R.textCenter(R.W / 2, S(258), "GAMEPAD   A DODGE   X ATTACK   B INTERACT   START PAUSE", rgba32(150, 165, 185), S(1));
    R.textCenter(R.W / 2, R.H - S(18), "KAYKIT ART BY KAY LOUSBERG (CC0)", rgba32(150, 160, 175), S(1));
    return;
  }

  // vitals
  int bx = S(18), by = R.H - S(54), bw = S(200), bh = S(11);
  R.rect(bx - 1, by - 1, bw + 2, bh + 2, rgba32(8, 12, 18, 190));
  float hpF = clampf(player.hp / player.maxHp, 0, 1);
  R.rect(bx, by, (int)(bw * hpF), bh, rgba32(179, 58, 46));
  R.rect(bx, by, bw, 2, rgba32(230, 106, 74, 160));
  R.rect(bx - 1, by + bh + 3, bw + 2, S(6) + 2, rgba32(8, 12, 18, 190));
  R.rect(bx, by + bh + 4, (int)(bw * clampf(player.stamina / player.maxStamina, 0, 1)), S(6), rgba32(120, 170, 90));
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%d / %d", (int)player.hp, (int)player.maxHp);
  R.text(bx + bw + S(8), by - S(1), buf, rgba32(240, 235, 220), S(1));

  // objective
  R.rect(S(18), S(16), S(2), S(34), rgba32(232, 195, 122));
  R.text(S(26), S(16), "OBJECTIVE", rgba32(232, 195, 122), S(1));
  R.text(S(26), S(30), toUpper(objective).c_str(), rgba32(250, 246, 235), S(1));
  if (!objectiveSub.empty()) R.text(S(26), S(42), objectiveSub.c_str(), rgba32(190, 195, 205), S(1));

  // compass
  {
    int cx = R.W / 2, cy = S(34), r = S(26);
    R.circle(cx, cy, r, rgba32(12, 18, 26, 170));
    R.circle(cx, cy, r, rgba32(232, 195, 122, 120), false);
    R.circle(cx, cy, r - 6, rgba32(255, 255, 255, 22), false);
    R.textCenter(cx, cy - r - S(12), "N", rgba32(232, 195, 122), S(1));
    // needle: toward the nearest live enemy, or straight ahead
    Actor* tgt = player.lockOn && player.lockTarget ? player.lockTarget : nullptr;
    float ang = camYaw;
    if (tgt) {
      V3 to = tgt->pos - player.pos;
      ang = std::atan2(to.x, to.z);
    }
    float rel = ang - (camYaw + PI_F);
    int nx = cx + (int)(std::sin(rel) * (r - 8));
    int ny = cy - (int)(std::cos(rel) * (r - 8));
    R.line(cx, cy, nx, ny, rgba32(143, 224, 255));
    R.circle(nx, ny, S(2), rgba32(220, 245, 255));
    std::string capStr = tgt ? std::string("TARGET") : (zoneName.empty() ? std::string() : toUpper(zoneName));
    if (!capStr.empty()) R.textCenter(cx, cy + r + S(4), capStr.c_str(), rgba32(200, 210, 225), S(1));
  }

  // gear
  {
    int gx = R.W - S(150), gy = R.H - S(50);
    R.rect(gx - S(6), gy - S(6), S(148), S(42), rgba32(12, 18, 26, 150));
    R.circle(gx, gy + S(4), S(3), rgba32(232, 195, 122));
    R.text(gx + S(10), gy, player.weaponName, rgba32(235, 230, 215), S(1));
    R.circle(gx, gy + S(18), S(3), rgba32(143, 224, 255));
    R.text(gx + S(10), gy + S(14), player.shieldName, rgba32(235, 230, 215), S(1));
  }

  // FPS
  {
    char fb[64];
    std::snprintf(fb, sizeof(fb), "%d FPS  %dx%d", (int)fps, R.W, R.H);
    R.text(R.W - R.textWidth(fb, S(1)) - S(10), S(8), fb, rgba32(150, 160, 175), S(1));
  }

  // toasts
  {
    int y = S(74);
    for (auto& t : toasts) {
      if (t.life <= 0) continue;
      int w = R.textWidth(t.text.c_str(), S(1)) + S(24);
      R.rect(R.W / 2 - w / 2, y, w, S(18), rgba32(10, 16, 24, 200));
      R.rectOutline(R.W / 2 - w / 2, y, w, S(18), rgba32(232, 195, 122, 90));
      R.textCenter(R.W / 2, y + S(5), t.text.c_str(), rgba32(245, 233, 205), S(1));
      y += S(22);
    }
  }

  // interact prompt
  {
    std::string p = interactPrompt();
    if (!p.empty() && !dialogue.active) {
      int w = R.textWidth(p.c_str(), S(1)) + S(30);
      int y = R.H - S(96);
      R.rect(R.W / 2 - w / 2, y, w, S(20), rgba32(246, 238, 219, 235));
      R.rect(R.W / 2 - w / 2, y, S(16), S(20), rgba32(38, 36, 29));
      R.text(R.W / 2 - w / 2 + S(5), y + S(6), "F", rgba32(246, 238, 219), S(1));
      R.textCenter(R.W / 2 + S(8), y + S(6), p.c_str(), rgba32(38, 36, 29), S(1));
    }
  }

  // dialogue
  if (dialogue.active) {
    int w = (int)(R.W * 0.72f), h = S(58);
    int x = (R.W - w) / 2, y = R.H - S(96) - h;
    R.rect(x, y, w, h, rgba32(12, 18, 27, 240));
    R.rectOutline(x, y, w, h, rgba32(143, 224, 255, 90));
    R.circle(x + S(24), y + S(24), S(14), rgba32(120, 200, 240));
    R.text(x + S(46), y + S(10), toUpper(dialogue.speaker).c_str(), rgba32(143, 224, 255), S(1));
    const std::string& line = dialogue.lines[std::min((size_t)dialogue.index, dialogue.lines.size() - 1)];
    // crude wrap
    std::string rest = line, out;
    int maxChars = (w - S(60)) / (FONT_W + 1);
    while ((int)rest.size() > maxChars) {
      size_t cut = rest.rfind(' ', maxChars);
      if (cut == std::string::npos) cut = maxChars;
      out += rest.substr(0, cut) + "\n";
      rest = rest.substr(cut + 1);
    }
    out += rest;
    R.text(x + S(46), y + S(24), out.c_str(), rgba32(242, 236, 220), S(1));
    R.text(x + S(46), y + h - S(11), "[F] CONTINUE", rgba32(140, 150, 165), S(1));
  }

  // boss bar
  if (bossActive && boss && !boss->dead) {
    int w = (int)(R.W * 0.52f), x = (R.W - w) / 2, y = S(12);
    R.textCenter(R.W / 2, y, toUpper(boss->cfg->name).c_str(), rgba32(223, 234, 255), S(1));
    R.rect(x, y + S(14), w, S(8), rgba32(6, 10, 16, 200));
    R.rect(x, y + S(14), (int)(w * clampf(boss->hp / boss->maxHp, 0, 1)), S(8), rgba32(147, 182, 234));
  }

  if (!locked) {
    R.textCenter(R.W / 2, R.H - S(30), "CLICK TO CAPTURE MOUSE", rgba32(200, 210, 225, 180), S(1));
  }

  if (paused) {
    R.rect(0, 0, R.W, R.H, rgba32(6, 10, 16, 190));
    R.textCenter(R.W / 2, R.H / 2 - S(16), "PAUSED", rgba32(245, 240, 225), S(2));
    R.textCenter(R.W / 2, R.H / 2 + S(10), "ESC TO RESUME", rgba32(200, 210, 225), S(1));
  }

  if (won) {
    R.rect(0, 0, R.W, R.H, rgba32(20, 40, 60, 170));
    R.textCenter(R.W / 2, R.H / 2 - S(10), winText(), rgba32(234, 246, 255), S(2));
    R.textCenter(R.W / 2, R.H / 2 + S(16), "ESC TO LEAVE", rgba32(200, 215, 230), S(1));
  }

  drawHudExtra();
}
