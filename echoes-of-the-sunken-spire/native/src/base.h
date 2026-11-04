// ---------------------------------------------------------------------------
//  base.h — math, file IO, JSON, PNG decode, BMP/TGA write
//  Part of the KayKit Native engine (Vaeloria engine).
//  Zero external dependencies except zlib (for PNG).
// ---------------------------------------------------------------------------
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#ifdef KAYKIT_EMBEDDED_ASSETS
#include "vfs.h"   // assets baked into the executable (portable single-file builds)
#endif

// ============================== math =======================================
constexpr float PI_F = 3.14159265358979323846f;
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float deg(float r) { return r * 180.0f / PI_F; }
inline float rad(float d) { return d * PI_F / 180.0f; }

struct V2 {
  float x = 0, y = 0;
  V2() {}
  V2(float x_, float y_) : x(x_), y(y_) {}
  V2 operator+(const V2& o) const { return {x + o.x, y + o.y}; }
  V2 operator-(const V2& o) const { return {x - o.x, y - o.y}; }
  V2 operator*(float s) const { return {x * s, y * s}; }
};

struct V3 {
  float x = 0, y = 0, z = 0;
  V3() {}
  V3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  explicit V3(float s) : x(s), y(s), z(s) {}
  V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  V3 operator*(float s) const { return {x * s, y * s, z * s}; }
  V3 operator*(const V3& o) const { return {x * o.x, y * o.y, z * o.z}; }
  V3 operator/(float s) const { return {x / s, y / s, z / s}; }
  V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const V3& v) { return std::sqrt(dot(v, v)); }
inline float lengthXZ(const V3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }
inline V3 normalize(const V3& v) { float l = length(v); return l > 1e-8f ? v / l : V3(0, 0, 0); }
inline V3 lerp(const V3& a, const V3& b, float t) { return a + (b - a) * t; }
inline float distXZ(const V3& a, const V3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }

struct V4 {
  float x = 0, y = 0, z = 0, w = 0;
  V4() {}
  V4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  V4(const V3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct Quat {
  float x = 0, y = 0, z = 0, w = 1;
  Quat() {}
  Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};
inline Quat qFromEulerXYZ(float rx, float ry, float rz) {
  float cx = std::cos(rx * 0.5f), sx = std::sin(rx * 0.5f);
  float cy = std::cos(ry * 0.5f), sy = std::sin(ry * 0.5f);
  float cz = std::cos(rz * 0.5f), sz = std::sin(rz * 0.5f);
  return {sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz,
          cx * cy * sz + sx * sy * cz, cx * cy * cz - sx * sy * sz};
}
inline Quat qFromAxisAngle(const V3& a, float ang) {
  V3 n = normalize(a);
  float s = std::sin(ang * 0.5f);
  return {n.x * s, n.y * s, n.z * s, std::cos(ang * 0.5f)};
}
inline Quat qMul(const Quat& a, const Quat& b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
          a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat qNormalize(const Quat& q) {
  float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (l < 1e-8f) return Quat();
  return {q.x / l, q.y / l, q.z / l, q.w / l};
}
inline Quat qSlerp(const Quat& a, Quat b, float t) {
  float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (d < 0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; d = -d; }
  if (d > 0.9995f) {
    return qNormalize({lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t)});
  }
  float th = std::acos(clampf(d, -1.f, 1.f));
  float s = std::sin(th);
  float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
  return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}

// column-major 4x4: m[col*4 + row]
struct Mat4 {
  float m[16];
  Mat4() { identity(); }
  void identity() {
    for (int i = 0; i < 16; i++) m[i] = 0.f;
    m[0] = m[5] = m[10] = m[15] = 1.f;
  }
  static Mat4 fromQuatPosScale(const Quat& q, const V3& p, const V3& s) {
    Mat4 r;
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    r.m[0] = (1 - (yy + zz)) * s.x; r.m[1] = (xy + wz) * s.x;       r.m[2] = (xz - wy) * s.x;       r.m[3] = 0;
    r.m[4] = (xy - wz) * s.y;       r.m[5] = (1 - (xx + zz)) * s.y; r.m[6] = (yz + wx) * s.y;       r.m[7] = 0;
    r.m[8] = (xz + wy) * s.z;       r.m[9] = (yz - wx) * s.z;       r.m[10] = (1 - (xx + yy)) * s.z; r.m[11] = 0;
    r.m[12] = p.x; r.m[13] = p.y; r.m[14] = p.z; r.m[15] = 1;
    return r;
  }
  Mat4 operator*(const Mat4& o) const {
    Mat4 r;
    for (int c = 0; c < 4; c++)
      for (int row = 0; row < 4; row++) {
        float s = 0;
        for (int k = 0; k < 4; k++) s += m[k * 4 + row] * o.m[c * 4 + k];
        r.m[c * 4 + row] = s;
      }
    return r;
  }
  V3 mulPoint(const V3& v) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
  }
  V3 mulDir(const V3& v) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z,
            m[1] * v.x + m[5] * v.y + m[9] * v.z,
            m[2] * v.x + m[6] * v.y + m[10] * v.z};
  }
  V4 mulVec4(const V4& v) const {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w};
  }
  V3 translation() const { return {m[12], m[13], m[14]}; }
  static Mat4 perspective(float fovyRad, float aspect, float zn, float zf) {
    Mat4 r;
    float f = 1.0f / std::tan(fovyRad * 0.5f);
    for (int i = 0; i < 16; i++) r.m[i] = 0;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1;
    r.m[14] = (2 * zf * zn) / (zn - zf);
    return r;
  }
  static Mat4 lookAt(const V3& eye, const V3& centre, const V3& up) {
    V3 f = normalize(centre - eye);
    V3 s = normalize(cross(f, up));
    V3 u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;   r.m[12] = -dot(s, eye);
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;   r.m[13] = -dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
    r.m[3] = 0; r.m[7] = 0; r.m[11] = 0; r.m[15] = 1;
    return r;
  }
  static Mat4 translate(const V3& t) {
    Mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r;
  }
  static Mat4 scale(const V3& s) {
    Mat4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r;
  }
  static Mat4 rotY(float a) { return fromQuatPosScale(qFromAxisAngle({0, 1, 0}, a), V3(0, 0, 0), V3(1, 1, 1)); }
};

struct AABB {
  V3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
  void add(const V3& p) {
    mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
    mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
  }
  bool valid() const { return mx.x >= mn.x; }
  bool intersects(const AABB& o) const {
    return !(o.mn.x > mx.x || o.mx.x < mn.x || o.mn.y > mx.y || o.mx.y < mn.y || o.mn.z > mx.z || o.mx.z < mn.z);
  }
};

struct RNG {
  uint32_t s = 1234567u;
  explicit RNG(uint32_t seed = 1234567u) : s(seed ? seed : 1u) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float f() { return (next() & 0xffffff) / 16777216.0f; }
  float range(float a, float b) { return a + (b - a) * f(); }
  int irange(int a, int b) { return a + (int)(next() % (uint32_t)std::max(1, b - a + 1)); }
};

// ============================== file IO ====================================
inline bool readFile(const std::string& path, std::vector<uint8_t>& out) {
#ifdef KAYKIT_EMBEDDED_ASSETS
  // portable build: the assets live inside the executable, so try those first
  // when nothing is on disk next to us
  if (vfs::read(path, out)) return true;
#endif
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n < 0) { std::fclose(f); return false; }
  out.resize((size_t)n);
  size_t got = n ? std::fread(out.data(), 1, (size_t)n, f) : 0;
  std::fclose(f);
  return got == (size_t)n;
}
inline std::string dirOf(const std::string& p) {
  size_t s = p.find_last_of("/\\");
  return s == std::string::npos ? std::string(".") : p.substr(0, s);
}
inline bool fileExists(const std::string& p) {
#ifdef KAYKIT_EMBEDDED_ASSETS
  if (vfs::exists(p)) return true;
#endif
  FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  std::fclose(f);
  return true;
}
inline bool writeBMP(const std::string& path, int w, int h, const uint32_t* px) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  int rowBytes = w * 3;
  int pad = (4 - (rowBytes % 4)) % 4;
  int dataSize = (rowBytes + pad) * h;
  uint8_t hdr[54] = {0};
  hdr[0] = 'B'; hdr[1] = 'M';
  int fileSize = 54 + dataSize;
  std::memcpy(hdr + 2, &fileSize, 4);
  int off = 54; std::memcpy(hdr + 10, &off, 4);
  int ih = 40; std::memcpy(hdr + 14, &ih, 4);
  std::memcpy(hdr + 18, &w, 4);
  std::memcpy(hdr + 22, &h, 4);
  short planes = 1; std::memcpy(hdr + 26, &planes, 2);
  short bpp = 24; std::memcpy(hdr + 28, &bpp, 2);
  std::memcpy(hdr + 34, &dataSize, 4);
  std::fwrite(hdr, 1, 54, f);
  std::vector<uint8_t> row(rowBytes + pad, 0);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      uint32_t c = px[y * w + x];
      // our pixels are 0xAABBGGRR; BMP wants B,G,R
      row[x * 3 + 0] = (uint8_t)((c >> 16) & 0xff);
      row[x * 3 + 1] = (uint8_t)((c >> 8) & 0xff);
      row[x * 3 + 2] = (uint8_t)(c & 0xff);
    }
    std::fwrite(row.data(), 1, row.size(), f);
  }
  std::fclose(f);
  return true;
}

// ============================== JSON =======================================
struct JVal {
  enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
  bool b = false;
  double num = 0;
  std::string str;
  std::vector<JVal> arr;
  std::vector<std::pair<std::string, JVal>> obj;

  const JVal* get(const char* k) const {
    if (type != Obj) return nullptr;
    for (auto& kv : obj) if (kv.first == k) return &kv.second;
    return nullptr;
  }
  bool has(const char* k) const { return get(k) != nullptr; }
  double n(const char* k, double d = 0) const { const JVal* v = get(k); return (v && v->type == Num) ? v->num : d; }
  std::string s(const char* k, const char* d = "") const { const JVal* v = get(k); return (v && v->type == Str) ? v->str : d; }
  size_t size() const { return arr.size(); }
  const JVal& at(size_t i) const { static JVal nul; return i < arr.size() ? arr[i] : nul; }
};

struct JParser {
  const char* p;
  const char* end;
  bool ok = true;

  void ws() {
    while (p < end) {
      char c = *p;
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { p++; continue; }
      if (c == '/' && p + 1 < end && p[1] == '/') { while (p < end && *p != '\n') p++; continue; }
      break;
    }
  }
  bool lit(const char* s) {
    size_t n = std::strlen(s);
    if ((size_t)(end - p) < n || std::strncmp(p, s, n) != 0) return false;
    p += n; return true;
  }
  std::string parseString() {
    std::string out;
    p++;  // opening quote
    while (p < end && *p != '"') {
      char c = *p++;
      if (c == '\\') {
        char e = *p++;
        switch (e) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            unsigned cp = 0;
            for (int i = 0; i < 4 && p < end; i++) {
              char h = *p++; cp <<= 4;
              if (h >= '0' && h <= '9') cp |= (h - '0');
              else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
            }
            if (cp < 0x80) out += (char)cp;
            else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
            else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
            break;
          }
          default: out += e;
        }
      } else out += c;
    }
    p++;
    return out;
  }
  JVal parse() {
    ws();
    JVal v;
    if (p >= end) { ok = false; return v; }
    char c = *p;
    if (c == '{') {
      v.type = JVal::Obj; p++; ws();
      if (p < end && *p == '}') { p++; return v; }
      while (p < end) {
        ws();
        if (p >= end || *p != '"') { ok = false; break; }
        std::string k = parseString();
        ws();
        if (p < end && *p == ':') p++; else { ok = false; break; }
        JVal child = parse();
        v.obj.emplace_back(std::move(k), std::move(child));
        ws();
        if (p < end && *p == ',') { p++; continue; }
        if (p < end && *p == '}') { p++; break; }
        ok = false; break;
      }
    } else if (c == '[') {
      v.type = JVal::Arr; p++; ws();
      if (p < end && *p == ']') { p++; return v; }
      while (p < end) {
        v.arr.push_back(parse());
        ws();
        if (p < end && *p == ',') { p++; continue; }
        if (p < end && *p == ']') { p++; break; }
        ok = false; break;
      }
    } else if (c == '"') {
      v.type = JVal::Str; v.str = parseString();
    } else if (c == 't') { v.type = JVal::Bool; v.b = true; if (!lit("true")) ok = false; }
    else if (c == 'f') { v.type = JVal::Bool; v.b = false; if (!lit("false")) ok = false; }
    else if (c == 'n') { v.type = JVal::Null; if (!lit("null")) ok = false; }
    else {
      v.type = JVal::Num;
      char* e = nullptr;
      v.num = std::strtod(p, &e);
      if (e == p) ok = false; else p = e;
    }
    return v;
  }
};
inline bool parseJSON(const char* data, size_t len, JVal& out) {
  JParser p{data, data + len};
  out = p.parse();
  return p.ok;
}


// ============================== inflate (RFC 1950/1951) =====================
// Self-contained DEFLATE decoder (no zlib / no third-party code) so the whole
// engine builds with nothing but a C++ compiler.
namespace inflate_ns {

struct Bits {
  const uint8_t* in;
  size_t len;
  size_t pos = 0;
  uint32_t bitbuf = 0;
  int bitcnt = 0;
  bool bad = false;
  int need(int n) {
    while (bitcnt < n) {
      if (pos >= len) { bad = true; return 1; }
      bitbuf |= (uint32_t)in[pos++] << bitcnt;
      bitcnt += 8;
    }
    return 0;
  }
  int get(int n) {
    if (need(n)) return 0;
    int v = (int)(bitbuf & ((1u << n) - 1));
    bitbuf >>= n;
    bitcnt -= n;
    return v;
  }
};

struct Huff {
  short count[16];
  short symbol[288];
};

inline int decodeSym(Bits& s, const Huff& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len <= 15; len++) {
    code |= s.get(1);
    if (s.bad) return -1;
    int count = h.count[len];
    if (code - count < first) return h.symbol[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return -1;
}

inline void buildHuff(Huff& h, const short* lengths, int n) {
  for (int i = 0; i < 16; i++) h.count[i] = 0;
  for (int i = 0; i < n; i++) h.count[lengths[i]]++;
  short offs[16];
  offs[1] = 0;
  for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h.count[len];
  for (int i = 0; i < n; i++)
    if (lengths[i] != 0) h.symbol[offs[lengths[i]]++] = (short)i;
}

static const short kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                            35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                             3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                             257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                             8193, 12289, 16385, 24577};
static const short kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                              7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

inline int codes(Bits& s, const Huff& lencode, const Huff& distcode, std::vector<uint8_t>& out, size_t cap) {
  for (;;) {
    int sym = decodeSym(s, lencode);
    if (sym < 0) return -1;
    if (sym < 256) {
      if (out.size() >= cap) return -1;
      out.push_back((uint8_t)sym);
    } else if (sym == 256) {
      return 0;
    } else {
      sym -= 257;
      if (sym >= 29) return -1;
      int len = kLenBase[sym] + s.get(kLenExtra[sym]);
      int dsym = decodeSym(s, distcode);
      if (dsym < 0 || dsym >= 30) return -1;
      int dist = kDistBase[dsym] + s.get(kDistExtra[dsym]);
      if ((size_t)dist > out.size()) return -1;
      for (int i = 0; i < len; i++) {
        if (out.size() >= cap) return -1;
        out.push_back(out[out.size() - dist]);
      }
    }
    if (s.bad) return -1;
  }
}

// zlib stream (2-byte header, raw deflate, adler32 trailer)
inline bool zlibDecompress(const uint8_t* data, size_t len, std::vector<uint8_t>& out, size_t cap) {
  if (len < 6) return false;
  if ((data[0] & 0x0f) != 8) return false;      // deflate
  Bits s;
  s.in = data;
  s.len = len;
  s.pos = 2;                                     // skip the zlib header
  out.clear();
  out.reserve(cap ? cap : (size_t)65536);
  for (;;) {
    int last = s.get(1);
    int type = s.get(2);
    if (s.bad) return false;
    if (type == 0) {
      s.bitbuf = 0;
      s.bitcnt = 0;
      if (s.pos + 4 > s.len) return false;
      unsigned l = data[s.pos] | (data[s.pos + 1] << 8);
      s.pos += 4;
      if (s.pos + l > s.len) return false;
      for (unsigned i = 0; i < l; i++) {
        if (out.size() >= cap && cap) return false;
        out.push_back(data[s.pos + i]);
      }
      s.pos += l;
    } else if (type == 1) {
      static Huff lencode, distcode;
      static bool init = false;
      if (!init) {
        short lengths[288];
        int i = 0;
        for (; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        buildHuff(lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        buildHuff(distcode, lengths, 30);
        init = true;
      }
      if (codes(s, lencode, distcode, out, cap ? cap : (size_t)1 << 30) < 0) return false;
    } else if (type == 2) {
      int nlen = s.get(5) + 257;
      int ndist = s.get(5) + 1;
      int ncode = s.get(4) + 4;
      if (s.bad) return false;
      static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      short lengths[320];
      for (int i = 0; i < 19; i++) lengths[i] = 0;
      for (int i = 0; i < ncode; i++) lengths[order[i]] = (short)s.get(3);
      Huff lencode, distcode;
      buildHuff(lencode, lengths, 19);
      if (s.bad) return false;
      int index = 0;
      short all[320];
      while (index < nlen + ndist) {
        int sym = decodeSym(s, lencode);
        if (sym < 0) return false;
        if (sym < 16) {
          all[index++] = (short)sym;
        } else {
          int rlen = 0;
          short val = 0;
          if (sym == 16) {
            if (index == 0) return false;
            val = all[index - 1];
            rlen = 3 + s.get(2);
          } else if (sym == 17) {
            rlen = 3 + s.get(3);
          } else {
            rlen = 11 + s.get(7);
          }
          if (index + rlen > nlen + ndist) return false;
          for (int i = 0; i < rlen; i++) all[index++] = val;
        }
      }
      buildHuff(lencode, all, nlen);
      buildHuff(distcode, all + nlen, ndist);
      if (codes(s, lencode, distcode, out, cap ? cap : (size_t)1 << 30) < 0) return false;
    } else {
      return false;
    }
    if (last) break;
  }
  return true;
}

}  // namespace inflate_ns

// ============================== PNG decode =================================
// Minimal PNG reader: 8-bit, non-interlaced, colour types 0/2/3/4/6.
struct MipLevel {
  int w = 0, h = 0;
  std::vector<uint8_t> px;
};

struct Image {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;
  std::vector<MipLevel> mips;   // 1x1, 2x2, ... (level 0 stays in rgba)

  bool empty() const { return w <= 0 || h <= 0; }
  bool hasMips() const { return !mips.empty(); }

  // Box-filtered mip chain. Without this a 1024x1024 atlas minified to a few
  // hundred pixels shimmers badly: the software renderer was doing point
  // sampling, which is the single biggest reason the port looked rougher than
  // the WebGL build (which gets trilinear + anisotropic for free).
  void buildMips() {
    mips.clear();
    if (empty()) return;
    int cw = w, ch = h;
    while (cw > 1 || ch > 1) {
      int nw = cw > 1 ? cw / 2 : 1;
      int nh = ch > 1 ? ch / 2 : 1;
      MipLevel lv;
      lv.w = nw; lv.h = nh;
      lv.px.resize((size_t)nw * nh * 4);
      const uint8_t* src = mips.empty() ? rgba.data() : mips.back().px.data();
      for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {
          int acc[4] = {0, 0, 0, 0};
          int cnt = 0;
          for (int dy = 0; dy < 2; dy++) {
            int sy = y * 2 + dy; if (sy >= ch) sy = ch - 1;
            for (int dx = 0; dx < 2; dx++) {
              int sx = x * 2 + dx; if (sx >= cw) sx = cw - 1;
              const uint8_t* p = &src[((size_t)sy * cw + sx) * 4];
              for (int c = 0; c < 4; c++) acc[c] += p[c];
              cnt++;
            }
          }
          uint8_t* q = &lv.px[((size_t)y * nw + x) * 4];
          for (int c = 0; c < 4; c++) q[c] = (uint8_t)(acc[c] / cnt);
        }
      }
      mips.push_back(std::move(lv));
      cw = nw; ch = nh;
      if (mips.size() >= 12) break;
    }
  }

  uint32_t sample(float u, float v) const {  // nearest, wrap
    int x = (int)(u * w), y = (int)(v * h);
    x = ((x % w) + w) % w;
    y = ((y % h) + h) % h;
    const uint8_t* p = &rgba[((size_t)y * w + x) * 4];
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  }

  // Bilinear fetch from mip level `lod` (fractional lod: trilinear between the
  // two neighbouring levels). Falls back to nearest if mips were not built.
  uint32_t sampleBilinear(float u, float v, float lod = 0.f) const {
    if (empty()) return 0xffffffffu;
    int lv = (int)lod;
    float frac = lod - (float)lv;
    if (lv < 0) { lv = 0; frac = 0.f; }
    int maxLv = (int)mips.size();
    if (lv > maxLv) { lv = maxLv; frac = 0.f; }

    auto fetch = [&](int level, float uu, float vv) -> uint32_t {
      int lw, lh;
      const uint8_t* base;
      if (level == 0) {
        lw = w; lh = h; base = rgba.data();
      } else if (level - 1 < (int)mips.size()) {
        lw = mips[level - 1].w; lh = mips[level - 1].h; base = mips[level - 1].px.data();
      } else {
        lw = w; lh = h; base = rgba.data();
      }
      if (lw <= 0 || lh <= 0) return 0xffffffffu;
      float fx = uu * (float)lw - 0.5f;
      float fy = vv * (float)lh - 0.5f;
      int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
      float tx = fx - (float)x0, ty = fy - (float)y0;
      int x1 = x0 + 1, y1 = y0 + 1;
      x0 = ((x0 % lw) + lw) % lw; x1 = ((x1 % lw) + lw) % lw;
      y0 = ((y0 % lh) + lh) % lh; y1 = ((y1 % lh) + lh) % lh;
      const uint8_t* p00 = &base[((size_t)y0 * lw + x0) * 4];
      const uint8_t* p10 = &base[((size_t)y0 * lw + x1) * 4];
      const uint8_t* p01 = &base[((size_t)y1 * lw + x0) * 4];
      const uint8_t* p11 = &base[((size_t)y1 * lw + x1) * 4];
      uint32_t out = 0;
      for (int c = 0; c < 4; c++) {
        float top = p00[c] + (p10[c] - p00[c]) * tx;
        float bot = p01[c] + (p11[c] - p01[c]) * tx;
        int val = (int)(top + (bot - top) * ty + 0.5f);
        if (val < 0) val = 0; if (val > 255) val = 255;
        out |= (uint32_t)val << (c * 8);
      }
      return out;
    };

    uint32_t a = fetch(lv, u, v);
    if (frac <= 0.001f || lv + 1 > maxLv) return a;
    uint32_t b = fetch(lv + 1, u, v);
    uint32_t out = 0;
    for (int c = 0; c < 4; c++) {
      int ca = (a >> (c * 8)) & 0xff, cb = (b >> (c * 8)) & 0xff;
      out |= (uint32_t)((int)(ca + (cb - ca) * frac + 0.5f)) << (c * 8);
    }
    return out;
  }
};

inline uint32_t be32(const uint8_t* p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

inline Image decodePNG(const uint8_t* data, size_t len) {
  Image img;
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  if (len < 8 || std::memcmp(data, sig, 8) != 0) return img;
  size_t p = 8;
  std::vector<uint8_t> idat;
  int bitDepth = 8, colorType = 6, interlace = 0;
  std::vector<uint8_t> palette;
  std::vector<uint8_t> trns;
  while (p + 8 <= len) {
    uint32_t clen = be32(data + p);
    std::string type((const char*)data + p + 4, 4);
    const uint8_t* cd = data + p + 8;
    p += 12 + clen;
    if (type == "IHDR") {
      img.w = (int)be32(cd);
      img.h = (int)be32(cd + 4);
      bitDepth = cd[8];
      colorType = cd[9];
      interlace = cd[12];
    } else if (type == "PLTE") {
      palette.assign(cd, cd + clen);
    } else if (type == "tRNS") {
      trns.assign(cd, cd + clen);
    } else if (type == "IDAT") {
      idat.insert(idat.end(), cd, cd + clen);
    } else if (type == "IEND") break;
  }
  if (img.w <= 0 || img.h <= 0 || idat.empty()) return img;
  if (bitDepth != 8 || interlace != 0) return img;

  int channels = colorType == 0 ? 1 : colorType == 2 ? 3 : colorType == 3 ? 1 : colorType == 4 ? 2 : 4;
  size_t rawSize = (size_t)img.h * (1 + (size_t)img.w * channels);
  std::vector<uint8_t> raw;
  if (!inflate_ns::zlibDecompress(idat.data(), idat.size(), raw, rawSize + 1024)) {
    std::fprintf(stderr, "[png] inflate failed\n");
    return img;
  }
  raw.resize(rawSize, 0);

  const size_t stride = (size_t)img.w * channels;
  std::vector<uint8_t> prev(stride, 0), cur(stride, 0);
  img.rgba.assign((size_t)img.w * img.h * 4, 255);
  for (int y = 0; y < img.h; y++) {
    const uint8_t* rowp = raw.data() + (size_t)y * (stride + 1);
    uint8_t filter = rowp[0];
    std::memcpy(cur.data(), rowp + 1, stride);
    for (size_t x = 0; x < stride; x++) {
      int a = x >= (size_t)channels ? cur[x - channels] : 0;
      int b = prev[x];
      int c = x >= (size_t)channels ? prev[x - channels] : 0;
      int v = cur[x];
      switch (filter) {
        case 1: v += a; break;
        case 2: v += b; break;
        case 3: v += (a + b) >> 1; break;
        case 4: {
          int pp = a + b - c;
          int pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
          v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
          break;
        }
        default: break;
      }
      cur[x] = (uint8_t)(v & 0xff);
    }
    for (int x = 0; x < img.w; x++) {
      uint8_t* dst = &img.rgba[((size_t)y * img.w + x) * 4];
      const uint8_t* s = &cur[(size_t)x * channels];
      switch (colorType) {
        case 0: dst[0] = dst[1] = dst[2] = s[0]; dst[3] = 255; break;
        case 2: dst[0] = s[0]; dst[1] = s[1]; dst[2] = s[2]; dst[3] = 255; break;
        case 3: {
          int idx = s[0];
          if ((size_t)(idx * 3 + 2) < palette.size()) {
            dst[0] = palette[idx * 3]; dst[1] = palette[idx * 3 + 1]; dst[2] = palette[idx * 3 + 2];
            dst[3] = (size_t)idx < trns.size() ? trns[idx] : 255;
          }
          break;
        }
        case 4: dst[0] = dst[1] = dst[2] = s[0]; dst[3] = s[1]; break;
        default: dst[0] = s[0]; dst[1] = s[1]; dst[2] = s[2]; dst[3] = s[3]; break;
      }
    }
    prev = cur;
  }
  return img;
}
inline Image loadPNG(const std::string& path) {
  std::vector<uint8_t> d;
  if (!readFile(path, d)) return Image();
  return decodePNG(d.data(), d.size());
}
