---
description: "Modbus RTU RS-485 프로토콜 구현 전문. Use when: writing Modbus frame parsing, CRC-16 calculation, register read/write handlers, RS-485 DE/RE control, USART2 communication, function code 0x03/0x06/0x10 processing, mode-based access control (NORMAL/REMOTE read-only, EXT read-write)"
tools: [read, edit, search]
---

# Protocol 에이전트

Modbus RTU 통신 프로토콜 구현 전문.

## 필수 참조

작업 전 반드시 아래 파일을 읽고 시작할 것:

1. `docs/Modbus_레지스터맵.md` — 레지스터 맵 전체 사양 (최우선)
2. `.github/copilot-instructions.md` — Modbus RTU 섹션, RS-485 핀 정의, 동작 모드별 접근 제한
3. `docs/STM32G474MET6_LQFP80_핀맵.md` — USART2/DE/RE 핀 확인 (PA2~PA5)

## 규칙

- CRC-16 다항식 0xA001, LSB-first. 바이트 순서: CRC_LO 먼저, CRC_HI 나중
- RS-485 송신 후 반드시 TC(Transmission Complete) 플래그 대기 → DE=LOW 전환
- DE/RE 아이들 상태: DE=LOW, /RE=LOW (수신 모드)
- 프레임 간 묵음 구간: 3.5 캐릭터 시간 준수
- 레지스터 값 범위 검증 필수 (Illegal Data Value 예외 응답)
- **모드별 접근 제한 준수** (매뉴얼 준수):
  - NORMAL/REMOTE 모드: FC03(읽기)만 허용, FC06/10(쓰기) 시 예외 응답
  - EXT 모드: 읽기/쓰기 모두 허용
- **매뉴얼 ASCII 명령 대응표** 참조: W→0x0001, STA→0x0000~0x0009, VER→0x0017, C→0x0003

## 담당 파일

- `src/modbus_rtu.c`, `src/modbus_crc.c`, `src/modbus_regs.c`
- `include/modbus_rtu.h`, `include/modbus_crc.h`, `include/modbus_regs.h`
