// ---------------------------------------------------------------------------
//  render.cpp — the rasterizer
// ---------------------------------------------------------------------------
#include "render.h"
#include "font_data.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

void Renderer::resize(int w, int h) {
  W = w;
  H = h;
  threads = (int)std::thread::hardware_concurrency();
  if (threads < 1) threads = 1;
  if (threads > 12) threads = 12;
  if (getenv("KK_THREADS")) threads = std::max(1, atoi(getenv("KK_THREADS")));
  bandCount = threads;   // triangle setup is duplicated per band, so keep bands == threads
  if (bandCount < 1) bandCount = 1;
  queue.reserve(1024);
  if (getenv("KK_THREAD_INFO"))
    std::printf("[threads] %d bands=%d\n", threads, bandCount);
  color.assign((size_t)W * H, 0xff000000u);
  depth.assign((size_t)W * H, 0.f);
  cam.aspect = (float)W / (float)H;
}

void Renderer::beginFrame() {
  auto t0 = std::chrono::high_resolution_clock::now();
  (void)t0;
  frames++;
  queued = 0;
  culledFrustum = 0;
  queue.clear();
  std::fill(depth.begin(), depth.end(), 1e30f);
  // sky gradient
  for (int y = 0; y < H; y++) {
    float t = (float)y / (float)H;
    float k = std::pow(clampf(t * 1.25f, 0.f, 1.f), 1.15f);
    uint32_t c = rgbaf(lerpf(skyTop.x, skyBot.x, k), lerpf(skyTop.y, skyBot.y, k), lerpf(skyTop.z, skyBot.z, k));
    uint32_t* row = &color[(size_t)y * W];
    std::fill(row, row + W, c);
  }
  cam.update();
  // extract the 6 frustum planes from proj*view for cheap item culling
  {
    const Mat4& P = cam.proj;
    const Mat4& V = cam.view;
    Mat4 M = P * V;
    auto ext = [&](int r) { return V4(M.m[r], M.m[4 + r], M.m[8 + r], M.m[12 + r]); };
    V4 rows[4] = {ext(0), ext(1), ext(2), ext(3)};
    auto addv = [](const V4& a, const V4& b, float sb) {
      return V4(a.x + b.x * sb, a.y + b.y * sb, a.z + b.z * sb, a.w + b.w * sb);
    };
    V4 planes[6] = {
      addv(rows[3], rows[0], 1.f),   // left
      addv(rows[3], rows[0], -1.f),  // right
      addv(rows[3], rows[1], 1.f),   // bottom
      addv(rows[3], rows[1], -1.f),  // top
      addv(rows[3], rows[2], 1.f),   // near
      addv(rows[3], rows[2], -1.f),  // far
    };
    for (int i = 0; i < 6; i++) {
      float len = std::sqrt(planes[i].x * planes[i].x + planes[i].y * planes[i].y +
                            planes[i].z * planes[i].z);
      if (len < 1e-9f) len = 1.f;
      cam.frustum[i][0] = planes[i].x / len;
      cam.frustum[i][1] = planes[i].y / len;
      cam.frustum[i][2] = planes[i].z / len;
      cam.frustum[i][3] = planes[i].w / len;
    }
  }
}

long long g_tris = 0, g_px = 0, g_tested = 0;
static thread_local long long t_tris = 0, t_px = 0, t_tested = 0;   // per-band accumulators

namespace {

struct SVert {
  V3 world;
  V3 nrm;
  V3 col{1, 1, 1};
  float u = 0, v = 0;
  V4 clip;
};

inline SVert makeVert(const V3& p, const V3& n, float u, float v, const Mat4& mv, const Mat4& proj) {
  SVert s;
  s.world = p;
  s.nrm = n;
  s.u = u;
  s.v = v;
  s.clip = proj.mulVec4(V4(mv.mulPoint(p), 1.f));
  return s;
}

inline SVert mixVert(const SVert& a, const SVert& b, float t) {
  SVert r;
  r.world = lerp(a.world, b.world, t);
  r.nrm = lerp(a.nrm, b.nrm, t);
  r.col = lerp(a.col, b.col, t);
  r.u = lerpf(a.u, b.u, t);
  r.v = lerpf(a.v, b.v, t);
  r.clip.x = lerpf(a.clip.x, b.clip.x, t);
  r.clip.y = lerpf(a.clip.y, b.clip.y, t);
  r.clip.z = lerpf(a.clip.z, b.clip.z, t);
  r.clip.w = lerpf(a.clip.w, b.clip.w, t);
  return r;
}

}  // namespace

void Renderer::submit(const DrawItem& d) {
  if (!d.mesh || d.mesh->idx.size() < 3) return;
  // ---- frustum cull against the mesh's world-space bounding sphere ----
  {
    const AABB& bb = d.mesh->bounds;
    V3 c = d.model.mulPoint((bb.mn + bb.mx) * 0.5f);
    V3 e = (bb.mx - bb.mn) * 0.5f;
    float rad = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z) * 1.02f + 0.05f;
    for (int i = 0; i < 6; i++) {
      const float* pl = cam.frustum[i];
      if (pl[0] * c.x + pl[1] * c.y + pl[2] * c.z + pl[3] < -rad) {
        culledFrustum++;
        return;
      }
    }
  }
  if (queue.size() >= queue.capacity() - 1) queue.reserve(queue.size() * 2 + 64);
  queue.push_back(d);
  queued++;
}

// rasterize one queued item, restricted to rows [bandY0, bandY1)
void Renderer::rasterizeItem(const DrawItem& d, int bandY0, int bandY1) {
  if (!d.mesh || d.mesh->idx.size() < 3) return;
  const Mesh& m = *d.mesh;
  const Mat4& VP = cam.proj;
  const Mat4& MV = cam.view;
  Mat4 mvp = cam.proj * (cam.view * d.model);
  (void)VP;

  // material / texture
  V3 baseColor = d.mat ? d.mat->color : V3(1, 1, 1);
  baseColor = baseColor * d.tint;
  float alpha = d.mat ? d.mat->alpha : 1.f;
  bool alphaCut = d.mat ? d.mat->alphaCut : false;
  if (getenv("KK_NOALPHACUT")) alphaCut = false;
  V3 emissive = d.mat ? d.mat->emissive : V3(0, 0, 0);
  const Image* tex = d.tex && !d.tex->empty() ? d.tex : nullptr;

  // transform vertices
  const size_t vcount = m.pos.size();
  static thread_local std::vector<SVert> verts;
  static thread_local std::vector<V3> skinnedPos;
  static thread_local std::vector<V3> skinnedNrm;
  verts.resize(vcount);

  const std::vector<Mat4>* joints = d.joints;
  static int noSkin = getenv("KK_NOSKIN") ? 1 : 0;
  if (m.skinned && joints && joints->size() >= 1 && !noSkin) {
    skinnedPos.resize(vcount);
    skinnedNrm.resize(vcount);
    const auto& J = *joints;
    for (size_t i = 0; i < vcount; i++) {
      const V4& ji = m.joints[i];
      const V4& w = m.weights[i];
      V3 p(0, 0, 0), n(0, 0, 0);
      const V3& sp = m.pos[i];
      const V3& sn = m.nrm[i];
      for (int k = 0; k < 4; k++) {
        int idx = (int)(k == 0 ? ji.x : k == 1 ? ji.y : k == 2 ? ji.z : ji.w);
        float weight = k == 0 ? w.x : k == 1 ? w.y : k == 2 ? w.z : w.w;
        if (weight <= 0.f) continue;
        if (idx < 0 || idx >= (int)J.size()) continue;
        p += J[idx].mulPoint(sp) * weight;
        n += J[idx].mulDir(sn) * weight;
      }
      skinnedPos[i] = p;
      skinnedNrm[i] = n;
      verts[i].world = d.model.mulPoint(p);
      verts[i].nrm = d.model.mulDir(n);
      verts[i].col = m.col.empty() ? V3(1, 1, 1) : m.col[i];
      verts[i].u = m.uv[i].x;
      verts[i].v = m.uv[i].y;
      verts[i].clip = mvp.mulVec4(V4(p, 1.f));
    }
  } else {
    for (size_t i = 0; i < vcount; i++) {
      verts[i].world = d.model.mulPoint(m.pos[i]);
      verts[i].nrm = d.model.mulDir(m.nrm[i]);
      verts[i].col = m.col.empty() ? V3(1, 1, 1) : m.col[i];
      verts[i].u = m.uv[i].x;
      verts[i].v = m.uv[i].y;
      verts[i].clip = mvp.mulVec4(V4(m.pos[i], 1.f));
    }
  }

  const V3 sun = sunDir;
  const float invW2 = 1.0f / (float)(W * H);

  auto shade = [&](const V3& worldPos, V3 n, float u, float v, const V3& vcol, V3& outCol,
                   float lod = 0.f) {
    n = normalize(n);
    float diff = std::max(0.f, dot(n, sun));
    // cheap half-lambert wrap keeps the shadow side readable
    float wrap = 0.35f + 0.65f * diff;
    V3 lit = ambient * 0.9f + sunColor * (sunPower * wrap);
    V3 texel(1, 1, 1);
    if (tex) {
      uint32_t c = tex->sampleBilinear(u, v, lod);
      texel = V3((c & 0xff) / 255.f, ((c >> 8) & 0xff) / 255.f, ((c >> 16) & 0xff) / 255.f);
    }
    outCol = (texel * baseColor * vcol * lit) + emissive * (1.f + d.extraLight);
    // distance fog
    float dist = length(worldPos - cam.pos);
    float f = 1.f - std::exp(-dist * fogDensity);
    outCol = lerp(outCol, fog, clampf(f, 0, 1));
    outCol = outCol * exposure;
    if (toneMap) {
      // ACES-ish filmic curve, the same one three.js applies in the browser
      // (ACESFilmicToneMapping); without it the native colours read flat and
      // the highlights clip hard.
      auto curve = [](float x) {
        float v = (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
        return clampf(v, 0.f, 1.f);
      };
      outCol.x = curve(outCol.x); outCol.y = curve(outCol.y); outCol.z = curve(outCol.z);
    }
  };

  auto texAlpha = [&](float u, float v) -> float {
    if (!tex) return 1.f;
    uint32_t c = tex->sample(u, v);
    return ((c >> 24) & 0xff) / 255.f;
  };

  // For each triangle: clip against near plane, then rasterize.
  auto project = [&](const SVert& s, float& sx, float& sy, float& sw) {
    float iw = 1.f / s.clip.w;
    sx = (s.clip.x * iw * 0.5f + 0.5f) * (float)W;
    sy = (0.5f - s.clip.y * iw * 0.5f) * (float)H;
    sw = iw;
  };

  static thread_local std::vector<SVert> poly;
  static thread_local std::vector<SVert> clipbuf;
  for (size_t t = 0; t < m.idx.size(); t += 3) {
    SVert tri[3];
    for (int k = 0; k < 3; k++) {
      uint32_t vi = m.idx[t + k];
      if (vi >= verts.size()) { tri[k] = verts[0]; continue; }
      tri[k] = verts[vi];
    }
    // near-plane clip (w > eps)
    const float NEARW = 0.001f;
    poly.clear();
    int insideCount = 0;
    for (int k = 0; k < 3; k++) if (tri[k].clip.w > NEARW) insideCount++;
    if (insideCount == 0) continue;
    if (insideCount == 3) {
      poly.push_back(tri[0]);
      poly.push_back(tri[1]);
      poly.push_back(tri[2]);
    } else {
      for (int k = 0; k < 3; k++) {
        const SVert& a = tri[k];
        const SVert& b = tri[(k + 1) % 3];
        bool ain = a.clip.w > NEARW, bin = b.clip.w > NEARW;
        if (ain) poly.push_back(a);
        if (ain != bin) {
          float tt = (NEARW - a.clip.w) / (b.clip.w - a.clip.w);
          poly.push_back(mixVert(a, b, clampf(tt, 0, 1)));
        }
      }
    }
    if (poly.size() < 3) continue;

    for (size_t f = 1; f + 1 < poly.size(); f++) {
      SVert A = poly[0], B = poly[f], C = poly[f + 1];
      float x0, y0, w0, x1, y1, w1, x2, y2, w2;
      project(A, x0, y0, w0);
      project(B, x1, y1, w1);
      project(C, x2, y2, w2);

      // bounding box + offscreen reject
      float minX = std::min(x0, std::min(x1, x2)), maxX = std::max(x0, std::max(x1, x2));
      float minY = std::min(y0, std::min(y1, y2)), maxY = std::max(y0, std::max(y1, y2));
      if (maxX < 0 || minX >= W || maxY < 0 || minY >= H) continue;

      int ix0 = std::max(0, (int)std::floor(minX)), ix1 = std::min(W - 1, (int)std::ceil(maxX));
      int iy0 = std::max(bandY0, (int)std::floor(minY));
      int iy1 = std::min(bandY1 - 1, (int)std::ceil(maxY));
      if (iy0 >= H) continue;
      if (ix0 > ix1 || iy0 > iy1) continue;

      float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
      // sub-pixel triangles: skip early (they only add shimmer and setup cost)
      static float minArea = getenv("KK_MINAREA") ? (float)atof(getenv("KK_MINAREA")) : 0.35f;
      if (std::fabs(area) < minArea) continue;
      // screen space has y going down, so a front (CCW-in-NDC) face has negative area
      bool front = area < 0.f;
      static int noCull = getenv("KK_NOCULL") ? 1 : 0;
      if (!noCull && !front && !(d.mat && d.mat->doubleSided)) continue;
      float invArea = 1.f / area;
      t_tris++;

      // Per-triangle level of detail: how many texels does one screen pixel
      // cover?  Without this the loop point-samples a 1024x1024 atlas and
      // distant geometry shimmers.  Cheap (computed once per triangle).
      float lod = 0.f;
      if (tex && tex->hasMips()) {
        float uvSpan = std::max(std::fabs(A.u - B.u), std::fabs(A.u - C.u)) * (float)tex->w +
                       std::max(std::fabs(A.v - B.v), std::fabs(A.v - C.v)) * (float)tex->h;
        float pxSpan = std::max(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)),
                                std::max(std::fabs(x2 - x0), std::fabs(y2 - y0)));
        if (pxSpan > 0.001f && uvSpan > 0.001f)
          lod = std::log2(uvSpan / pxSpan);
        if (lod < 0.f) lod = 0.f;
        if (lod > 8.f) lod = 8.f;
      }

      // incremental edge functions: everything below varies linearly in x/y
      const float dxa = -(y2 - y1), dxb = -(y0 - y2), dxc = -(y1 - y0);
      const float w0s = w0 * invArea, w1s = w1 * invArea, w2s = w2 * invArea;
      const float dpw = dxa * w0s + dxb * w1s + dxc * w2s;   // d(persp w)/dx
      for (int y = iy0; y <= iy1; y++) {
        float pyc = y + 0.5f;
        float pxc = ix0 + 0.5f;
        float ea = (x2 - x1) * (pyc - y1) - (pxc - x1) * (y2 - y1);
        float eb = (x0 - x2) * (pyc - y2) - (pxc - x2) * (y0 - y2);
        float ec = (x1 - x0) * (pyc - y0) - (pxc - x0) * (y1 - y0);
        float pw = ea * w0s + eb * w1s + ec * w2s;
        uint32_t* crow = &color[(size_t)y * W];
        float* drow = &depth[(size_t)y * W];
        for (int x = ix0; x <= ix1; x++) {
          t_tested++;
          if ((ea < 0 || eb < 0 || ec < 0) && (ea > 0 || eb > 0 || ec > 0)) {
            ea += dxa; eb += dxb; ec += dxc; pw += dpw;
            continue;
          }
          if (pw <= 1e-9f) { ea += dxa; eb += dxb; ec += dxc; pw += dpw; continue; }
          float depthVal = 1.f / pw;   // view-space w: smaller is nearer
          if (depthVal >= drow[x]) {
            ea += dxa; eb += dxb; ec += dxc; pw += dpw;
            continue;
          }
          float b0 = ea * invArea, b1 = eb * invArea, b2 = ec * invArea;
          float iw = 1.f / pw;
          float q0 = b0 * w0 * iw, q1 = b1 * w1 * iw, q2 = b2 * w2 * iw;

          float u = q0 * A.u + q1 * B.u + q2 * C.u;
          float v = q0 * A.v + q1 * B.v + q2 * C.v;
          if (alphaCut && texAlpha(u, v) < 0.5f) {
            ea += dxa; eb += dxb; ec += dxc; pw += dpw;
            continue;
          }

          V3 col;
          static int flatShade = getenv("KK_FLATSHADE") ? 1 : 0;
          if (flatShade) {
            col = tex ? V3((tex->sample(u, v) & 0xff) / 255.f, 1, 1) : V3(1, 1, 1);
          } else {
            V3 wp = A.world * q0 + B.world * q1 + C.world * q2;
            V3 n = A.nrm * q0 + B.nrm * q1 + C.nrm * q2;
            V3 vc = A.col * q0 + B.col * q1 + C.col * q2;
            shade(wp, n, u, v, vc, col, lod);
          }
          drow[x] = depthVal;
          crow[x] = rgbaf(col.x, col.y, col.z, alpha);
          t_px++;
          ea += dxa; eb += dxb; ec += dxc; pw += dpw;
        }
      }
    }
  }
}

void Renderer::rasterize() {
  if (queue.empty()) return;
  auto t0 = std::chrono::high_resolution_clock::now();
  struct Timer {
    std::chrono::high_resolution_clock::time_point t0;
    ~Timer() {
      if (getenv("KK_TIME_RASTER")) {
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::high_resolution_clock::now() - t0).count();
        static double acc = 0; static int n = 0;
        acc += ms; n++;
        if (n % 60 == 0) { std::printf("[raster] %.2f ms/frame px=%lld tested=%lld\n", acc / 60, (long long)g_px, (long long)g_tested); acc = 0; }
      }
    }
  } timer{t0};
  g_tris = 0;
  g_px = 0;
  g_tested = 0;
  int nb = bandCount;
  if (nb <= 1) {
    t_tris = t_px = 0;
    for (auto& d : queue) rasterizeItem(d, 0, H);
    g_tris = t_tris;
    g_px = t_px;
    g_tested = t_tested;
  } else {
    std::mutex m;
    std::vector<long long> bt(nb, 0), bp(nb, 0), btest(nb, 0);
    std::vector<std::thread> pool;
    pool.reserve(nb);
    for (int b = 0; b < nb; b++) {
      int y0 = (int)((long long)H * b / nb), y1 = (int)((long long)H * (b + 1) / nb);
      pool.emplace_back([this, y0, y1, b, &m, &bt, &bp, &btest]() {
        t_tris = t_px = t_tested = 0;
        for (auto& d : queue) rasterizeItem(d, y0, y1);
        std::lock_guard<std::mutex> lk(m);
        bt[b] = t_tris;
        bp[b] = t_px;
        btest[b] = t_tested;
      });
    }
    for (auto& t : pool) t.join();
    for (int b = 0; b < nb; b++) { g_tris += bt[b]; g_px += bp[b]; g_tested += btest[b]; }
  }
  queue.clear();
  submitted = queued;
}

void Renderer::endFrame() { tris = g_tris; }

// ===================== 2D overlay =====================
void Renderer::rect(int x, int y, int w, int h, uint32_t c) {
  int a = (c >> 24) & 0xff;
  if (a == 0) return;
  int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(W, x + w), y1 = std::min(H, y + h);
  for (int yy = y0; yy < y1; yy++) {
    uint32_t* row = &color[(size_t)yy * W];
    for (int xx = x0; xx < x1; xx++) {
      if (a >= 255) { row[xx] = c; continue; }
      uint32_t d = row[xx];
      float t = a / 255.f;
      int r = (int)lerpf((float)(d & 0xff), (float)(c & 0xff), t);
      int g = (int)lerpf((float)((d >> 8) & 0xff), (float)((c >> 8) & 0xff), t);
      int b = (int)lerpf((float)((d >> 16) & 0xff), (float)((c >> 16) & 0xff), t);
      row[xx] = rgba32(r, g, b, 255);
    }
  }
}

void Renderer::rectOutline(int x, int y, int w, int h, uint32_t c) {
  rect(x, y, w, 1, c);
  rect(x, y + h - 1, w, 1, c);
  rect(x, y, 1, h, c);
  rect(x + w - 1, y, 1, h, c);
}

int Renderer::textWidth(const char* s, int scale) const {
  return (int)std::strlen(s) * (FONT_W + 1) * scale;
}

void Renderer::text(int x, int y, const char* s, uint32_t c, int scale) {
  int cx = x;
  for (const char* p = s; *p; p++) {
    unsigned char ch = (unsigned char)*p;
    if (ch == '\n') { cx = x; y += (FONT_H + 2) * scale; continue; }
    if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
    const uint8_t* g = g_font[ch - FONT_FIRST];
    for (int gy = 0; gy < FONT_H; gy++) {
      uint16_t bits = g[gy];
      if (!bits) continue;
      for (int gx = 0; gx < FONT_W; gx++) {
        if (!(bits & (1 << gx))) continue;
        if (scale == 1) rect(cx + gx, y + gy, 1, 1, c);
        else rect(cx + gx * scale, y + gy * scale, scale, scale, c);
      }
    }
    cx += (FONT_W + 1) * scale;
  }
}

void Renderer::textCenter(int cx, int y, const char* s, uint32_t c, int scale) {
  text(cx - textWidth(s, scale) / 2, y, s, c, scale);
}

void Renderer::line(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  while (true) {
    rect(x0, y0, 1, 1, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err * 2;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx) { err += dx; y0 += sy; }
  }
}

void Renderer::circle(int cx, int cy, int r, uint32_t c, bool filled) {
  for (int y = -r; y <= r; y++) {
    for (int x = -r; x <= r; x++) {
      int d2 = x * x + y * y;
      if (d2 > r * r) continue;
      if (!filled && d2 < (r - 1) * (r - 1)) continue;
      rect(cx + x, cy + y, 1, 1, c);
    }
  }
}

// Nearest-neighbour upscale into an arbitrary destination buffer.
void Renderer::blitScaled(std::vector<uint32_t>& dst, int dstW, int dstH) {
  // Bilinear when the internal resolution differs from the window. The old
  // nearest-neighbour blit is what made an upscaled frame look blocky rather
  // than merely soft. At 1:1 this degenerates to a straight copy.
  if (dstW == W && dstH == H) {
    std::memcpy(dst.data(), color.data(), (size_t)W * H * 4);
    return;
  }
  const float sx = (float)W / (float)dstW, sy = (float)H / (float)dstH;
  const bool upscale = sx < 1.0f && sy < 1.0f;
  for (int y = 0; y < dstH; y++) {
    float fy = (y + 0.5f) * sy - 0.5f;
    int y0 = (int)std::floor(fy);
    float ty = fy - (float)y0;
    int y1 = y0 + 1;
    y0 = std::max(0, std::min(H - 1, y0));
    y1 = std::max(0, std::min(H - 1, y1));
    const uint32_t* r0 = &color[(size_t)y0 * W];
    const uint32_t* r1 = &color[(size_t)y1 * W];
    uint32_t* drow = &dst[(size_t)y * dstW];
    if (!upscale) {
      // downscale (supersampling): average the covered source columns
      for (int x = 0; x < dstW; x++) {
        int xa = std::max(0, (int)(x * sx)), xb = std::min(W - 1, (int)((x + 1) * sx));
        uint32_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;
        int n = std::max(1, xb - xa + 1);
        for (int xx = xa; xx <= xb; xx++) {
          uint32_t a = r0[xx], b = r1[xx];
          acc0 += a & 0xff; acc1 += (a >> 8) & 0xff; acc2 += (a >> 16) & 0xff; acc3 += (b >> 8) & 0xff;
        }
        drow[x] = (uint32_t)(acc0 / n) | ((uint32_t)((acc1 / n + acc3 / n) / 2) << 8) |
                  ((uint32_t)(acc2 / n) << 16) | 0xff000000u;
      }
      continue;
    }
    for (int x = 0; x < dstW; x++) {
      float fx = (x + 0.5f) * sx - 0.5f;
      int x0 = (int)std::floor(fx);
      float tx = fx - (float)x0;
      int x1 = x0 + 1;
      x0 = std::max(0, std::min(W - 1, x0));
      x1 = std::max(0, std::min(W - 1, x1));
      uint32_t out = 0;
      for (int sh = 0; sh <= 24; sh += 8) {
        float a0 = (float)((r0[x0] >> sh) & 0xff), a1 = (float)((r0[x1] >> sh) & 0xff);
        float b0 = (float)((r1[x0] >> sh) & 0xff), b1 = (float)((r1[x1] >> sh) & 0xff);
        float top = a0 + (a1 - a0) * tx, bot = b0 + (b1 - b0) * tx;
        int v = (int)(top + (bot - top) * ty + 0.5f);
        out |= (uint32_t)std::max(0, std::min(255, v)) << sh;
      }
      drow[x] = out | 0xff000000u;
    }
  }
}
