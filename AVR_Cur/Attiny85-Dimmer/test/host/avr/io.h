#pragma once
#include <stdint.h>
// 호스트 테스트용 레지스터 대역. 하드웨어 전기적 특성/명령 실행시간은 모사하지 않는다.
inline uint8_t PORTB, DDRB, PINB, DIDR0, TIMSK, TCCR0B, TCCR1, GTCCR;
inline uint8_t PLLCSR, TCNT1, OCR1C, OCR1A, TIFR;
enum { ADC3D=3, PCKE=2, OCF1A=6, TOV1=2, CTC1=7, CS12=2, OCIE1A=6 };
