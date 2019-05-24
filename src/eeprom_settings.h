#ifndef _EEPROM_SETTINGS_H
#define _EEPROM_SETTINGS_H

#include <stdint.h>

static const int kNumServos = 11;

struct EepromSettings {
  uint8_t signature[2];

  struct ServoParam {
    uint8_t zero_offset;
  } servo_param[kNumServos];

  int16_t gyro_correction[3];
};

class EepromSettingsManager {
 public:
  EepromSettingsManager() {}
  void Initialize();
  EepromSettings& settings() { return settings_; }
  void Store();

 private:
  EepromSettings settings_;
};

#endif  // _EEPROM_SETTINGS_H