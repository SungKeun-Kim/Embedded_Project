/**
 * @file  buck_dac.c
 * @brief LM5005 buck converter control via DAC1_OUT1(PA4)
 */
#include "buck_dac.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"

#define BUCK_DAC_MAX_CODE ((uint16_t)(ADC_RESOLUTION - 1U))

static uint16_t s_buck_dac_code;

void BuckDAC_Init(void)
{
    __HAL_RCC_DAC1_CLK_ENABLE();

    BUCK_DAC->CR &= ~DAC_CR_EN1;
    BUCK_DAC->CR &= ~(DAC_CR_TEN1 | DAC_CR_WAVE1);
    BUCK_DAC->MCR &= ~DAC_MCR_MODE1; /* mode 0: external pin, output buffer enabled */
    BUCK_DAC->DHR12R1 = BUCK_DAC_MAX_CODE;
    BUCK_DAC->CR |= DAC_CR_EN1;

    s_buck_dac_code = BUCK_DAC_MAX_CODE;
}

void BuckDAC_SetCode(uint16_t code)
{
    if (code > BUCK_DAC_MAX_CODE) {
        code = BUCK_DAC_MAX_CODE;
    }

    BUCK_DAC->DHR12R1 = code;
    s_buck_dac_code = code;
}

uint16_t BuckDAC_CodeForDuty(uint16_t duty_01pct)
{
    uint32_t code;

    if (duty_01pct > DUTY_CLAMP_MAX) {
        duty_01pct = DUTY_CLAMP_MAX;
    }

    /* LM5005 FB control is inverted on this board.
     * 0% duty drives the DAC high for minimum VBUS, and DUTY_CLAMP_MAX
     * drives the DAC low for maximum VBUS. */
    code = (uint32_t)BUCK_DAC_MAX_CODE
           - (((uint32_t)duty_01pct * (uint32_t)BUCK_DAC_MAX_CODE
               + ((uint32_t)DUTY_CLAMP_MAX / 2U))
              / (uint32_t)DUTY_CLAMP_MAX);
    return (uint16_t)code;
}

uint16_t BuckDAC_DutyForVoltage01V(uint16_t voltage_01v)
{
    uint32_t span_v;
    uint32_t delta_v;

    if (voltage_01v <= BUCK_OUTPUT_MIN_01V) {
        return DUTY_MIN;
    }
    if (voltage_01v >= BUCK_OUTPUT_MAX_01V) {
        return DUTY_CLAMP_MAX;
    }

    span_v = (uint32_t)(BUCK_OUTPUT_MAX_01V - BUCK_OUTPUT_MIN_01V);
    delta_v = (uint32_t)(voltage_01v - BUCK_OUTPUT_MIN_01V);
    return (uint16_t)((delta_v * (uint32_t)DUTY_CLAMP_MAX + (span_v / 2U)) / span_v);
}

void BuckDAC_SetDuty(uint16_t duty_01pct)
{
    BuckDAC_SetCode(BuckDAC_CodeForDuty(duty_01pct));
}

uint16_t BuckDAC_GetCode(void)
{
    return s_buck_dac_code;
}
