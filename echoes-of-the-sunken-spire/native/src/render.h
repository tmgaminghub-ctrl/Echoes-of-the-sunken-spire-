// ---------------------------------------------------------------------------
//  render.h — multithreaded software rasterizer (colour + depth + textured)
// ---------------------------------------------------------------------------
#pragma once
#include "base.h"
#include "gltf.h"

struct Camera {
  V3 pos{0, 8, 20};
  V3 target{0, 1, 0};
  float fov = rad(58.f);
  float aspect = 16.f / 9.f;
  float zn = 0.12f, zf = 400.f;
  Mat4 view, proj;
  float frustum[6][4] = {};   // world-space planes (a,b,c,d) filled by update()
  void update() {
    view = Mat4::lookAt(pos, target, V3(0, 1, 0));
    proj = Mat4::perspective(fov, aspect, zn, zf);
  }
};

struct DrawItem {
  const Mesh* mesh = nullptr;
  const Material* mat = nullptr;
  const Image* tex = nullptr;
  Mat4 model;
  const std::vector<Mat4>* joints = nullptr;   // non-null for skinned meshes
  V3 tint{1, 1, 1};
  float extraLight = 0.f;                       // self-illumination boost
};

class Renderer {
 public:
  int W = 640, H = 360;
  std::vector<uint32_t> color;
  std::vector<float> depth;
  Camera cam;

  // scene look
  V3 skyTop{0.35f, 0.55f, 0.85f};
  V3 skyBot{0.72f, 0.82f, 0.92f};
  V3 fog{0.72f, 0.82f, 0.92f};
  float fogDensity = 0.0045f;
  V3 sunDir = normalize(V3(-0.45f, 1.0f, 0.35f));
  V3 sunColor{1.0f, 0.96f, 0.86f};
  float sunPower = 1.0f;
  V3 ambient{0.42f, 0.46f, 0.55f};
  float exposure = 1.0f;
  bool toneMap = true;      // ACES-ish filmic curve (matches the WebGL build)
  uint32_t frames = 0;
  long long submitted = 0, tris = 0;
  long long culledFrustum = 0, queued = 0;
  int threads = 1;
  double lastMs = 0;
  std::vector<DrawItem> queue;
  int bandCount = 1;

  void resize(int w, int h);
  void beginFrame();
  void submit(const DrawItem& d);   // queue a draw item (culled here)
  void rasterize();                 // flush the queue, threaded over screen bands
  void rasterizeItem(const DrawItem& d, int bandY0, int bandY1);
  void endFrame();

  // ---- 2D overlay (drawn after endFrame, no depth) ----
  void rect(int x, int y, int w, int h, uint32_t rgba);
  void rectOutline(int x, int y, int w, int h, uint32_t rgba);
  void text(int x, int y, const char* s, uint32_t rgba, int scale = 1);
  void textCenter(int cx, int y, const char* s, uint32_t rgba, int scale = 1);
  int textWidth(const char* s, int scale = 1) const;
  void line(int x0, int y0, int x1, int y1, uint32_t rgba);
  void circle(int cx, int cy, int r, uint32_t rgba, bool filled = true);
  void blitScaled(std::vector<uint32_t>& dst, int dstW, int dstH);
};

inline uint32_t rgba32(int r, int g, int b, int a = 255) {
  return (uint32_t)(r & 0xff) | ((uint32_t)(g & 0xff) << 8) | ((uint32_t)(b & 0xff) << 16) | ((uint32_t)(a & 0xff) << 24);
}
inline uint32_t rgbaf(float r, float g, float b, float a = 1.f) {
  return rgba32((int)(clampf(r, 0, 1) * 255), (int)(clampf(g, 0, 1) * 255), (int)(clampf(b, 0, 1) * 255), (int)(clampf(a, 0, 1) * 255));
}
