// ---------------------------------------------------------------------------
//  platform.cpp — one file, three back ends:
//     _WIN32  -> Win32 window + StretchDIBits
//     __linux__/unix -> X11 window + XPutImage
//     headless -> no window at all (used for automated checks / screenshots)
// ---------------------------------------------------------------------------
#include "platform.h"
#include <chrono>
#include <thread>
#include <string>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <windowsx.h>
  #include <xinput.h>
  #pragma comment(lib, "xinput.lib")
#elif defined(__linux__)
  #include <X11/Xlib.h>
  #include <X11/keysym.h>
  #include <X11/Xutil.h>
#endif

namespace plat {
namespace {

Config g_cfg;
Input* g_input = nullptr;
bool g_headless = true;
double g_startTime = 0;
double g_lastFPS = 0;
double g_fpsAccum = 0;
int g_fpsFrames = 0;

double nowSecondsImpl() {
  using clock = std::chrono::high_resolution_clock;
  static auto t0 = clock::now();
  return std::chrono::duration<double>(clock::now() - t0).count();
}

Key mapAscii(int c) {
  switch (c) {
    case 'W': case 'w': return KEY_W;
    case 'A': case 'a': return KEY_A;
    case 'S': case 's': return KEY_S;
    case 'D': case 'd': return KEY_D;
    case 'Q': case 'q': return KEY_Q;
    case 'E': case 'e': return KEY_E;
    case 'F': case 'f': return KEY_F;
    case 'R': case 'r': return KEY_R;
    case 'J': case 'j': return KEY_J;
    case 'M': case 'm': return KEY_M;
    case 'P': case 'p': return KEY_P;
    case '1': return KEY_1;
    case '2': return KEY_2;
    case '3': return KEY_3;
    case '4': return KEY_4;
    case '5': return KEY_5;
    case ' ': return KEY_SPACE;
    default: return KEY_NONE;
  }
}

#if defined(__linux__)
Display* g_dpy = nullptr;
Window g_win = 0;
GC g_gc = nullptr;
XImage* g_img = nullptr;
Atom g_wmDelete = 0;
std::vector<uint32_t> g_blitBuf;
int g_winW = 0, g_winH = 0;
bool g_warping = false;

Key mapKeysym(KeySym ks) {
  switch (ks) {
    case XK_Escape: return KEY_ESC;
    case XK_space: return KEY_SPACE;
    case XK_Shift_L: case XK_Shift_R: return KEY_SHIFT;
    case XK_Control_L: case XK_Control_R: return KEY_CTRL;
    case XK_Tab: return KEY_TAB;
    case XK_Return: return KEY_ENTER;
    case XK_BackSpace: return KEY_BACKSPACE;
    case XK_Up: return KEY_UP;
    case XK_Down: return KEY_DOWN;
    case XK_Left: return KEY_LEFT;
    case XK_Right: return KEY_RIGHT;
    default: break;
  }
  if (ks >= 'a' && ks <= 'z') return mapAscii(ks - 32);
  if (ks >= 'A' && ks <= 'Z') return mapAscii((int)ks);
  return mapAscii((int)(ks & 0xff));
}

void centrePointer() {
  g_warping = true;
  XWarpPointer(g_dpy, None, g_win, 0, 0, 0, 0, g_winW / 2, g_winH / 2);
  XFlush(g_dpy);
  g_warping = false;
}
#endif

#if defined(_WIN32)
HWND g_hwnd = nullptr;
HDC g_hdc = nullptr;
BITMAPINFO g_bmi{};
std::vector<uint32_t> g_blitBuf;
int g_winW = 0, g_winH = 0;
bool g_centreing = false;
LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
#endif

}  // namespace

bool init(const Config& cfg, Input& input) {
  g_cfg = cfg;
  g_headless = cfg.headless;
  g_startTime = nowSecondsImpl();
  g_input = &input;
  if (g_headless) return true;

#if defined(_WIN32)
  WNDCLASSA wc{};
  wc.lpfnWndProc = wndProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = "KayKitNativeWindow";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassA(&wc);
  RECT rc{0, 0, cfg.windowW, cfg.windowH};
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
  g_hwnd = CreateWindowA("KayKitNativeWindow", cfg.title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                         nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  if (!g_hwnd) return false;
  g_hdc = GetDC(g_hwnd);
  g_winW = cfg.windowW;
  g_winH = cfg.windowH;
  g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  g_bmi.bmiHeader.biWidth = g_winW;
  g_bmi.bmiHeader.biHeight = -g_winH;   // top-down
  g_bmi.bmiHeader.biPlanes = 1;
  g_bmi.bmiHeader.biBitCount = 32;
  g_bmi.bmiHeader.biCompression = BI_RGB;
  g_blitBuf.assign((size_t)g_winW * g_winH, 0);
  ShowWindow(g_hwnd, SW_SHOW);
  if (cfg.grabMouse) {
    ShowCursor(FALSE);
    SetCapture(g_hwnd);
    RECT rc2;
    GetClientRect(g_hwnd, &rc2);
    POINT c{(rc2.right - rc2.left) / 2, (rc2.bottom - rc2.top) / 2};
    ClientToScreen(g_hwnd, &c);
    SetCursorPos(c.x, c.y);
  }
  return true;

#elif defined(__linux__)
  g_dpy = XOpenDisplay(nullptr);
  if (!g_dpy) {
    std::fprintf(stderr, "[platform] no X display — falling back to headless\n");
    g_headless = true;
    return true;
  }
  int screen = DefaultScreen(g_dpy);
  g_winW = cfg.windowW;
  g_winH = cfg.windowH;
  g_win = XCreateSimpleWindow(g_dpy, RootWindow(g_dpy, screen), 0, 0, g_winW, g_winH, 1,
                              BlackPixel(g_dpy, screen), BlackPixel(g_dpy, screen));
  XStoreName(g_dpy, g_win, cfg.title);
  XSelectInput(g_dpy, g_win, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                                ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask);
  g_wmDelete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_dpy, g_win, &g_wmDelete, 1);
  g_gc = XCreateGC(g_dpy, g_win, 0, nullptr);
  XMapWindow(g_dpy, g_win);

  g_blitBuf.assign((size_t)g_winW * g_winH, 0);
  g_img = XCreateImage(g_dpy, DefaultVisual(g_dpy, screen), 24, ZPixmap, 0,
                       (char*)g_blitBuf.data(), g_winW, g_winH, 32, 0);
  XFlush(g_dpy);
  if (cfg.grabMouse) {
    XGrabPointer(g_dpy, g_win, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, g_win, None, CurrentTime);
    centrePointer();
  }
  return true;
#else
  g_headless = true;
  return true;
#endif
}


// ---------------------------------------------------------------------------
//  Gamepad support. XInput on Windows, the Linux joystick API elsewhere.
//  Both normalise into the same Input structure the keyboard/mouse fill, so
//  the game code never needs to know which device is being used.
//    L-stick move · R-stick camera · D-pad L/R camera, up elixir
//    A dodge · X/RT attack · LT block · Y emberbolt · LB elixir
//    RB sprint · B interact · R3 lock-on · Start pause
// ---------------------------------------------------------------------------
struct PadState {
  bool connected = false;
  bool down[16] = {false};
  bool prev[16] = {false};
  float axis[6] = {0, 0, 0, 0, 0, 0};
  std::string name;
};

PadState g_pad;
double g_padPollTime = 0;

float padDeadzone(float v, float dz = 0.18f) {
  float a = std::fabs(v);
  if (a < dz) return 0.f;
  float s = v < 0 ? -1.f : 1.f;
  return s * std::min(1.f, (a - dz) / (1.f - dz));
}

void padApplyToInput(Input& in) {
  if (!g_pad.connected) return;
  const PadState& p = g_pad;
  float lx = padDeadzone(p.axis[0]), ly = padDeadzone(p.axis[1]);
  float rx = padDeadzone(p.axis[2]), ry = padDeadzone(p.axis[3]);

  // left stick -> WASD
  if (ly < -0.35f) in.padDown[KEY_W] = true;
  if (ly > 0.35f) in.padDown[KEY_S] = true;
  if (lx < -0.35f) in.padDown[KEY_A] = true;
  if (lx > 0.35f) in.padDown[KEY_D] = true;
  if (std::fabs(lx) > 0.35f || std::fabs(ly) > 0.35f) in.padActive = true;

  // right stick -> camera look (mouse deltas are what the camera reads)
  if (rx != 0.f || ry != 0.f) {
    static double lastMove = 0;
    double now = nowSecondsImpl();
    float dt = (float)std::min(0.05, std::max(0.0, now - lastMove));
    if (lastMove == 0) dt = 0.016f;
    lastMove = now;
    in.mouseDX += rx * 620.f * dt;
    in.mouseDY += ry * 420.f * dt;
    in.padActive = true;
  }

  // buttons
  auto holdKey = [&](int b, Key k) {
    if (p.down[b]) { in.padDown[k] = true; in.padActive = true; }
  };
  auto tapKey = [&](int b, Key k) {
    if (p.down[b]) { in.padDown[k] = true; in.padActive = true; }
    if (p.down[b] && !p.prev[b]) in.padPressed[k] = true;
  };
  tapKey(0, KEY_SPACE);           // A    dodge / jump
  tapKey(1, KEY_F);               // B    interact
  tapKey(3, KEY_Q);               // Y    emberbolt
  tapKey(4, KEY_E);               // LB   elixir
  tapKey(12, KEY_E);              // D-pad up  elixir
  tapKey(9, KEY_ESC);             // Start pause
  tapKey(11, KEY_TAB);            // R3   lock-on
  holdKey(5, KEY_SHIFT);          // RB   sprint
  if (p.down[14]) { in.padDown[KEY_LEFT] = true; in.padActive = true; }
  if (p.down[15]) { in.padDown[KEY_RIGHT] = true; in.padActive = true; }

  // X / RT attack, LT block -> mouse buttons
  if (p.down[2] || p.down[7]) { in.padMouseDown[0] = true; in.padActive = true; }
  if ((p.down[2] && !p.prev[2]) || (p.down[7] && !p.prev[7])) in.padMousePressed[0] = true;
  if (p.down[6]) { in.padMouseDown[1] = true; in.padActive = true; }

  for (int i = 0; i < 16; i++) g_pad.prev[i] = p.down[i];
}

#if defined(_WIN32)
void padPoll() {
  XINPUT_STATE st{};
  static int idx = -1;
  if (idx < 0) {
    for (int i = 0; i < 4; i++) {
      if (XInputGetState(i, &st) == ERROR_SUCCESS) { idx = i; break; }
    }
  }
  if (idx < 0) { g_pad.connected = false; return; }
  if (XInputGetState(idx, &st) != ERROR_SUCCESS) {
    idx = -1;
    g_pad.connected = false;
    return;
  }
  if (!g_pad.connected) { g_pad.connected = true; g_pad.name = "XInput controller"; }
  const XINPUT_GAMEPAD& g = st.Gamepad;
  auto ax = [](SHORT v) { return (float)v / 32767.f; };
  g_pad.axis[0] = ax(g.sThumbLX);
  g_pad.axis[1] = ax(g.sThumbLY);
  g_pad.axis[2] = ax(g.sThumbRX);
  g_pad.axis[3] = ax(g.sThumbRY);
  g_pad.axis[4] = (float)g.bLeftTrigger / 255.f;
  g_pad.axis[5] = (float)g.bRightTrigger / 255.f;
  bool b[16] = {false};
  b[0] = (g.wButtons & XINPUT_GAMEPAD_A) != 0;
  b[1] = (g.wButtons & XINPUT_GAMEPAD_B) != 0;
  b[2] = (g.wButtons & XINPUT_GAMEPAD_X) != 0;
  b[3] = (g.wButtons & XINPUT_GAMEPAD_Y) != 0;
  b[4] = (g.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
  b[5] = (g.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
  b[6] = g.bLeftTrigger > 60;
  b[7] = g.bRightTrigger > 60;
  b[8] = (g.wButtons & XINPUT_GAMEPAD_BACK) != 0;
  b[9] = (g.wButtons & XINPUT_GAMEPAD_START) != 0;
  b[11] = (g.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
  b[12] = (g.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
  b[13] = (g.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
  b[14] = (g.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
  b[15] = (g.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
  for (int i = 0; i < 16; i++) g_pad.down[i] = b[i];
}
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
int g_jsfd = -1;
char g_jsbuf[64];
int g_jsbufLen = 0;

void padPoll() {
  double now = nowSecondsImpl();
  if (g_jsfd < 0) {
    if (now - g_padPollTime < 1.5) { g_pad.connected = false; return; }
    g_padPollTime = now;
    for (int i = 0; i < 4; i++) {
      char path[64];
      std::snprintf(path, sizeof(path), "/dev/input/js%d", i);
      int fd = open(path, O_RDONLY | O_NONBLOCK);
      if (fd >= 0) {
        g_jsfd = fd;
        g_pad.connected = true;
        g_pad.name = path;
        g_jsbufLen = 0;
        std::printf("[platform] gamepad: %s\\n", path);
        break;
      }
    }
    if (g_jsfd < 0) { g_pad.connected = false; return; }
  }
  // js_event: __u32 time; __s16 value; __u8 type; __u8 number;
  for (;;) {
    ssize_t n = read(g_jsfd, g_jsbuf + g_jsbufLen, sizeof(g_jsbuf) - g_jsbufLen);
    if (n <= 0) break;
    g_jsbufLen += (int)n;
    while (g_jsbufLen >= 8) {
      int16_t value;
      std::memcpy(&value, g_jsbuf + 4, 2);
      uint8_t type = (uint8_t)g_jsbuf[6];
      uint8_t num = (uint8_t)g_jsbuf[7];
      bool init = (type & 0x80) != 0;
      bool isAxis = (type & 0x7f) == 0x02;
      bool isBtn = (type & 0x7f) == 0x01;
      if (isAxis && num < 6) g_pad.axis[num] = (float)value / 32767.f;
      if (isBtn && num < 16 && !init) {
        g_pad.down[num] = value != 0;
      }
      std::memmove(g_jsbuf, g_jsbuf + 8, (size_t)(g_jsbufLen - 8));
      g_jsbufLen -= 8;
    }
  }
}
#else
void padPoll() { g_pad.connected = false; }
#endif

void pollGamepad(Input& in) {
  padPoll();
  padApplyToInput(in);
}

void pump(Input& in) {
  if (g_headless) {
    return;
  }
  pollGamepad(in);
#if defined(_WIN32)
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
#elif defined(__linux__)
  if (!g_dpy) return;
  alignas(16) static unsigned char evbuf[sizeof(XEvent) + 64];
  while (XPending(g_dpy)) {
    XEvent& ev = *reinterpret_cast<XEvent*>(evbuf);
    std::memset(evbuf, 0, sizeof(XEvent));
    XNextEvent(g_dpy, &ev);
    switch (ev.type) {
      case KeyPress:
      case KeyRelease: {
        KeySym ks = XLookupKeysym(&ev.xkey, 0);
        Key k = mapKeysym(ks);
        if (k != KEY_NONE) {
          bool isDown = ev.type == KeyPress;
          if (isDown && !in.down[k]) in.pressed[k] = true;
          in.down[k] = isDown;
        }
        break;
      }
      case ButtonPress:
      case ButtonRelease: {
        int b = ev.xbutton.button;
        int idx = b == 1 ? 0 : (b == 3 ? 1 : (b == 2 ? 2 : -1));
        if (idx >= 0) {
          bool isDown = ev.type == ButtonPress;
          if (isDown && !in.mouseDown[idx]) in.mousePressed[idx] = true;
          in.mouseDown[idx] = isDown;
        }
        break;
      }
      case MotionNotify: {
        if (g_warping) break;
        int cx = g_winW / 2, cy = g_winH / 2;
        int dx = ev.xmotion.x - cx, dy = ev.xmotion.y - cy;
        if (std::abs(dx) < g_winW / 2 - 4 && std::abs(dy) < g_winH / 2 - 4) {
          in.mouseDX += (float)dx;
          in.mouseDY += (float)dy;
        }
        if (g_cfg.grabMouse) centrePointer();
        break;
      }
      case FocusOut: in.focus = false; break;
      case FocusIn: in.focus = true; break;
      case ClientMessage:
        if ((Atom)ev.xclient.data.l[0] == g_wmDelete) in.quit = true;
        break;
      case DestroyNotify: in.quit = true; break;
      default: break;
    }
  }
#endif
}

void present(Renderer& r) {
  if (g_headless) return;
  r.blitScaled(g_blitBuf, g_winW, g_winH);
#if defined(_WIN32)
  StretchDIBits(g_hdc, 0, 0, g_winW, g_winH, 0, 0, g_winW, g_winH, g_blitBuf.data(), &g_bmi,
                DIB_RGB_COLORS, SRCCOPY);
#elif defined(__linux__)
  if (!g_dpy || !g_img) return;
  // the X server wants its own buffer, so hand it ours (restore after)
  g_img->data = (char*)g_blitBuf.data();
  XPutImage(g_dpy, g_win, g_gc, g_img, 0, 0, 0, 0, g_winW, g_winH);
  g_img->data = (char*)g_blitBuf.data();
  XFlush(g_dpy);
#endif
}

void setTitle(const char* t) {
#if defined(_WIN32)
  if (g_hwnd) SetWindowTextA(g_hwnd, t);
#elif defined(__linux__)
  if (g_dpy && g_win) XStoreName(g_dpy, g_win, t);
#else
  (void)t;
#endif
}

void shutdown() {
  if (g_headless) return;
#if defined(_WIN32)
  if (g_hdc) ReleaseDC(g_hwnd, g_hdc);
  if (g_hwnd) DestroyWindow(g_hwnd);
  g_hwnd = nullptr;
#elif defined(__linux__)
  if (g_dpy) {
    if (g_img) {
      g_img->data = nullptr;
      XDestroyImage(g_img);
      g_img = nullptr;
    }
    if (g_gc) XFreeGC(g_dpy, g_gc);
    if (g_win) XDestroyWindow(g_dpy, g_win);
    XUngrabPointer(g_dpy, CurrentTime);
    XCloseDisplay(g_dpy);
    g_dpy = nullptr;
  }
#endif
}

bool isHeadless() { return g_headless; }

void setMouseCapture(bool on) {
  if (g_headless) return;
  g_cfg.grabMouse = on;
#if defined(__linux__)
  if (!g_dpy || !g_win) return;
  if (on) {
    XGrabPointer(g_dpy, g_win, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, g_win, None, CurrentTime);
    centrePointer();
  } else {
    XUngrabPointer(g_dpy, CurrentTime);
  }
  XFlush(g_dpy);
#elif defined(_WIN32)
  if (on) {
    ShowCursor(FALSE);
    SetCapture(g_hwnd);
  } else {
    ShowCursor(TRUE);
    ReleaseCapture();
  }
#else
  (void)on;
#endif
}

bool mouseCaptured() {
  if (g_headless) return false;
  return g_cfg.grabMouse;
}

double nowSeconds() { return nowSecondsImpl(); }

void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

double tickFPS() {
  g_fpsFrames++;
  g_fpsAccum += 1.0 / 60.0;  // unused; FPS is measured by the caller
  return g_lastFPS;
}

#if defined(_WIN32)
namespace {
bool keyFromWParam(WPARAM wp, Key& out) {
  switch (wp) {
    case VK_ESCAPE: out = KEY_ESC; return true;
    case VK_SPACE: out = KEY_SPACE; return true;
    case VK_SHIFT: out = KEY_SHIFT; return true;
    case VK_CONTROL: out = KEY_CTRL; return true;
    case VK_TAB: out = KEY_TAB; return true;
    case VK_RETURN: out = KEY_ENTER; return true;
    case VK_UP: out = KEY_UP; return true;
    case VK_DOWN: out = KEY_DOWN; return true;
    case VK_LEFT: out = KEY_LEFT; return true;
    case VK_RIGHT: out = KEY_RIGHT; return true;
    default: break;
  }
  if (wp >= 'A' && wp <= 'Z') { out = mapAscii((int)wp); return true; }
  if (wp >= '1' && wp <= '5') { out = mapAscii((int)wp); return true; }
  return false;
}

// NOTE: wndProc must stay inside this anonymous namespace so it matches the
// forward declaration above; linking it as plat::wndProc leaves the one the
// window class points at undefined (an MSVC/mingw link error).
LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
  static POINT lastPt{0, 0};
  static bool haveLast = false;
  Input* in = g_input;
  switch (msg) {
    case WM_CLOSE:
      if (in) in->quit = true;
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_SETFOCUS: if (in) in->focus = true; return 0;
    case WM_KILLFOCUS: if (in) in->focus = false; return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      if (!in) return 0;
      Key k;
      if (keyFromWParam(wp, k)) {
        bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        if (isDown && !in->down[k]) in->pressed[k] = true;
        in->down[k] = isDown;
      }
      return 0;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: {
      if (!in) return 0;
      int idx = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) ? 0 : 1;
      bool isDown = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN);
      if (isDown && !in->mouseDown[idx]) in->mousePressed[idx] = true;
      in->mouseDown[idx] = isDown;
      if (isDown) SetCapture(h); else ReleaseCapture();
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (!in) return 0;
      POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      if (g_cfg.grabMouse) {
        RECT rc;
        GetClientRect(h, &rc);
        int cx = (rc.right - rc.left) / 2, cy = (rc.bottom - rc.top) / 2;
        int dx = p.x - lastPt.x, dy = p.y - lastPt.y;
        // ignore the jump produced by our own recentre
        if (std::abs(dx) < cx - 4 && std::abs(dy) < cy - 4) {
          in->mouseDX += (float)dx;
          in->mouseDY += (float)dy;
        }
        POINT c{cx, cy};
        ClientToScreen(h, &c);
        SetCursorPos(c.x, c.y);
        lastPt = POINT{cx, cy};
        haveLast = true;
        return 0;
      }
      if (haveLast && in->focus) {
        in->mouseDX += (float)(p.x - lastPt.x);
        in->mouseDY += (float)(p.y - lastPt.y);
      }
      lastPt = p;
      haveLast = true;
      return 0;
    }
    default: break;
  }
  return DefWindowProcA(h, msg, wp, lp);
}
}  // namespace
#endif

}  // namespace plat
