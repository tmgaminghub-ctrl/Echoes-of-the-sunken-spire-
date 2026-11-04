// ---------------------------------------------------------------------------
//  game.h — shared action-RPG layer for both native ports
// ---------------------------------------------------------------------------
#pragma once
#include "render.h"
#include "platform.h"
#include "gltf.h"

struct ClassDef;   // classes.h — playable paths (Duelist, Warden, Mage, ...)

// ------------------------------------------------------------------ world
struct Collider {
  float x = 0, z = 0, r = 1, h = 2.2f;
};
struct Plat {
  float x = 0, z = 0, w = 4, d = 4, top = 0;
  float roundR = 0;      // >0 -> circular support
  bool ghost = false;    // only solid during a temporal shift
  bool active = true;
};
struct StaticProp {
  const Model* model = nullptr;
  Mat4 xform;
  float cullDist = 90.f;
};

struct World {
  std::string name;
  std::vector<Collider> colliders;
  std::vector<Plat> platforms;
  std::vector<StaticProp> props;
  V3 spawn{0, 0, 0};
  float bound = 60.f;
  V3 skyTop{0.35f, 0.55f, 0.85f};
  V3 skyBot{0.72f, 0.82f, 0.92f};
  V3 fogColor{0.72f, 0.82f, 0.92f};
  float fogDensity = 0.0045f;
  V3 sunDir{-0.45f, 1.0f, 0.35f};
  V3 sunColor{1.f, 0.96f, 0.86f};
  float sunPower = 1.0f;
  V3 ambient{0.42f, 0.46f, 0.55f};

  float groundY(float x, float z) const { return groundYFn ? groundYFn(x, z) : 0.f; }
  float (*groundYFn)(float, float) = nullptr;

  Model* terrain = nullptr;
  void addCollider(float x, float z, float r, float h = 2.2f) { colliders.push_back({x, z, r, h}); }
  // builds a vertex-coloured terrain mesh (seg x seg quads over `size`) and
  // registers it as a giant static prop
  void buildTerrain(int seg, float size, const V3& colorA, const V3& colorB, uint32_t seed);
  void addProp(const Model* m, const Mat4& x, float cull = 90.f) { props.push_back({m, x, cull}); }
};

// ------------------------------------------------------------------ assets
struct Assets {
  std::string root;
  std::unordered_map<std::string, Model*> cache;
  std::unordered_map<std::string, const Clip*> clips;

  Model* load(const std::string& rel);
  Model* tryLoad(const std::string& rel);
  // looks in several pack folders for a prop name with optional extension
  Model* prop(const char* name);
  void loadClipPack(const char* rel);
  const Clip* clip(const char* name) const {
    auto it = clips.find(name);
    return it == clips.end() ? nullptr : it->second;
  }
  const Clip* clipAny(std::initializer_list<const char*> names) const {
    for (auto n : names) if (const Clip* c = clip(n)) return c;
    return nullptr;
  }
  ~Assets();
};

// ------------------------------------------------------------------ anim
struct Animator {
  const Model* model = nullptr;
  std::vector<Mat4> world, jointMat;
  std::vector<Quat> rot, rotPrev;
  std::vector<V3> trans, transPrev, scl, sclPrev;
  std::vector<int> order;
  const Clip* cur = nullptr;
  const Clip* prev = nullptr;
  float tCur = 0, tPrev = 0;
  float fade = 0, fadeDur = 0.18f;
  float speed = 1.f;
  bool loop = true;
  std::unordered_map<const Clip*, std::vector<int>> bindCache;

  void bind(const Model* m);
  void play(const Clip* c, float fadeDur = 0.18f, bool loop = true, float speed = 1.f);
  void update(float dt);
  Mat4 nodeWorld(const char* name) const;
  bool valid() const { return model && !world.empty(); }
};

// ------------------------------------------------------------------ actors
enum class ActorState { Idle, Move, Attack, Dodge, Block, Hurt, Dead, Grapple };

struct WeaponDef {
  const char* name;
  const char* modelPath;
  float damage;
  float reach;
  float speed;
  V3 gripEuler;
  float scale;
  float comboCancel;
};

struct Actor {
  V3 pos{0, 0, 0};
  V3 vel{0, 0, 0};
  float facing = 0;
  float radius = 0.55f;
  float hp = 100, maxHp = 100;
  bool dead = false;
  float flash = 0;
  Animator anim;
  const Model* model = nullptr;
  float flashTint = 0;
};

struct Player : Actor {
  float stamina = 100, maxStamina = 100;
  bool blocking = false;
  ActorState state = ActorState::Idle;

  // combat
  int combo = 0;
  float attackT = 0, attackDur = 0.5f;
  bool hitApplied = false;
  float attackCooldown = 0, comboWindow = 0;
  float dodgeT = 0, dodgeDur = 0.42f;
  V3 dodgeDir;
  float hurtT = 0;
  float invuln = 0;

  // vertical
  float vy = 0;
  bool grounded = true;
  float coyote = 0, jumpBuffer = 0;
  int jumpsLeft = 1;
  bool gliding = false;
  float airTime = 0;
  bool onPlatform = false;
  float platformY = 0;

  // gear
  const Model* weaponModel = nullptr;
  const Model* shieldModel = nullptr;
  V3 weaponGrip{0, 0, 0}, shieldGrip{0, 0, 0};
  float weaponScale = 1.f, shieldScale = 1.f;
  const char* weaponName = "";
  const char* shieldName = "";

  // targeting
  Actor* lockTarget = nullptr;
  bool lockOn = false;
  bool glideUnlocked = false;
  float stepTimer = 0;
  float speedSmooth = 0;
  float yawTarget = 0;

  // class / abilities
  const ClassDef* cls = nullptr;   // active playable class (see classes.h)
  int classIndex = 0;
  float abilityCd = 0;             // seconds until Q is ready again
  float abilityT = 0;              // >0 while a signature ability is animating
  int elixirs = 3;                 // shared healing draughts (E)
  float elixirCd = 0;
  float attackDamage = 16.f;       // resolved from class weapon x damage mult
  float attackReach = 2.6f;
  float moveSpeed = 5.6f;
};

extern WeaponDef kRapier;
extern WeaponDef kLongsword;

struct EnemyCfg {
  const char* name;
  const char* modelPath;
  const char* weaponProp;    // may be null
  const char* shieldProp;
  float hp, speed, damage, reach, windup, recover, cooldown;
  float aggro, scale;
  bool ranged;
  bool boss;
  const char* attackClip;
};

struct Enemy : Actor {
  const EnemyCfg* cfg = nullptr;
  int uid = 0;                     // stable id (piercing shots track who they hit)
  ActorState state = ActorState::Idle;
  float attackT = 0, cooldownT = 0, hurtT = 0;
  bool hitApplied = false;
  bool aggroed = false;
  float deathT = 0;
  V3 home{0, 0, 0};
  float eVelX = 0, eVelZ = 0;
  float wanderT = 0;
  V3 wanderDir{0, 0, 0};
  float circleDir = 1, circleT = 2;
  float castT = 0;
  bool summoned = false;
  bool isBoss() const { return cfg && cfg->boss; }
};

struct Projectile {
  V3 pos, vel;
  float life = 4.f;
  float damage = 10.f;
  bool dead = false;
  bool fromPlayer = false;
  bool pierce = false;             // passes through enemies (Ranger's Piercing Shot)
  V3 color{1.f, 0.6f, 0.2f};       // tracer tint
  std::vector<int> hitUids;        // enemies already struck (for pierce)
};

// ------------------------------------------------------------------ game
class Game {
 public:
  Renderer R;
  Input in;
  World world;
  Assets assets;
  Player player;
  std::vector<Enemy> enemies;
  std::vector<Projectile> projectiles;
  int nextEnemyUid = 1;
  Camera cam;
  float camYaw = 0.f, camPitch = 0.26f, camDist = 6.4f, camPosInit = 0;
  float camIdle = 0.f;
  V3 camPos;
  bool paused = false;
  bool titleScreen = true;
  bool classSelect = false;        // class-picker screen (between title and play)
  int classCursor = 0;             // highlighted class on the picker
  float time = 0;
  float hitstop = 0;
  float shakeAmt = 0;
  std::string objective = "";
  std::string objectiveSub = "";
  std::string zoneName = "";
  Enemy* boss = nullptr;
  bool bossActive = false;
  bool won = false;
  bool locked = false;

  // dialogue / toasts
  struct Toast { std::string text; float life; };
  std::vector<Toast> toasts;
  struct Dialogue { std::string speaker; std::vector<std::string> lines; int index = 0; bool active = false; };
  Dialogue dialogue;

  float fps = 0;
  int frameCount = 0;
  double fpsTimer = 0;

  virtual ~Game() {}
  virtual const char* title() const = 0;
  virtual const char* subtitle() const = 0;
  virtual const char* winText() const = 0;
  virtual void buildContent() = 0;             // world + assets + spawns
  virtual void onUpdate(float dt) {}           // per-game quest logic
  virtual void drawHudExtra() {}               // per-game HUD bits
  virtual void drawExtra3D() {}                // NPCs and per-game world actors
  virtual void onInteract() {}
  virtual std::string interactPrompt() { return ""; }

  // shared engine
  void init(const std::string& dataRoot, int rw, int rh, bool headless);
  void runFrame(float dt);
  void drawScene();
  void drawHud();
  void updatePlayer(float dt);
  void updateEnemies(float dt);
  void updateProjectiles(float dt);
  void resolveBodies();
  void resolveStatic(V3& pos, float radius);
  void meleeArc(const V3& origin, const V3& dir, float range, float cosLimit, float damage, bool crit = false);
  void spawnEnemy(const char* cfgName, float x, float z);
  bool groundSupport(float& outY, bool& onPlat, float x, float z, float currentY, float vy);
  void addToasts(const char* text, float secs = 2.6f);
  void say(const char* speaker, std::vector<std::string> lines);
  void advanceDialogue();
  void equipWeapon(const WeaponDef& def);
  void drawModel(const Model* m, const Mat4& xform, V3 tint, float extraLight = 0.f, const std::vector<Mat4>* joints = nullptr);
  Actor* nearestEnemyTarget(float maxRange);
  void damagePlayer(float dmg, const V3& fromDir);
  const EnemyCfg* enemyCfg(const char* name);
  void setObjective(const char* main, const char* sub);
  void startTitle();
  void beginPlay();

  // ---- class system (classes.h / classes.cpp) ----
  void applyClass(int index);          // equip model, weapon, stats for a class
  void openClassSelect();              // title -> class picker
  void updateClassSelect();            // picker input (called from main loop)
  void drawClassSelect();              // picker overlay
  void confirmClass();                 // picker -> beginPlay with chosen class
  void useAbility();                   // Q — signature class ability
  void useElixir();                    // E — shared heal
  void fireProjectile(const V3& dir, float speed, float damage, bool pierce, const V3& color);
  void drawClassHud();                 // ability cooldown + elixir pips
  float camForwardX() const { return -std::sin(camYaw); }
  float camForwardZ() const { return -std::cos(camYaw); }
};
