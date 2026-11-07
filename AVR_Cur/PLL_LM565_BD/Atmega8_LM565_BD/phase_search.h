#ifndef PHASE_SEARCH_H
#define PHASE_SEARCH_H
#include "../pll_control_config.h"

typedef unsigned char (*phase_measure_fn)(unsigned char code, unsigned short *current, unsigned short *frequency);
/* 성공한 경우에만 result를 사용한다. 호출자가 실패 시 시작값을 복원한다. */
unsigned char phase_find_minimum(unsigned char start, unsigned char *result,
   unsigned short *frequency, phase_measure_fn measure);
/* 부하 드리프트를 A-B-A 비교로 검사하는 제한된 한 단계 보정. EEPROM 저장 없음. */
unsigned char phase_track_once(unsigned char start, unsigned char low, unsigned char high,
   signed char direction, unsigned char *result, phase_measure_fn measure);
#endif
