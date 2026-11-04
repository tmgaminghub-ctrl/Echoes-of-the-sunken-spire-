// ---------------------------------------------------------------------------
//  gltf.cpp — glTF 2.0 (.gltf + external buffers) and .glb loader
// ---------------------------------------------------------------------------
#include "gltf.h"

namespace {

struct Reader {
  const JVal* doc = nullptr;
  std::string baseDir;
  std::vector<uint8_t> glbBin;
  std::vector<std::vector<uint8_t>> buffers;
  bool ok = true;
};

// glTF accessor "type" is a string: SCALAR / VEC2 / VEC3 / VEC4 / MAT2 / MAT3 / MAT4
int compCount(const std::string& type) {
  if (type == "SCALAR") return 1;
  if (type == "VEC2") return 2;
  if (type == "VEC3") return 3;
  if (type == "VEC4") return 4;
  if (type == "MAT2") return 4;
  if (type == "MAT3") return 9;
  if (type == "MAT4") return 16;
  return 0;
}

// Reads an accessor into a flat float array (components interleaved).
std::vector<float> readAccessor(const Reader& r, int accessorIndex) {
  std::vector<float> out;
  if (!r.doc) return out;
  const JVal* accs = r.doc->get("accessors");
  if (!accs || accessorIndex < 0 || (size_t)accessorIndex >= accs->arr.size()) return out;
  const JVal& acc = accs->arr[accessorIndex];
  size_t count = (size_t)acc.n("count");
  int nc = compCount(acc.s("type", "SCALAR"));
  int ct = (int)acc.n("componentType", 5126);
  bool normalized = acc.get("normalized") && acc.get("normalized")->b;
  int cc = ct == 5126 ? 4 : ct == 5125 ? 4 : ct == 5123 ? 2 : ct == 5122 ? 2 : 1;
  out.assign(count * nc, 0.f);

  const JVal* bv = nullptr;
  const JVal* bvs = r.doc->get("bufferViews");
  if (acc.has("bufferView") && bvs) {
    int bvIndex = (int)acc.n("bufferView");
    if (bvIndex >= 0 && (size_t)bvIndex < bvs->arr.size()) bv = &bvs->arr[bvIndex];
  }
  if (!bv) return out;
  int bufferIndex = (int)bv->n("buffer", -1);
  if (bufferIndex < 0 || (size_t)bufferIndex >= r.buffers.size()) return out;
  const std::vector<uint8_t>& buf = r.buffers[bufferIndex];
  size_t base = (size_t)bv->n("byteOffset", 0) + (size_t)acc.n("byteOffset", 0);
  size_t stride = (size_t)bv->n("byteStride", 0);
  if (stride == 0) stride = (size_t)nc * cc;

  for (size_t i = 0; i < count; i++) {
    const uint8_t* p = buf.data() + base + i * stride;
    if (base + i * stride + (size_t)nc * cc > buf.size()) break;
    for (int c = 0; c < nc; c++) {
      float v = 0;
      const uint8_t* q = p + c * cc;
      switch (ct) {
        case 5126: { float f; std::memcpy(&f, q, 4); v = f; break; }
        case 5125: { uint32_t u; std::memcpy(&u, q, 4); v = (float)u; break; }
        case 5123: { uint16_t u; std::memcpy(&u, q, 2); v = normalized ? u / 65535.f : (float)u; break; }
        case 5122: { int16_t s; std::memcpy(&s, q, 2); v = normalized ? std::max(s / 32767.f, -1.f) : (float)s; break; }
        case 5121: { v = normalized ? q[0] / 255.f : (float)q[0]; break; }
        case 5120: { int8_t s; std::memcpy(&s, q, 1); v = normalized ? std::max(s / 127.f, -1.f) : (float)s; break; }
        default: break;
      }
      out[i * nc + c] = v;
    }
  }
  return out;
}

bool loadBuffers(Reader& r, const std::string& path) {
  std::vector<uint8_t> raw;
  if (!readFile(path, raw)) { r.ok = false; return false; }
  if (raw.size() >= 12 && raw[0] == 'g' && raw[1] == 'l' && raw[2] == 'T' && raw[3] == 'F') {
    // GLB: header(12) then chunks
    size_t p = 12;
    std::string jsonText;
    while (p + 8 <= raw.size()) {
      uint32_t clen; std::memcpy(&clen, raw.data() + p, 4);
      uint32_t ctype; std::memcpy(&ctype, raw.data() + p + 4, 4);
      p += 8;
      if (p + clen > raw.size()) break;
      if (ctype == 0x4E4F534A) jsonText.assign((const char*)raw.data() + p, clen);       // JSON
      else if (ctype == 0x004E4942) r.glbBin.assign(raw.begin() + p, raw.begin() + p + clen);  // BIN
      p += clen;
      p = (p + 3) & ~size_t(3);
    }
    if (getenv("KK_GLTF_DEBUG")) {
      size_t total = 0;
      for (auto& b : r.buffers) total += b.size();
      std::printf("[glb] %s raw=%d bin=%d buffers=%d total=%d\n", path.c_str(), (int)raw.size(),
                  (int)r.glbBin.size(), (int)r.buffers.size(), (int)total);
    }
    if (jsonText.empty()) { r.ok = false; return false; }
    JVal doc;
    if (!parseJSON(jsonText.data(), jsonText.size(), doc)) { r.ok = false; return false; }
    r.doc = new JVal(std::move(doc));
  } else {
    JVal doc;
    if (!parseJSON((const char*)raw.data(), raw.size(), doc)) { r.ok = false; return false; }
    r.doc = new JVal(std::move(doc));
  }

  r.baseDir = dirOf(path);
  const JVal* bufs = r.doc->get("buffers");
  if (bufs) {
    for (auto& b : bufs->arr) {
      std::vector<uint8_t> data;
      if (b.has("uri")) {
        std::string uri = b.s("uri");
        if (uri.rfind("data:", 0) == 0) {
          size_t comma = uri.find(',');
          if (comma != std::string::npos) data.assign(uri.begin() + comma + 1, uri.end());
        } else {
          readFile(r.baseDir + "/" + uri, data);
        }
      } else {
        data = r.glbBin;
      }
      r.buffers.push_back(std::move(data));
    }
  }
  return true;
}

Image loadImage(const Reader& r, int imageIndex) {
  Image img;
  const JVal* images = r.doc->get("images");
  if (!images || imageIndex < 0 || (size_t)imageIndex >= images->arr.size()) return img;
  const JVal& im = images->arr[imageIndex];
  if (im.has("bufferView")) {
    int bvIndex = (int)im.n("bufferView");
    const JVal* bvs = r.doc->get("bufferViews");
    if (bvs && bvIndex >= 0 && (size_t)bvIndex < bvs->arr.size()) {
      const JVal& bv = bvs->arr[bvIndex];
      int bufferIndex = (int)bv.n("buffer", -1);
      if (bufferIndex >= 0 && (size_t)bufferIndex < r.buffers.size()) {
        size_t off = (size_t)bv.n("byteOffset", 0);
        size_t len = (size_t)bv.n("byteLength", 0);
        const auto& buf = r.buffers[bufferIndex];
        if (off + len <= buf.size()) img = decodePNG(buf.data() + off, len);
      }
    }
  } else if (im.has("uri")) {
    std::string uri = im.s("uri");
    if (uri.rfind("data:", 0) != 0) img = loadPNG(r.baseDir + "/" + uri);
  }
  return img;
}

}  // namespace

Model loadModel(const std::string& path) {
  Model out;
  out.path = path;
  Reader r;
  loadBuffers(r, path);
  if (!r.ok || !r.doc) {
    std::fprintf(stderr, "[gltf] failed to load %s\n", path.c_str());
    return out;
  }
  const JVal& doc = *r.doc;

  // ---- textures & materials ----
  const JVal* textures = doc.get("textures");
  if (textures) {
    for (auto& t : textures->arr) {
      int src = (int)t.n("source", -1);
      Image img = src >= 0 ? loadImage(r, src) : Image();
      img.buildMips();   // trilinear-ready; strips point sampling from the raster
      if (img.empty()) {
        // keep slot numbering stable even on failure
        Image blank; blank.w = 1; blank.h = 1; blank.rgba = {255, 255, 255, 255};
        out.texs.push_back(blank);
      } else {
        out.texs.push_back(img);
      }
    }
  }
  const JVal* materials = doc.get("materials");
  if (materials) {
    for (auto& m : materials->arr) {
      Material mat;
      const JVal* pbr = m.get("pbrMetallicRoughness");
      if (pbr) {
        const JVal* bc = pbr->get("baseColorFactor");
        if (bc && bc->arr.size() >= 3) {
          mat.color = V3((float)bc->arr[0].num, (float)bc->arr[1].num, (float)bc->arr[2].num);
          if (bc->arr.size() >= 4) mat.alpha = (float)bc->arr[3].num;
        }
        mat.metallic = (float)pbr->n("metallicFactor", 0.0);
        mat.roughness = (float)pbr->n("roughnessFactor", 1.0);
        const JVal* bct = pbr->get("baseColorTexture");
        if (bct) mat.tex = (int)bct->n("index", -1);
      }
      const JVal* em = m.get("emissiveFactor");
      if (em && em->arr.size() >= 3)
        mat.emissive = V3((float)em->arr[0].num, (float)em->arr[1].num, (float)em->arr[2].num);
      std::string alphaMode = m.s("alphaMode", "OPAQUE");
      mat.alphaCut = alphaMode == "MASK";
      if (alphaMode == "BLEND") mat.alphaCut = true;
      const JVal* ds = m.get("doubleSided");
      mat.doubleSided = ds && ds->b;
      out.mats.push_back(mat);
    }
  }
  if (out.mats.empty()) {
    Material m;
    out.mats.push_back(m);
  }

  // ---- meshes ----
  const JVal* meshes = doc.get("meshes");
  if (meshes) {
    for (auto& m : meshes->arr) {
      const JVal* prims = m.get("primitives");
      if (!prims) continue;
      for (auto& p : prims->arr) {
        Mesh mesh;
        const JVal* attrs = p.get("attributes");
        if (!attrs) continue;
        auto acc = [&](const char* n) { return attrs->has(n) ? (int)attrs->get(n)->num : -1; };
        std::vector<float> P = readAccessor(r, acc("POSITION"));
        std::vector<float> N = readAccessor(r, acc("NORMAL"));
        std::vector<float> T = readAccessor(r, acc("TEXCOORD_0"));
        std::vector<float> J = readAccessor(r, acc("JOINTS_0"));
        std::vector<float> W = readAccessor(r, acc("WEIGHTS_0"));
        size_t vcount = P.size() / 3;
        if (vcount == 0) continue;
        mesh.pos.resize(vcount);
        mesh.nrm.resize(vcount);
        mesh.uv.resize(vcount);
        for (size_t i = 0; i < vcount; i++) {
          mesh.pos[i] = V3(P[i * 3], P[i * 3 + 1], P[i * 3 + 2]);
          mesh.bounds.add(mesh.pos[i]);
          if (N.size() >= vcount * 3) mesh.nrm[i] = V3(N[i * 3], N[i * 3 + 1], N[i * 3 + 2]);
          else mesh.nrm[i] = V3(0, 1, 0);
          if (T.size() >= vcount * 2) mesh.uv[i] = V2(T[i * 2], T[i * 2 + 1]);
        }
        if (getenv("KK_GLTF_DEBUG")) {
          std::printf("[gltf] %s prim: verts=%d J=%d W=%d attrs=%d\n", path.c_str(), (int)vcount,
                      (int)J.size(), (int)W.size(), (int)attrs->obj.size());
        }
        if (J.size() >= vcount * 4 && W.size() >= vcount * 4) {
          mesh.skinned = true;
          mesh.joints.resize(vcount);
          mesh.weights.resize(vcount);
          for (size_t i = 0; i < vcount; i++) {
            mesh.joints[i] = V4(J[i * 4], J[i * 4 + 1], J[i * 4 + 2], J[i * 4 + 3]);
            mesh.weights[i] = V4(W[i * 4], W[i * 4 + 1], W[i * 4 + 2], W[i * 4 + 3]);
          }
        }
        if (p.has("indices")) {
          std::vector<float> I = readAccessor(r, (int)p.n("indices"));
          mesh.idx.resize(I.size());
          for (size_t i = 0; i < I.size(); i++) mesh.idx[i] = (uint32_t)I[i];
        } else {
          mesh.idx.resize(vcount);
          for (size_t i = 0; i < vcount; i++) mesh.idx[i] = (uint32_t)i;
        }
        if (p.has("material")) mesh.mat = (int)p.n("material", 0);
        out.meshes.push_back(std::move(mesh));
      }
    }
  }

  // ---- nodes ----
  const JVal* nodes = doc.get("nodes");
  std::vector<std::vector<int>> nodeChildren;
  if (nodes) {
    for (auto& n : nodes->arr) {
      Node node;
      node.name = n.s("name");
      const JVal* tr = n.get("translation");
      if (tr && tr->arr.size() >= 3) node.t = V3((float)tr->arr[0].num, (float)tr->arr[1].num, (float)tr->arr[2].num);
      const JVal* ro = n.get("rotation");
      if (ro && ro->arr.size() >= 4)
        node.r = Quat((float)ro->arr[0].num, (float)ro->arr[1].num, (float)ro->arr[2].num, (float)ro->arr[3].num);
      const JVal* sc = n.get("scale");
      if (sc && sc->arr.size() >= 3) node.s = V3((float)sc->arr[0].num, (float)sc->arr[1].num, (float)sc->arr[2].num);
      if (n.has("mesh")) node.mesh = (int)n.n("mesh");
      if (n.has("skin")) node.skin = (int)n.n("skin");
      out.nodes.push_back(node);
    }
  }
  if (nodes) {
    for (size_t i = 0; i < nodes->arr.size(); i++) {
      const JVal* ch = nodes->arr[i].get("children");
      if (!ch) continue;
      for (auto& c : ch->arr) {
        int ci = (int)c.num;
        if (ci >= 0 && (size_t)ci < out.nodes.size()) {
          out.nodes[ci].parent = (int)i;
          out.nodes[i].children.push_back(ci);
        }
      }
    }
  }

  // ---- skins ----
  const JVal* skins = doc.get("skins");
  if (skins) {
    for (auto& s : skins->arr) {
      Skin skin;
      const JVal* joints = s.get("joints");
      if (joints) for (auto& j : joints->arr) skin.joints.push_back((int)j.num);
      std::vector<float> ibm = readAccessor(r, (int)s.n("inverseBindMatrices", -1));
      skin.invBind.assign(skin.joints.size(), Mat4());
      for (size_t i = 0; i < skin.joints.size(); i++) {
        if (ibm.size() >= (i + 1) * 16) {
          // glTF stores column-major, same as our Mat4
          std::memcpy(skin.invBind[i].m, &ibm[i * 16], sizeof(float) * 16);
        }
      }
      out.skins.push_back(std::move(skin));
    }
  }

  // ---- animations ----
  const JVal* anims = doc.get("animations");
  if (anims) {
    for (auto& a : anims->arr) {
      Clip clip;
      clip.name = a.s("name");
      const JVal* samplers = a.get("samplers");
      const JVal* channels = a.get("channels");
      if (!samplers || !channels) continue;
      for (auto& ch : channels->arr) {
        const JVal* target = ch.get("target");
        if (!target) continue;
        int ni = (int)target->n("node", -1);
        std::string p = target->s("path");
        int si = (int)ch.n("sampler", -1);
        if (ni < 0 || si < 0 || (size_t)si >= samplers->arr.size() || ni >= (int)out.nodes.size()) continue;
        const JVal& smp = samplers->arr[si];
        Chan c;
        c.node = out.nodes[ni].name;
        c.path = p == "translation" ? 0 : (p == "rotation" ? 1 : 2);
        c.times = readAccessor(r, (int)smp.n("input", -1));
        c.vals = readAccessor(r, (int)smp.n("output", -1));
        if (c.times.empty() || c.vals.empty()) continue;
        clip.dur = std::max(clip.dur, c.times.back());
        clip.chans.push_back(std::move(c));
      }
      if (!clip.chans.empty()) out.clips.push_back(std::move(clip));
    }
  }

  delete r.doc;
  out.finalize();
  return out;
}
