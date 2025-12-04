#include <Arduino.h>

// ========================================
// AC 조광기 (AC Dimmer) 제어 코드
// Timer1 레지스터 직접 제어 버전
// ========================================
//
// 하드웨어 요구사항:
//
// 1. 제로크로스 검출부 (권장 구성):
//    - AC 220V → 9V 다운 트랜스 (안전 및 정확도 향상)
//    - H11AA1 제로크로스 옵토커플러 (정확한 제로크로스 타이밍)
//    - 출력 → Arduino 디지털 핀 2 (INT0)
//
// 2. 위상 제어부:
//    - Arduino 디지털 핀 3 → MOC3021 옵토커플러
//    - MOC3021 → 트라이악 (BT136, BTA16, BTA41 등)
//    - 트라이악 → AC 부하 (백열등, 할로겐 램프 등)
//
// 3. 조광 입력:
//    - 가변저항 10kΩ (0-5V) → Arduino 아날로그 핀 A0
//
// 동작 원리:
//    - 60Hz AC → 8.33ms 반주기마다 제로크로스 검출
//    - 100μs 타이머로 위상 지연 카운트 (0~100 단계)
//    - dimValue에 따라 트라이악 트리거 타이밍 조절
//    - dimValue 0 = 최대 밝기, 100 = 최소 밝기
//
// Timer1 설정 (ATmega328):
//    - CTC 모드 (Clear Timer on Compare Match)
//    - 프리스케일러: 8
//    - OCR1A: 199 (100μs @ 16MHz)
//    - 16MHz / 8 / 200 = 10,000Hz (100μs 주기)
// ========================================

// 함수 프로토타입 선언
void zeroCrossDetect();

// 전역 변수                                   
volatile int counter = 0;          // 타이머 카운터 변수
volatile boolean zeroCrossDetected = false;  // 제로크로스 감지 플래그
int triacPin = 3;                  // 트라이악 옵토커플러 MOC 3021 연결 핀
int dimValue;                      // 조광 값, 0 = 최대밝기 ; 100 = 최소밝기
int potValue;                      // 가변저항 읽기 값

void setup() { 
  //Serial.begin(9600);  
  pinMode(triacPin, OUTPUT);                           // 트라이악 핀을 출력으로 설정
  attachInterrupt(0, zeroCrossDetect, RISING);         // 핀 2에서 제로크로스 감지 시 인터럽트 실행
  
  // Timer1 레지스터 직접 설정 (100μs 주기)
  cli();                                               // 전역 인터럽트 비활성화
  TCCR1A = 0;                                          // Timer1 제어 레지스터 A 초기화
  TCCR1B = 0;                                          // Timer1 제어 레지스터 B 초기화
  TCNT1  = 0;                                          // Timer1 카운터 초기화
  
  OCR1A = 199;                                         // 비교 매치 값 설정 (100μs @ 16MHz, prescaler 8)
                                                       // 16MHz / 8 / 200 = 10,000Hz (100μs)
  TCCR1B |= (1 << WGM12);                             // CTC 모드 (Clear Timer on Compare Match)
  TCCR1B |= (1 << CS11);                              // 프리스케일러 8 설정
  TIMSK1 |= (1 << OCIE1A);                            // Timer1 비교 매치 A 인터럽트 활성화
  
  sei();                                               // 전역 인터럽트 활성화
}                                                            
void zeroCrossDetect() 
{                                                      
  zeroCrossDetected = true;                            // 제로크로스가 감지되면 플래그를 true로 설정
  counter = 0;                                         // 카운터를 0으로 리셋
  digitalWrite(triacPin, LOW);                         // 트라이악 출력을 LOW로 설정
  
}   

// Timer1 비교 매치 A 인터럽트 서비스 루틴 (100μs마다 호출)
ISR(TIMER1_COMPA_vect) 
{                   
  if (zeroCrossDetected == true) {
    if (dimValue == 0 || counter >= dimValue) {
      digitalWrite(triacPin, HIGH);                    // 설정된 지연 후 트라이악 트리거 (dimValue=0이면 즉시)
      counter = 0;                                     // 카운터 리셋
      zeroCrossDetected = false;                       // 제로크로스 플래그 리셋
    }
    else {
      counter++;                                       // 카운터 증가
    }
  }
}

void loop()
{
  potValue = analogRead(A0);                           // 가변저항(A0 핀)에서 아날로그 값 읽기
  dimValue = map(potValue, 0, 1023, 0, 100);           // 0-1023 범위를 0-100 조광 값으로 매핑
}