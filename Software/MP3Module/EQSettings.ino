#include "EQSettings.h"
#include "AudioTools.h"

struct EQPreset presets[NUMBER_EQ_PRESETS];

void initPresets() {
  presets[0] = EQPreset{0.5, 0.5, 0.5}; // normal
  presets[1] = EQPreset{2.0, 0.5, 0.5}; // bass boost
  presets[2] = EQPreset{0.0, 0.0, 0.0}; // treble boost
}

bool changeEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq, struct EQPreset preset) {
  cfg_eq->gain_low = preset.bass;
  cfg_eq->gain_medium = preset.mid;
  cfg_eq->gain_high = preset.treble;

  if (eq) {
    Serial.printf("Changing EQ Setting to: %f, %f, %f", preset.bass, preset.mid, preset.treble);
    return eq->begin(*cfg_eq);
  } else {
    return false;
  }
}

bool changeEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq, float bass, float mid, float treble) {
  struct EQPreset preset = {bass, mid, treble};

  return changeEQcfg(eq, cfg_eq, preset);
}

uint16_t cycleEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq) {
  static uint16_t cur_preset = DEFAULT_EQ_PRESET;

  cur_preset++;

  if (cur_preset >= NUMBER_EQ_PRESETS)
    cur_preset = 0;

  changeEQcfg(eq, cfg_eq, presets[cur_preset]);

  return cur_preset;
}

bool initEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq) {
  return changeEQcfg(eq, cfg_eq, presets[DEFAULT_EQ_PRESET]);
}
