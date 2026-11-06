// ---------------------------------------------------------------------------
//  main.cpp — entry point, argument parsing, the frame loop
// ---------------------------------------------------------------------------
#include "game.h"
#include "classes.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
extern long long g_px;
#include <cstring>
#include <string>

Game* createSunkenSpire();

#ifdef _WIN32
// The portable build is a GUI-subsystem exe (-mwindows) so double-clicking it
// does not flash a console window. When it is started from a command line with
// diagnostic flags we attach to (or open) a console so the log is still visible.
extern "C" {
__declspec(dllimport) int __stdcall AttachConsole(unsigned long dwProcessId);
__declspec(dllimport) int __stdcall AllocConsole(void);
}
static void attachConsoleIfUseful(int argc, char** argv) {
  if (argc <= 1) return;
  bool wantsLog = false;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--help" || a == "-h" || a == "--headless" || a == "--offscreen" ||
        a == "--smoke" || a == "--shot" || a == "--frames" || a == "--shots-every" ||
        a == "--console")
      wantsLog = true;
  }
  if (!wantsLog) return;
  if (!AttachConsole((unsigned long)-1)) {   // ATTACH_PARENT_PROCESS
    if (!AllocConsole()) return;
  }
  FILE* f = nullptr;
  freopen_s(&f, "CONOUT$", "w", stdout);
  freopen_s(&f, "CONOUT$", "w", stderr);
}
#endif

static bool dirHasAssets(const std::string& dir) {
  return fileExists(dir + "/characters") || fileExists(dir + "/characters/Knight.glb");
}

static std::string findDataRoot(const std::string& explicitRoot) {
  std::vector<std::string> candidates;
  if (!explicitRoot.empty()) candidates.push_back(explicitRoot);
  candidates.push_back("assets");
  candidates.push_back("../assets");
  candidates.push_back("../../assets");
  candidates.push_back("../game/public/assets");
  candidates.push_back("../../game/public/assets");
  candidates.push_back("../spire/public/assets");
  candidates.push_back("../../spire/public/assets");
  candidates.push_back("game/public/assets");
  candidates.push_back("spire/public/assets");
  for (auto& c : candidates) {
    if (fileExists(c + "/characters/Knight.glb")) return c;
  }
  return explicitRoot.empty() ? "assets" : explicitRoot;
}

// directory of the running executable (so a copied build still finds ./assets)
static std::string exeDir(const char* argv0) {
  std::string p = argv0 ? argv0 : "";
  size_t slash = p.find_last_of("/\\");
  if (slash == std::string::npos) return ".";
  return p.substr(0, slash);
}

int main(int argc, char** argv) {
  // line-buffer the log: a GUI-subsystem binary otherwise drops everything
  // written before it stops, which makes a hang impossible to diagnose
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
#ifdef _WIN32
  attachConsoleIfUseful(argc, argv);
#endif
  std::string gameName;
  std::string dataRoot;
  bool headless = false;
  int frames = 0;
  std::string shot;
  int winW = 1280, winH = 720;
  int renW = 0, renH = 0;   // 0 = match the window (1:1, no upscale blur)
  bool renderPinned = false;   // --render-width/height disables adaptive scaling
  bool autoPlay = false;
  bool smoke = false;
  bool showTitle = false;
  bool showSelect = false;
  std::string classArg;
  int shotsEvery = 0;
  float warpX = 1e9f, warpZ = 1e9f;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&](const char* def) -> std::string { return i + 1 < argc ? argv[++i] : def; };
    if (a == "--game" || a == "-g") gameName = next("spire");
    else if (a == "--data" || a == "-d") dataRoot = next("");
    else if (a == "--headless" || a == "--offscreen") headless = true;
    else if (a == "--frames") frames = std::atoi(next("120").c_str());
    else if (a == "--shot") shot = next("shot.bmp");
    else if (a == "--shots-every") shotsEvery = std::atoi(next("0").c_str());
    else if (a == "--width") winW = std::atoi(next("1280").c_str());
    else if (a == "--height") winH = std::atoi(next("720").c_str());
    else if (a == "--render-width") { renW = std::atoi(next("640").c_str()); renderPinned = true; }
    else if (a == "--render-height") { renH = std::atoi(next("360").c_str()); renderPinned = true; }
    else if (a == "--play") autoPlay = true;
    else if (a == "--smoke") { smoke = true; autoPlay = true; }
    else if (a == "--title") showTitle = true;
    else if (a == "--select") showSelect = true;
    else if (a == "--class") classArg = next("duelist");
    else if (a == "--warp") { warpX = (float)std::atof(next("0").c_str()); warpZ = (float)std::atof(next("0").c_str()); }
    else if (a == "--help" || a == "-h") {
      std::printf(
          "Echoes of the Sunken Spire - native C++ port\n"
          "usage: %s [--data <assets dir>] [options]\n"
          "  --headless            no window, render frames offscreen (for checks)\n"
          "  --frames N            run N frames then exit\n"
          "  --shot file.bmp       write the last frame to a BMP\n"
          "  --shots-every N       write shot0001.bmp, shot0002.bmp ... every N frames\n"
          "  --width/--height      window size (default 1280x720)\n"
          "  --render-width/-height internal render resolution (default: the window size)\n"
          "  --play                skip the title + class picker and start playing\n"
          "  --title               keep the title screen (screenshots)\n"
          "  --select              open the class picker (screenshots)\n"
          "  --class <id>          choose a class: duelist, warden, mage, ranger, berserker\n"
          "  --smoke               scripted self-test: walk, jump, attack, ability, elixir (prints state)\n"
          "  --warp X Z            drop the player at world X,Z (debug)\n",
          argv[0]);
      return 0;
    }
  }

  // internal resolution: 1:1 with the window unless asked otherwise. The old
  // 640x360 default was then point-scaled to 1280x720, which is exactly the
  // "downgraded" look compared with the WebGL build.
  if (renW <= 0) renW = winW;
  if (renH <= 0) renH = winH;

  if (gameName.empty()) {
    // no --game: pick the campaign whose assets are next to us
    std::string ex = exeDir(argc > 0 ? argv[0] : "");
    if (fileExists(ex + "/game.txt")) {
      std::vector<uint8_t> raw;
      if (readFile(ex + "/game.txt", raw))
        gameName.assign(raw.begin(), raw.end());
      while (!gameName.empty() && (gameName.back() == '\n' || gameName.back() == '\r' ||
                                   gameName.back() == ' '))
        gameName.pop_back();
    }
    if (gameName.empty()) gameName = "spire";
    // no game.txt: guess from the asset set (the Spire packs ship nature/dungeon/platform)
    std::string probe = dataRoot.empty() ? ex + "/assets" : dataRoot;
    if (fileExists(probe + "/nature") || fileExists(probe + "/platform"))
      gameName = "spire";
    else if (fileExists(probe + "/characters/Knight.glb"))
      gameName = "spire";
  }
  {
    std::string ex = exeDir(argc > 0 ? argv[0] : "");
    std::string exAssets = ex + "/assets";
    if (dataRoot.empty() && dirHasAssets(exAssets)) dataRoot = exAssets;
    dataRoot = findDataRoot(dataRoot);
  }
  std::printf("[kaykit-native] game=%s data=%s\n", gameName.c_str(), dataRoot.c_str());
#ifdef KAYKIT_EMBEDDED_ASSETS
  std::printf("[kaykit-native] portable build: %d assets embedded in this executable "
              "(no assets folder needed)\n", vfs::count());
#endif

  Game* game = createSunkenSpire();
  game->init(dataRoot, renW, renH, headless);

  // resolve --class (id, name, or friendly alias) before the world is built
  if (!classArg.empty()) {
    int ci = classIndexById(classArg.c_str());
    if (ci < 0) {
      std::fprintf(stderr, "[kaykit-native] unknown class '%s' - using duelist\n", classArg.c_str());
      ci = 0;
    }
    game->player.classIndex = ci;
  }

  plat::Config cfg;
  cfg.title = game->title();
  cfg.windowW = winW;
  cfg.windowH = winH;
  cfg.renderW = renW;
  cfg.renderH = renH;
  cfg.headless = headless;
  if (!plat::init(cfg, game->in)) {
    std::fprintf(stderr, "[kaykit-native] platform init failed\n");
    return 1;
  }

  // build the world (meshes, textures, skeletons, enemies)
  double t0 = plat::nowSeconds();
  game->buildContent();
  // debug camera overrides (handy for screenshots)
  if (const char* cy = getenv("KK_CAMYAW")) game->camYaw = (float)atof(cy);
  if (const char* cp = getenv("KK_CAMPITCH")) game->camPitch = (float)atof(cp);
  if (const char* cd = getenv("KK_CAMDIST")) game->camDist = (float)atof(cd);
  std::printf("[kaykit-native] world built in %.2fs (%d props, %d colliders, %d enemies)\n",
              plat::nowSeconds() - t0, (int)game->world.props.size(),
              (int)game->world.colliders.size(), (int)game->enemies.size());

  if (game->world.props.size() < 20) {
    std::fprintf(stderr,
                 "[kaykit-native] warning: only %d props loaded - the assets folder probably does not\n"
                 "                 belong to the '%s' campaign (expected characters/, props/, weapons/,\n"
                 "                 anim/ and, for the Spire, nature/ dungeon/ wargear/)\n",
                 (int)game->world.props.size(), gameName.c_str());
  }

  if (getenv("KAYKIT_MODELS")) {
    int nPropsNoMesh = 0;
    for (auto& p : game->world.props) {
      if (!p.model) { nPropsNoMesh++; continue; }
      if (p.model->meshes.empty()) {
        nPropsNoMesh++;
        if (nPropsNoMesh < 6 || 1) {
          std::printf("[models] EMPTY %s  (nodes=%d mats=%d texs=%d)\n",
                      p.model->path.c_str(), (int)p.model->nodes.size(),
                      (int)p.model->mats.size(), (int)p.model->texs.size());
        }
      }
    }
    std::printf("[models] props without meshes: %d / %d\n", nPropsNoMesh, (int)game->world.props.size());
  }

  if (showSelect) {
    game->openClassSelect();
  } else if (!showTitle && (autoPlay || headless)) {
    game->beginPlay();
    if (!headless) plat::setMouseCapture(true);
  }
  if (warpX < 1e8f) {
    game->player.pos.x = warpX;
    game->player.pos.z = warpZ;
    game->camPos = game->player.pos + V3(0, 4.f, game->camDist);
  }

  // adaptive quality: only when the user did not pin a render size
  bool adaptive = !renderPinned;
  float renderScale = 1.0f;
  int fpsStableFor = 0;
  if (adaptive) {
    game->R.resize(renW, renH);
    renderScale = (float)renW / (float)winW;
  }

  double accS = 0, accH = 0, accU = 0;
  double last = plat::nowSeconds();
  double fpsAcc = 0;
  int fpsFrames = 0;
  int frame = 0;
  char shotName[512];

  while (!game->in.quit) {
    double now = plat::nowSeconds();
    float dt = (float)std::min(0.05, now - last);
    last = now;
    if (headless) dt = 1.f / 60.f;

    // ---- scripted self-test ----
    Input& in = game->in;
    if (smoke) {
      static const Key scripted[] = {KEY_W, KEY_A, KEY_S, KEY_D, KEY_SPACE, KEY_TAB, KEY_Q, KEY_E};
      for (Key k : scripted) in.down[k] = false;
      int ph = frame % 240;
      if (ph < 50) in.down[KEY_W] = true;                 // walk forward
      if (ph >= 180 && ph < 225) in.down[KEY_W] = true;   // walk again
      if (ph >= 100 && ph < 150) in.down[KEY_TAB] = true; // lock on
      if (ph == 90) { in.down[KEY_SPACE] = true; in.pressed[KEY_SPACE] = true; }
      if (ph == 52 || ph == 66 || ph == 80 || ph == 195 || ph == 210) in.mousePressed[0] = true;
      if (ph == 5 || ph == 120) in.pressed[KEY_F] = true;   // interact
      if (ph == 160) in.pressed[KEY_Q] = true;            // bolt
      if (ph == 172) in.pressed[KEY_E] = true;            // elixir
      if (ph % 60 == 0) {
        int alive = 0;
        for (auto& e : game->enemies) if (!e.dead) alive++;
        std::printf("[smoke] f=%3d pos=(%.1f,%.1f,%.1f) hp=%.0f/%.0f enemies=%d alive=%d state=%d obj=\"%s\" sub=\"%s\"\n",
                    frame, game->player.pos.x, game->player.pos.y, game->player.pos.z,
                    game->player.hp, game->player.maxHp, (int)game->enemies.size(), alive,
                    (int)game->player.state, game->objective.c_str(),
                    game->objectiveSub.c_str());
      }
    }

    // ---- input edges ----
    game->locked = plat::mouseCaptured();
    if (!game->titleScreen && !game->classSelect && !game->paused && !game->locked && in.mouseHit(0)) {
      // first click grabs the pointer, it should not also swing the sword
      plat::setMouseCapture(true);
      game->in.mousePressed[0] = false;
      game->in.padMousePressed[0] = false;
      game->locked = true;
    }
    if (game->titleScreen) {
      if (in.hit(KEY_ENTER) || in.hit(KEY_SPACE) || in.mouseHit(0)) {
        game->in.mousePressed[0] = false;
        game->in.padMousePressed[0] = false;
        game->openClassSelect();
      }
      if (in.hit(KEY_ESC)) break;
    } else if (game->classSelect) {
      game->updateClassSelect();
    } else {
      if (in.hit(KEY_ESC)) {
        if (game->paused) {
          game->paused = false;
          plat::setMouseCapture(true);
        } else {
          game->paused = true;
          plat::setMouseCapture(false);
        }
      }
      if (in.hit(KEY_F)) {
        if (game->dialogue.active) game->advanceDialogue();
        else game->onInteract();
      }
    }

    // ---- simulate ----
    auto tUp0 = std::chrono::high_resolution_clock::now();
    if (!game->paused) {
      float wdt = dt;
      if (game->hitstop > 0) {
        game->hitstop = std::max(0.f, game->hitstop - dt);
        wdt = dt * 0.15f;
      }
      game->time += dt;
      if (!game->titleScreen && !game->classSelect) {
        game->updatePlayer(wdt);
        game->updateEnemies(wdt);
        game->updateProjectiles(wdt);
        game->resolveBodies();
        game->onUpdate(wdt);
      } else if (game->classSelect) {
        // slow orbit around the previewed character
        game->camYaw += dt * 0.35f;
        game->camDist = 6.5f;
        game->camPitch = 0.2f;
        game->player.anim.update(dt);
      } else {
        // idle orbit behind the title
        game->camYaw += dt * 0.12f;
        game->camDist = 12.f;
        game->camPitch = 0.28f;
        game->player.anim.update(dt);
      }
      // toasts
      for (auto& t : game->toasts) t.life -= dt;
      game->toasts.erase(std::remove_if(game->toasts.begin(), game->toasts.end(),
                                        [](const Game::Toast& t) { return t.life <= 0; }),
                         game->toasts.end());
      // shake decay
      game->shakeAmt *= std::pow(0.004f, dt);
    }

    if (getenv("KK_PROFILE")) {
      accU += std::chrono::duration<double, std::milli>(
                  std::chrono::high_resolution_clock::now() - tUp0).count();
    }
    // ---- draw ----
    auto tDraw0 = std::chrono::high_resolution_clock::now();
    game->R.submitted = 0;
    game->drawScene();
    if (getenv("KAYKIT_STATS") && frame % 30 == 0) {
      std::printf("[render] frame %d  draws=%lld  tris=%lld  px=%lld\n", frame,
                  (long long)game->R.submitted, (long long)game->R.tris, (long long)g_px);
    }
    auto tDraw1 = std::chrono::high_resolution_clock::now();
    game->drawHud();
    auto tDraw2 = std::chrono::high_resolution_clock::now();
    if (getenv("KK_PROFILE")) {
      double sceneMs = std::chrono::duration<double, std::milli>(tDraw1 - tDraw0).count();
      double hudMs = std::chrono::duration<double, std::milli>(tDraw2 - tDraw1).count();
      accS += sceneMs; accH += hudMs;
      if (frame % 60 == 59) {
        std::printf("[profile] scene=%.2fms hud=%.2fms update=%.2fms draws=%lld tris=%lld culled=%lld\n",
                    accS / 60, accH / 60, accU / 60, (long long)game->R.submitted,
                    (long long)game->R.tris, (long long)game->R.culledFrustum);
        accS = accH = accU = 0;
      }
    }
    if (game->shakeAmt > 0.01f) {
      // cheap screen shake: shift the HUD-less frame by a couple of pixels
      int sx = (int)((std::rand() % 100 - 50) / 100.f * game->shakeAmt * 6);
      int sy = (int)((std::rand() % 100 - 50) / 100.f * game->shakeAmt * 6);
      std::vector<uint32_t> tmp = game->R.color;
      for (int y = 0; y < game->R.H; y++)
        for (int x = 0; x < game->R.W; x++) {
          int px = std::clamp(x + sx, 0, game->R.W - 1), py = std::clamp(y + sy, 0, game->R.H - 1);
          game->R.color[(size_t)y * game->R.W + x] = tmp[(size_t)py * game->R.W + px];
        }
    }
    if (!headless) plat::present(game->R);

    // ---- fps + screenshots ----
    fpsAcc += dt;
    fpsFrames++;
    if (fpsAcc > 0.5) {
      game->fps = (float)(fpsFrames / fpsAcc);
      fpsAcc = 0;
      fpsFrames = 0;

      // ---- adaptive internal resolution ----
      // A pure software rasterizer is fill-rate bound, so rather than shipping
      // a permanently low resolution (which is what made the port look worse
      // than the WebGL build) the scale floats: crisp when the machine can
      // afford it, softer rather than stuttering when it cannot.
      if (adaptive && !headless && !autoPlay) {
        if (game->fps < 26.f && renderScale > 0.5f) {
          renderScale = std::max(0.5f, renderScale - 0.15f);
          game->R.resize(std::max(320, (int)(winW * renderScale)),
                         std::max(180, (int)(winH * renderScale)));
          fpsStableFor = 0;
        } else if (game->fps > 55.f && renderScale < 1.0f) {
          if (++fpsStableFor >= 4) {
            renderScale = std::min(1.0f, renderScale + 0.1f);
            game->R.resize(std::max(320, (int)(winW * renderScale)),
                           std::max(180, (int)(winH * renderScale)));
            fpsStableFor = 0;
          }
        } else {
          fpsStableFor = 0;
        }
      }
      char title[256];
      std::snprintf(title, sizeof(title), "%s  -  %d fps  -  %s", game->title(),
                    (int)game->fps, game->zoneName.c_str());
      plat::setTitle(title);
    }
    if (shotsEvery > 0 && frame % shotsEvery == 0) {
      std::snprintf(shotName, sizeof(shotName), "shot%04d.bmp", frame);
      writeBMP(shotName, game->R.W, game->R.H, game->R.color.data());
    }

    frame++;
    if (frames > 0 && frame >= frames) break;
    if (headless) {
      // no vsync in headless mode
    } else {
      plat::sleepMs(0);
    }
    game->in.newFrame();
    plat::pump(game->in);
  }

  if (!shot.empty()) {
    writeBMP(shot, game->R.W, game->R.H, game->R.color.data());
    std::printf("[kaykit-native] wrote %s (%dx%d)\n", shot.c_str(), game->R.W, game->R.H);
  }
  plat::shutdown();
  delete game;
  return 0;
}
