#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ========================================
// AC 조광기 (AC Dimmer) 제어 코드
// ATtiny85 전용 - Timer1 레지스터 직접 제어
// ========================================
//
// 하드웨어 요구사항:
//
// 1. MCU: ATtiny85 (DIP-8)
//    - 내부 8MHz 클럭 사용 (외부 크리스탈 불필요)
//    - 퓨즈 설정: LFUSE=0xE2, HFUSE=0xDF, EFUSE=0xFF
//
// 2. 제로크로스 검출부 (권장 구성):
//    - AC 220V → 9V 다운 트랜스 (안전 및 정확도 향상)
//    - H11AA1 제로크로스 옵토커플러 (정확한 제로크로스 타이밍)
//    - 출력 → ATtiny85 PB2 (7번 핀, INT0)
//
// 3. 위상 제어부:
//    - ATtiny85 PB0 (5번 핀) → MOC3021 옵토커플러
//    - MOC3021 → 트라이악 (BT136, BTA16, BTA41 등)
//    - 트라이악 → AC 부하 (백열등, 할로겐 램프 등)
//
// 4. 조광 입력:
//    - 가변저항 10kΩ (0-5V) → ATtiny85 PB3 (2번 핀, ADC3)
//
// 핀 배치 (DIP-8):
//    1: RESET/PB5       - 리셋 (ISP)
//    2: PB3 (ADC3)      - 가변저항 입력
//    3: PB4 (ADC2)      - 예비
//    4: GND             - 그라운드
//    5: PB0             - 트라이악 제어 출력 ✅
//    6: PB1             - 예비
//    7: PB2 (INT0)      - 제로크로스 입력 ✅
//    8: VCC             - 5V 전원
//
// 동작 원리:
//    - 60Hz AC → 8.33ms 반주기마다 제로크로스 검출
//    - 50μs 타이머로 위상 지연 카운트 (0~158 단계)
//    - dimValue에 따라 트라이악 트리거 타이밍 조절
//    - dimValue 0 = 최대 밝기 100% (즉시 트리거), 158 = 최소 밝기 5% (7.9ms 지연)
//    - 트라이악 트리거 펄스 폭: 50μs (안정적인 트리거 보장)
//
// Timer1 설정 (ATtiny85):
//    - CTC 모드 (Clear Timer on Compare Match)
//    - 프리스케일러: 8
//    - OCR1C: 49 (50μs @ 8MHz)
//    - 8MHz / 8 / 50 = 20,000Hz (50μs 주기)
// ========================================

// 전역 변수
volatile int counter = 0;                    // 타이머 카운터 변수
volatile bool zeroCrossDetected = false;     // 제로크로스 감지 플래그
volatile bool triacTriggered = false;        // 트라이악 트리거 상태
volatile int triggerPulseCounter = 0;        // 트리거 펄스 카운터 (50μs 단위)
volatile unsigned long lastZeroCrossTime = 0; // 마지막 제로크로스 시간 (디바운싱용)
const int triacPin = 0;                      // PB0 (5번 핀) - 트라이악 제어
const int potPin = 3;                        // PB3 (2번 핀) - 가변저항 (ADC3)
int potValue;                                // 가변저항 읽기 값
const int MIN_DIM = 0;                       // 최소 딤 값 (최대 밝기 100%)
const int MAX_DIM = 158;                     // 최대 딤 값 (최소 밝기 5%)
volatile int dimValue = MAX_DIM;             // 조광 값, 0 = 최대밝기(100%) ; 158 = 최소밝기(5%) - 꺼진 상태로 시작

void setup() {
  // 트라이악 제어 핀을 출력으로 설정
  pinMode(triacPin, OUTPUT);
  digitalWrite(triacPin, LOW);
  
  // 제로크로스 외부 인터럽트 설정 (PB2, INT0)
  // 상승 엣지에서 인터럽트 발생
  GIMSK |= (1 << INT0);                      // INT0 외부 인터럽트 활성화
  MCUCR |= (1 << ISC01) | (1 << ISC00);      // 상승 엣지 트리거
  
  // Timer1 레지스터 직접 설정 (50μs 주기)
  cli();                                     // 전역 인터럽트 비활성화
  
  TCCR1 = 0;                                 // Timer1 제어 레지스터 초기화
  TCNT1 = 0;                                 // Timer1 카운터 초기화
  
  // CTC 모드 설정 (OCR1C와 비교)
  TCCR1 |= (1 << CTC1);                      // CTC 모드 활성화
  
  // 프리스케일러 8 설정 (CS13:CS12:CS11:CS10 = 0100)
  TCCR1 |= (1 << CS12);                      // 프리스케일러 8
  
  // 비교 매치 값 설정
  OCR1C = 49;                                // TOP 값 (50μs @ 8MHz, prescaler 8)
                                             // 8MHz / 8 / 50 = 20,000Hz (50μs)
  OCR1A = 49;                                // 비교 매치 A 값
  
  // Timer1 비교 매치 A 인터럽트 활성화
  TIMSK |= (1 << OCIE1A);                    // Timer1 비교 매치 A 인터럽트 활성화
  
  sei();                                     // 전역 인터럽트 활성화
}

// 제로크로스 외부 인터럽트 (INT0)
ISR(INT0_vect) {
  unsigned long currentTime = micros();
  
  // 디바운싱: 마지막 제로크로스로부터 2ms 이상 경과했는지 확인
  if (zeroCrossDetected == false && (currentTime - lastZeroCrossTime) > 2000) {
    zeroCrossDetected = true;                // 제로크로스가 감지되면 플래그를 true로 설정
    lastZeroCrossTime = currentTime;         // 현재 시간 저장
    counter = 0;                             // 카운터를 0으로 리셋
    triacTriggered = false;                  // 트라이악 트리거 상태 리셋
    PORTB &= ~(1 << triacPin);               // 트라이악 출력을 LOW로 설정 (직접 레지스터)
  }
}

// Timer1 비교 매치 A 인터럽트 서비스 루틴 (50μs마다 호출)
ISR(TIMER1_COMPA_vect) {
  if (zeroCrossDetected == true) {
    // 트라이악 트리거 펄스 관리 (50μs 폭)
    if (triacTriggered == true) {
      triggerPulseCounter++;
      if (triggerPulseCounter >= 1) {        // 50μs 펄스 폭 (1 × 50μs)
        PORTB &= ~(1 << triacPin);           // 펄스 종료 (직접 레지스터)
        triacTriggered = false;              // 트리거 완료
        zeroCrossDetected = false;           // 다음 제로크로스 대기
        triggerPulseCounter = 0;
      }
    }
    // 위상 지연 카운트 및 트리거
    else if (counter >= dimValue) {
      PORTB |= (1 << triacPin);              // 설정된 지연 후 트라이악 트리거 (직접 레지스터)
      triacTriggered = true;                 // 트리거 시작
      triggerPulseCounter = 0;               // 펄스 카운터 초기화
    }
    else {
      counter++;                             // 카운터 증가
    }
  }
}

void loop() {
  // 가변저항 값 읽기 (ADC3)
  potValue = analogRead(potPin);                          // PB3(ADC3)에서 아날로그 값 읽기
  int tempDimValue = map(potValue, 0, 1023, MIN_DIM, MAX_DIM);  // 임시 변수에 먼저 계산
                                                          // 0 = 최대밝기 100% (즉시 트리거)
                                                          // 158 = 최소밝기 5% (7.9ms 지연)
  
  // 원자성 보장: 2바이트 변수 쓰기 중 인터럽트 방지
  cli();                                                  // 인터럽트 비활성화
  dimValue = tempDimValue;                                // 0-158 조광 값 업데이트
  sei();                                                  // 인터럽트 재활성화
  
  delay(50);                                              // 50ms 지연 (ADC 읽기 부하 감소)
}

// Arduino main 함수 (ATtiny 프레임워크용)
int main(void) {
  init();  // Arduino 타이머/인터럽트 초기화
  setup();
  
  for (;;) {
    loop();
  }
  
  return 0;
}