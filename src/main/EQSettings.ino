#include "EQSettings.h"
#include "AudioTools.h"

struct EQPreset presets[NUMBER_EQ_PRESETS];

void initPresets() {
  presets[0] = EQPreset{1.0, 0.8, 1.0};
  presets[1] = EQPreset{1.5, 0.0, 0.0}; // testing
}

bool changeEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq, struct EQPreset preset) {
  cfg_eq->gain_low = preset.low;
  cfg_eq->gain_medium = preset.mid;
  cfg_eq->gain_high = preset.high;

  if (eq) {
    return eq->begin(*cfg_eq);
  } else {
    return false;
  }
}

bool changeEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq, float low, float mid, float high) {
  struct EQPreset preset = {low, mid, high};

  return changeEQcfg(eq, cfg_eq, preset);
}

uint16_t cycleEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq) {
  static uint16_t cur_preset = DEFAULT_EQ_PRESET;

  cur_preset++;
  if (cur_preset >= NUMBER_EQ_PRESETS)
    cur_preset = 0;

  return changeEQcfg(eq, cfg_eq, presets[cur_preset]);
}

bool initEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq) {
  return changeEQcfg(eq, cfg_eq, presets[DEFAULT_EQ_PRESET]);
}