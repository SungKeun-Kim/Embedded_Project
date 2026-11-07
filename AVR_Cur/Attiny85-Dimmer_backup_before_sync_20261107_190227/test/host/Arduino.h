#pragma once
#include <stdint.h>
extern int simulatedAdc;
inline int analogRead(uint8_t) { return simulatedAdc; }
inline void init() {}
