#pragma once
#include "AudioTools.h"

#define DEFAULT_EQ_PRESET 0
#define NUMBER_EQ_PRESETS 3

struct EQPreset {
  float bass;
  float mid;
  float treble;
};

extern struct EQPreset presets[NUMBER_EQ_PRESETS];

void initPresets();
bool changeEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq, float bass, float mid, float treble);
bool changeEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq, struct EQPreset preset);
bool initEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq);
uint16_t cycleEQcfg(Equalizer3Bands *eq, ConfigEqualizer3Bands *cfg_eq);
