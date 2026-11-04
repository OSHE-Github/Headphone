#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

// Artist-interleaving shuffle (after Ruud van Asseldonk's approach):
// group by artist, shuffle each group, then merge groups smallest-first,
// spreading the shorter list evenly through the longer one.
class ShufflePlaylist {
public:
  // Read every .mp3 in 'dir', pull its artist, build a play order.
  size_t scan(fs::FS& fs, const char* dir = "/");

  // Build a new play order (called by scan and at the end of each pass).
  void shuffle();

  size_t size() const { return order.size(); }
  size_t position() const { return pos; }

  const char* current() const;   // path of the track at the current position
  const char* next();            // move forward; reshuffles after the last track
  const char* prev();            // move back; stays on the first track of a pass

  void printOrder(Print& out, size_t maxLines = 30) const;

private:
  struct Track {
    String   path;
    uint32_t artist;   // hash of the normalised artist name
  };

  std::vector<Track>    tracks;
  std::vector<uint16_t> order;    // indexes into 'tracks'
  size_t pos = 0;

  void buildOrder();
  static uint32_t readArtist(fs::File& f, const char* fileName);
  static std::vector<uint16_t> interleave(const std::vector<uint16_t>& a,
                                          const std::vector<uint16_t>& b);
};
