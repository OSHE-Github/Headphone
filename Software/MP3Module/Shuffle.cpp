#include "Shuffle.h"
#include <algorithm>


// shuffle flag management
bool ShufflePlaylist::getShuffleFlag() {
  return shuffle_flag;
}

bool ShufflePlaylist::toggleShuffleFlag() {
  shuffle_flag = !shuffle_flag;
  return shuffle_flag;
}

void ShufflePlaylist::setShuffleFlag(bool flag) {
  shuffle_flag = flag;
}


// ---------- helpers ----------
static uint32_t rnd(uint32_t n) { return n ? esp_random() % n : 0; }
static float    rnd01()         { return (esp_random() >> 8) / 16777216.0f; }

// FNV-1a hash, case-insensitive. Skips NUL bytes so simple UTF-16 text
// hashes the same as ASCII, and skips spaces so "AC DC" == "ACDC".
static uint32_t hashName(const char* s, size_t n) {
  uint32_t h = 2166136261u;
  bool any = false;
  for (size_t i = 0; i < n; i++) {
    uint8_t c = (uint8_t)s[i];
    if (c == 0 || c == ' ') continue;
    if (c >= 'A' && c <= 'Z') c += 32;
    h ^= c; h *= 16777619u; any = true;
  }
  return any ? h : 0;
}

static uint32_t syncsafe(const uint8_t* b) {
  return ((uint32_t)(b[0] & 0x7F) << 21) | ((uint32_t)(b[1] & 0x7F) << 14) |
         ((uint32_t)(b[2] & 0x7F) << 7)  |  (uint32_t)(b[3] & 0x7F);
}
static uint32_t be32(const uint8_t* b) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

template <typename T>
static void fisherYates(std::vector<T>& v) {
  for (size_t i = v.size(); i > 1; i--) std::swap(v[i - 1], v[rnd(i)]);
}

// ---------- artist reading ----------
uint32_t ShufflePlaylist::readArtist(fs::File& f, const char* fileName) {
  uint8_t hdr[10];

  // 1) ID3v2 (2.2 / 2.3 / 2.4): look for the TPE1 / TP1 frame
  f.seek(0);
  if (f.read(hdr, 10) == 10 && memcmp(hdr, "ID3", 3) == 0) {
    uint8_t  ver     = hdr[3];
    uint32_t tagEnd  = 10 + syncsafe(hdr + 6);
    uint32_t p       = 10;
    bool     v22     = (ver == 2);
    size_t   fhLen   = v22 ? 6 : 10;

    if (!v22 && (hdr[5] & 0x40)) {               // skip extended header
      uint8_t eh[4];
      if (f.read(eh, 4) == 4) p += (ver == 4) ? syncsafe(eh) : be32(eh) + 4;
      f.seek(p);
    }

    while (p + fhLen < tagEnd) {
      uint8_t fh[10];
      if (f.read(fh, fhLen) != fhLen || fh[0] == 0) break;   // padding / EOF
      uint32_t sz = v22 ? ((uint32_t)fh[3] << 16 | fh[4] << 8 | fh[5])
                        : (ver == 4 ? syncsafe(fh + 4) : be32(fh + 4));
      p += fhLen;
      if (sz == 0 || p + sz > tagEnd) break;

      bool isArtist = v22 ? memcmp(fh, "TP1", 3) == 0 : memcmp(fh, "TPE1", 4) == 0;
      if (isArtist && sz > 1) {
        char buf[96];
        size_t n = std::min<size_t>(sz, sizeof(buf));
        n = f.read((uint8_t*)buf, n);
        size_t start = 1;                                   // skip encoding byte
        if (buf[0] == 1 && n >= 3) start = 3;               // skip UTF-16 BOM
        uint32_t h = hashName(buf + start, n - start);
        if (h) return h;
        break;
      }
      p += sz;
      f.seek(p);
    }
  }

  // 2) ID3v1 at the end of the file
  if (f.size() >= 128) {
    uint8_t tag[128];
    f.seek(f.size() - 128);
    if (f.read(tag, 128) == 128 && memcmp(tag, "TAG", 3) == 0) {
      uint32_t h = hashName((const char*)tag + 33, 30);
      if (h) return h;
    }
  }

  // 3) File name "Artist - Title.mp3"
  const char* dash = strstr(fileName, " - ");
  if (dash && dash > fileName) return hashName(fileName, dash - fileName);

  // 4) Unknown: treat the file as its own artist
  return hashName(fileName, strlen(fileName)) | 1;
}

// ---------- scanning ----------
size_t ShufflePlaylist::scan(fs::FS& fs, const char* dir) {
  tracks.clear();
  order.clear();
  pos = 0;

  File root = fs.open(dir);
  if (!root || !root.isDirectory()) return 0;

  File entry;
  while ((entry = root.openNextFile())) {
    if (!entry.isDirectory()) {
      const char* name = entry.name();
      size_t len = strlen(name);
      if (len > 4 && strcasecmp(name + len - 4, ".mp3") == 0 && name[0] != '.') {
        Track t;
        t.path = "/";
        t.path += name;
        t.artist = readArtist(entry, name);
        tracks.push_back(t);
        if (tracks.size() >= 65000) { entry.close(); break; }
      }
    }
    entry.close();
  }
  root.close();

  if (getShuffleFlag() == true)
    shuffle();
  
  return tracks.size();
}

// ---------- the shuffle ----------
// Spread the shorter list evenly through the longer one, random start offset.
std::vector<uint16_t> ShufflePlaylist::interleave(const std::vector<uint16_t>& a,
                                                  const std::vector<uint16_t>& b) {
  const std::vector<uint16_t>& L = (a.size() >= b.size()) ? a : b;
  const std::vector<uint16_t>& S = (&L == &a) ? b : a;
  size_t n = L.size(), m = S.size(), total = n + m;
  if (m == 0) return L;

  std::vector<uint16_t> out;
  out.reserve(total);
  float step = (float)total / m;   // >= 2, so slots never collide
  float off  = rnd01();
  size_t si = 0, li = 0;

  for (size_t p = 0; p < total; p++) {
    size_t slot = (si < m) ? (size_t)((si + off) * step) : SIZE_MAX;
    if (p == slot) out.push_back(S[si++]);
    else           out.push_back(L[li++]);
  }
  return out;
}

void ShufflePlaylist::buildOrder() {
  // 1) group track indexes by artist
  std::vector<uint16_t> idx(tracks.size());
  for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
  std::sort(idx.begin(), idx.end(),
            [this](uint16_t x, uint16_t y) { return tracks[x].artist < tracks[y].artist; });

  std::vector<std::vector<uint16_t>> groups;
  for (size_t i = 0; i < idx.size(); i++) {
    if (i == 0 || tracks[idx[i]].artist != tracks[idx[i - 1]].artist) groups.emplace_back();
    groups.back().push_back(idx[i]);
  }

  // 2) shuffle inside each group; shuffle groups so equal sizes tie randomly
  for (auto& g : groups) fisherYates(g);
  fisherYates(groups);
  std::stable_sort(groups.begin(), groups.end(),
                   [](const std::vector<uint16_t>& x, const std::vector<uint16_t>& y) {
                     return x.size() < y.size();
                   });

  // 3) merge smallest-first
  std::vector<uint16_t> merged;
  for (auto& g : groups) merged = interleave(merged, g);
  order.swap(merged);
}

void ShufflePlaylist::shuffle() {
  if (tracks.empty()) { order.clear(); pos = 0; return; }

  uint32_t lastArtist = order.empty() ? 0 : tracks[order.back()].artist;
  buildOrder();

  // avoid the same artist across the seam between two passes
  for (int tries = 0; tries < 5 && lastArtist && order.size() > 1 &&
                      tracks[order[0]].artist == lastArtist; tries++) {
    buildOrder();
  }
  pos = 0;
}

// ---------- navigation ----------
const char* ShufflePlaylist::current() const {
  return order.empty() ? "" : tracks[order[pos]].path.c_str();
}

const char* ShufflePlaylist::next() {
  if (order.empty()) return "";
  if (++pos >= order.size()) shuffle();   // new pass, new order
  return current();
}

const char* ShufflePlaylist::prev() {
  if (order.empty()) return "";
  if (pos > 0) pos--;
  return current();
}

void ShufflePlaylist::printOrder(Print& out, size_t maxLines) const {
  out.printf("Shuffle order (%u tracks):\n", (unsigned)order.size());
  for (size_t i = 0; i < order.size() && i < maxLines; i++) {
    const Track& t = tracks[order[i]];
    out.printf("  %3u  %08lx  %s\n", (unsigned)i, (unsigned long)t.artist, t.path.c_str());
  }
}
