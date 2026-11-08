#ifndef PHASE_SEARCH_H
#define PHASE_SEARCH_H
#include "../pll_control_config.h"

typedef unsigned char (*phase_measure_fn)(unsigned char code, unsigned short *current, unsigned short *frequency);
#define PHASE_MEASURE_FAILED 0
#define PHASE_MEASURE_OK 1
#define PHASE_MEASURE_RETRY 2
/* 불안정한 측정은 재시도한다. 콜백은 매번 보호/통신/중단을 처리해야 한다.
   성공 결과는 확인된 최소점보다 저항이 낮은 쪽(+1코드)이다. */
unsigned char phase_find_minimum(unsigned char start, unsigned char *result,
   unsigned short *frequency, phase_measure_fn measure);
/* 부하 드리프트를 A-B-A 비교로 검사하는 제한된 한 단계 보정. EEPROM 저장 없음. */
unsigned char phase_track_once(unsigned char start, unsigned char low, unsigned char high,
   signed char direction, unsigned char *result, phase_measure_fn measure);
#endif
