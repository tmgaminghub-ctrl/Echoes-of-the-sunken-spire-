// ---------------------------------------------------------------------------
//  vfs.h — read-only virtual filesystem for portable single-file builds.
//
//  In a normal build every asset comes off disk. In a *portable* build the whole
//  assets/ tree is baked into the executable (see tools/embed_assets.py), so
//  readFile()/fileExists() ask this layer whenever the disk lookup fails.
//
//  Lookup is by path suffix, so it does not matter what prefix the game computed:
//  "assets/characters/Knight.glb", "./assets/characters/Knight.glb" and
//  "C:/Games/Hollowmere/assets/characters/Knight.glb" all resolve to the baked-in
//  "characters/knight.glb". Paths are compared case-insensitively.
//
//  Only compiled in when KAYKIT_EMBEDDED_ASSETS is defined.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vfs {

// true when this build has an asset table baked in
bool available();

// number of files baked in (for the banner/log)
int count();

// fills `out` and returns true if `path` (or a suffix of it) is baked in.
// Directories are not files: they answer exists() but not read().
bool read(const std::string& path, std::vector<uint8_t>& out);

// true if a file *or directory* with that path suffix is baked in
bool exists(const std::string& path);

}  // namespace vfs
