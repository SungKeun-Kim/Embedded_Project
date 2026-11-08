#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H
#include "../pll_control_config.h"

/* ACS712-20A 100 mV/A, R100=2k, R101=10k, R103=57.6k,
   AVCC=5V의 명목 환산. D100 동저항/온도 영향은 실측 보정이 필요하다.
   단위 Q4: ADC 1단계=16. 실측 시 이득만 교체한다. */
#define RT_GAIN_Q4_PER_A 265U
#define RT_TRIGGER_Q4 53U          /* 명목 0.2A 상승 */
#define RT_IMPROVE_Q4 16U          /* ADC 1단계: 약 0.060A, 잡음보다 큰 개선 */
#define RT_EDGE_Q4 40U             /* 명목 0.15A 상승/하강 히스테리시스 */
#define RT_IDLE_RANGE_Q4 27U       /* 약 0.1A 이내로 안정 */
#define RT_IDLE_ABOVE_STANDBY_Q4 212U /* 대기 기준보다 약 0.8A 이내 */
#define RT_WINDOW_TICKS 586U       /* 3.00032초 */
#define RT_FAST_TICKS 195U         /* 998.4ms 이하 */
#define RT_SLOW_TICKS 391U         /* 2.00192초 이상 */
#define RT_EDGE_MIN_TICKS 20U      /* 약 102ms보다 빠른 변화는 잡음 후보 */
#define RT_EXPIRE_TICKS 1954U      /* 10초간 상승이 없으면 분류 해제 */
#define RT_COOLDOWN_TICKS 1954U
#define RT_SETTLE_TICKS PLL_SETTLE_TICKS
#define RT_PERMIT_TICKS 100U       /* 약 512ms마다 허가 만료 */
#define RT_REQUIRED_INTERVALS 3U
#define RT_REQUIRED_PEAKS 3U
#define RT_REQUIRED_WINDOWS 2U
#endif
