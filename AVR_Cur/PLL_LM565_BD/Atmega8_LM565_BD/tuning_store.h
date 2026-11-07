#ifndef TUNING_STORE_H
#define TUNING_STORE_H
#include "../pll_control_config.h"

/* 업로드 알림 영역(0x1F8~0x1FF)과 겹치지 않는 두 개의 저장 슬롯. */
#define TUNING_SLOT0 0x100U
#define TUNING_SLOT1 0x108U
#define TUNING_RECORD_SIZE 8U

/* 반환값: 2=검증된 새 형식, 1=구형 설정, 0=유효한 설정 없음. */
unsigned char tuning_store_load(unsigned char *code, unsigned short *count);
unsigned char tuning_store_save(unsigned char code, unsigned short count);
#endif
