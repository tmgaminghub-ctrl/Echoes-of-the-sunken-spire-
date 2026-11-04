// ---------------------------------------------------------------------------
//  gltf.h — glTF 2.0 / GLB loader with skins and animation
// ---------------------------------------------------------------------------
#pragma once
#include "base.h"

struct Mesh {
  std::vector<V3> pos, nrm;
  std::vector<V2> uv;
  std::vector<V3> col;   // optional vertex colour (terrain)
  std::vector<uint32_t> idx;
  std::vector<V4> joints;
  std::vector<V4> weights;
  int mat = -1;
  bool skinned = false;
  AABB bounds;
};

struct Node {
  std::string name;
  V3 t{0, 0, 0};
  Quat r;
  V3 s{1, 1, 1};
  int parent = -1;
  std::vector<int> children;
  int mesh = -1, skin = -1;
};

struct Skin {
  std::vector<int> joints;      // node indices
  std::vector<Mat4> invBind;
};

struct Chan {
  std::string node;
  int path = 0;                 // 0=translation 1=rotation 2=scale
  std::vector<float> times;
  std::vector<float> vals;      // 3 or 4 floats per key
};

struct Clip {
  std::string name;
  float dur = 0.f;
  std::vector<Chan> chans;
};

struct Material {
  V3 color{1, 1, 1};
  float alpha = 1.f;
  int tex = -1;
  bool doubleSided = false;
  bool alphaCut = false;
  float metallic = 0.f, roughness = 0.85f;
  V3 emissive{0, 0, 0};
};

struct Model {
  std::string path;
  std::vector<Node> nodes;
  std::vector<int> roots;
  std::vector<Mesh> meshes;
  std::vector<Material> mats;
  std::vector<Image> texs;
  std::vector<Skin> skins;
  std::vector<Clip> clips;
  std::unordered_map<std::string, int> byName;
  bool skinned = false;

  void finalize() {
    byName.clear();
    for (size_t i = 0; i < nodes.size(); i++) byName[nodes[i].name] = (int)i;
    roots.clear();
    for (size_t i = 0; i < nodes.size(); i++) if (nodes[i].parent < 0) roots.push_back((int)i);
    for (auto& m : meshes) if (m.skinned) skinned = true;
  }
  int nodeIndex(const char* n) const {
    auto it = byName.find(n);
    return it == byName.end() ? -1 : it->second;
  }
  const Clip* clip(const char* n) const {
    for (auto& c : clips) if (c.name == n) return &c;
    return nullptr;
  }
  // rest-pose world matrices (parents before children is guaranteed here)
  void restWorld(std::vector<Mat4>& out) const {
    out.assign(nodes.size(), Mat4());
    std::vector<char> done(nodes.size(), 0);
    std::vector<int> stack = roots;
    while (!stack.empty()) {
      int i = stack.back();
      stack.pop_back();
      if (done[i]) continue;
      const Node& n = nodes[i];
      Mat4 local = Mat4::fromQuatPosScale(n.r, n.t, n.s);
      out[i] = n.parent >= 0 ? out[n.parent] * local : local;
      done[i] = 1;
      for (int c : n.children) stack.push_back(c);
    }
  }
};

Model loadModel(const std::string& path);
