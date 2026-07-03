/**
 * @file  buck_dac.h
 * @brief LM5005 buck converter control via DAC1_OUT1(PA4)
 */
#ifndef BUCK_DAC_H
#define BUCK_DAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void BuckDAC_Init(void);
void BuckDAC_SetCode(uint16_t code);
void BuckDAC_SetDuty(uint16_t duty_01pct);
uint16_t BuckDAC_DutyForVoltage01V(uint16_t voltage_01v);
uint16_t BuckDAC_CodeForDuty(uint16_t duty_01pct);
uint16_t BuckDAC_GetCode(void);

#ifdef __cplusplus
}
#endif

#endif /* BUCK_DAC_H */
