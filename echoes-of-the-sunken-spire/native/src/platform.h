// ---------------------------------------------------------------------------
//  platform.h — window, input, timing. X11 (Linux) / Win32 / headless.
// ---------------------------------------------------------------------------
#pragma once
#include "render.h"

enum Key {
  KEY_NONE = 0,
  KEY_ESC = 1, KEY_SPACE, KEY_SHIFT, KEY_CTRL, KEY_TAB, KEY_ENTER, KEY_BACKSPACE,
  KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
  KEY_W, KEY_A, KEY_S, KEY_D, KEY_Q, KEY_E, KEY_F, KEY_R, KEY_J, KEY_M, KEY_P, KEY_1, KEY_2, KEY_3,
  KEY_COUNT = 64
};

struct Input {
  bool down[KEY_COUNT] = {false};
  bool pressed[KEY_COUNT] = {false};      // edge, this frame only
  bool mouseDown[3] = {false, false, false};
  bool mousePressed[3] = {false, false, false};
  // controller state is kept apart from the keyboard/mouse so either device
  // can drive the game without stomping on the other
  bool padDown[KEY_COUNT] = {false};
  bool padPressed[KEY_COUNT] = {false};
  bool padMouseDown[3] = {false, false, false};
  bool padMousePressed[3] = {false, false, false};
  float mouseDX = 0, mouseDY = 0;
  bool quit = false;
  bool focus = true;
  bool padActive = false;    // last input came from a controller (HUD hints)

  void newFrame() {
    std::memset(pressed, 0, sizeof(pressed));
    std::memset(mousePressed, 0, sizeof(mousePressed));
    std::memset(padPressed, 0, sizeof(padPressed));
    std::memset(padMousePressed, 0, sizeof(padMousePressed));
    std::memset(padDown, 0, sizeof(padDown));
    std::memset(padMouseDown, 0, sizeof(padMouseDown));
    mouseDX = mouseDY = 0;
  }
  bool held(Key k) const { return down[k] || padDown[k]; }
  bool hit(Key k) const { return pressed[k] || padPressed[k]; }
  bool mouseHeld(int i) const { return mouseDown[i] || padMouseDown[i]; }
  bool mouseHit(int i) const { return mousePressed[i] || padMousePressed[i]; }
};

namespace plat {

struct Config {
  const char* title = "KayKit Native";
  int windowW = 1280;
  int windowH = 720;
  int renderW = 640;
  int renderH = 360;
  bool headless = false;
  bool grabMouse = true;
};

bool init(const Config& cfg, Input& input);
// mouse capture: the game runs with the pointer grabbed so look never leaves the window
void setMouseCapture(bool on);
bool mouseCaptured();
void pump(Input& in);                       // events -> input state, then in.newFrame() next frame
void present(Renderer& r);
void setTitle(const char* t);
void shutdown();
bool isHeadless();
double nowSeconds();
void sleepMs(int ms);
// Frames-per-second helper
double tickFPS();

}  // namespace plat
