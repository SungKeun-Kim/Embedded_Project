#ifndef TUNING_KEY_H
#define TUNING_KEY_H
/* 100.8 ms 틱. 첫 에지의 위상 오차를 포함해 최소 약 3초를 확보한다. */
#define TUNING_HOLD_TICKS 31
void tuning_key_tick(unsigned char pressed, unsigned char allowed);
unsigned char tuning_key_take(void);
#endif
