/**
 * @file  megasonic_pwm.c
 * @brief HRTIM Timer A 메가소닉 PWM 하드웨어 제어 (200kHz~3MHz)
 *
 * STM32G474MET6 HRTIM을 사용한 고해상도 상보 PWM 생성.
 * PA8(CHA1) + PA9(CHA2) → 하프브리지 GaN FET 구동.
 * 유효 클럭 5.44 GHz (184 ps 분해능), DLL 캘리브레이션 필수.
 */
#include "megasonic_pwm.h"
#include "config.h"
#include "params.h"

HRTIM_HandleTypeDef hhrtim1;

/* 현재 Period 값 (주파수 설정 시 캐시) */
static uint32_t s_current_period;

void MegasonicPWM_Init(void)
{
    HRTIM_TimeBaseCfgTypeDef timebase_cfg = {0};
    HRTIM_TimerCfgTypeDef    timer_cfg    = {0};
    HRTIM_OutputCfgTypeDef   output_cfg   = {0};
    HRTIM_DeadTimeCfgTypeDef dt_cfg       = {0};
    HRTIM_CompareCfgTypeDef  compare_cfg  = {0};

    /* HRTIM 클럭 활성화 */
    __HAL_RCC_HRTIM1_CLK_ENABLE();

    /* HRTIM 기본 초기화 */
    hhrtim1.Instance = HRTIM1;
    hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
    hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
    HAL_HRTIM_Init(&hhrtim1);

    /* DLL 캘리브레이션 — HRTIM 고해상도(×32) 활성화 필수 */
    HAL_HRTIM_DLLCalibrationStart(&hhrtim1, HRTIM_CALIBRATIONRATE_3);
    HAL_HRTIM_PollForDLLCalibration(&hhrtim1, 10);

    /* ---- Timer A 타임베이스 설정 ---- */
    /* 기본 주파수: 285 kHz → Period = 5,440,000,000 / 285,000 ≈ 19088 */
    s_current_period = (uint32_t)((uint64_t)HRTIM_EFF_CLOCK_HZ / 285000U);

    timebase_cfg.Period          = s_current_period;
    timebase_cfg.RepetitionCounter = 0;
    timebase_cfg.PrescalerRatio  = HRTIM_PRESCALERRATIO_MUL32; /* ×32 DLL 모드 */
    timebase_cfg.Mode            = HRTIM_MODE_CONTINUOUS;
    HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &timebase_cfg);

    /* ---- Timer A 설정 ---- */
    timer_cfg.DMARequests    = HRTIM_TIM_DMA_NONE;
    timer_cfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
    timer_cfg.StartOnSync    = HRTIM_SYNCSTART_DISABLED;
    timer_cfg.ResetOnSync    = HRTIM_SYNCRESET_DISABLED;
    timer_cfg.DACSynchro     = HRTIM_DACSYNC_NONE;
    timer_cfg.PreloadEnable  = HRTIM_PRELOAD_ENABLED;
    timer_cfg.UpdateGating   = HRTIM_UPDATEGATING_INDEPENDENT;
    timer_cfg.BurstMode      = HRTIM_UPDATEONBURSTDMA_INDEPENDENT;
    timer_cfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_DISABLED;
    timer_cfg.PushPull       = HRTIM_TIMPUSHPULLMODE_DISABLED;
    timer_cfg.FaultEnable    = HRTIM_TIMFAULTENABLE_NONE;
    timer_cfg.FaultLock      = HRTIM_TIMFAULTLOCK_READWRITE;
    timer_cfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
    timer_cfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
    timer_cfg.UpdateTrigger  = HRTIM_TIMUPDATETRIGGER_NONE;
    timer_cfg.ResetTrigger   = HRTIM_TIMRESETTRIGGER_NONE;
    timer_cfg.ResetUpdate    = HRTIM_TIMUPDATEONRESET_DISABLED;
    HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &timer_cfg);

    /* ---- Compare 1 (듀티비) — 초기값 0% ---- */
    compare_cfg.CompareValue = 1;  /* 최소값 */
    HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                     HRTIM_COMPAREUNIT_1, &compare_cfg);

    /* ---- 데드타임 설정 (184 ps 분해능) ---- */
    /* DTR = 데드타임(ns) / 0.184(ns/tick) */
    uint32_t dt_ticks = (uint32_t)((uint64_t)HRTIM_DEADTIME_NS * HRTIM_EFF_CLOCK_HZ / 1000000000ULL);
    if (dt_ticks > 511) dt_ticks = 511;  /* 9비트 최대 */

    dt_cfg.Prescaler        = HRTIM_TIMDEADTIME_PRESCALERRATIO_MUL8;
    dt_cfg.RisingValue      = dt_ticks;
    dt_cfg.RisingSign       = HRTIM_TIMDEADTIME_RISINGSIGN_POSITIVE;
    dt_cfg.RisingLock       = HRTIM_TIMDEADTIME_RISINGLOCK_READONLY;
    dt_cfg.RisingSignLock   = HRTIM_TIMDEADTIME_RISINGSIGNLOCK_READONLY;
    dt_cfg.FallingValue     = dt_ticks;
    dt_cfg.FallingSign      = HRTIM_TIMDEADTIME_FALLINGSIGN_POSITIVE;
    dt_cfg.FallingLock      = HRTIM_TIMDEADTIME_FALLINGLOCK_READONLY;
    dt_cfg.FallingSignLock  = HRTIM_TIMDEADTIME_FALLINGSIGNLOCK_READONLY;
    HAL_HRTIM_DeadTimeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &dt_cfg);

    /* ---- 출력 설정: CHA1 (PA8, 하이사이드) ---- */
    output_cfg.Polarity              = HRTIM_OUTPUTPOLARITY_HIGH;
    output_cfg.SetSource             = HRTIM_OUTPUTSET_TIMPER;     /* Period 이벤트에서 SET */
    output_cfg.ResetSource           = HRTIM_OUTPUTRESET_TIMCMP1;  /* Compare1 이벤트에서 RESET */
    output_cfg.IdleMode              = HRTIM_OUTPUTIDLEMODE_NONE;
    output_cfg.IdleLevel             = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
    output_cfg.FaultLevel            = HRTIM_OUTPUTFAULTLEVEL_INACTIVE;
    output_cfg.ChopperModeEnable     = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
    output_cfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
    HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                    HRTIM_OUTPUT_TA1, &output_cfg);

    /* ---- 출력 설정: CHA2 (PA9, 로우사이드) — 데드타임 자동 삽입 ---- */
    output_cfg.SetSource   = HRTIM_OUTPUTSET_NONE;
    output_cfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
    HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                                    HRTIM_OUTPUT_TA2, &output_cfg);

    /* 기본 주파수 적용 */
    MegasonicPWM_SetFrequency(FREQ_DEFAULT);
}

void MegasonicPWM_SetFrequency(uint16_t freq_01khz)
{
    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;

    /*
     * Period = HRTIM_EFF_CLK / freq_hz
     * freq_hz = freq_01khz × 100
     * HRTIM_EFF_CLK = 5,440,000,000 Hz
     */
    uint32_t freq_hz = (uint32_t)freq_01khz * 100U;
    uint32_t period = (uint32_t)((uint64_t)HRTIM_EFF_CLOCK_HZ / freq_hz);

    /* HRTIM Period 유효 범위: 0x0003 ~ 0xFFFD */
    if (period < 3U) period = 3U;
    if (period > 0xFFFDU) period = 0xFFFDU;

    s_current_period = period;

    /* HRTIM Timer A Period 레지스터 직접 업데이트 (HAL 오버헤드 회피) */
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR = period;
}

void MegasonicPWM_SetDuty(uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) duty_01pct = DUTY_CLAMP_MAX;

    /* Compare = Period × (duty / 1000) */
    uint32_t cmp = (s_current_period * (uint32_t)duty_01pct) / 1000U;
    if (cmp < 1U) cmp = 1U;
    if (cmp >= s_current_period) cmp = s_current_period - 1U;

    /* HRTIM Timer A Compare 1 레지스터 직접 업데이트 */
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = cmp;
}

void MegasonicPWM_Start(void)
{
    HAL_HRTIM_WaveformOutputStart(&hhrtim1,
        HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
    HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_A);
}

void MegasonicPWM_Stop(void)
{
    HAL_HRTIM_WaveformCountStop(&hhrtim1, HRTIM_TIMERID_TIMER_A);
    HAL_HRTIM_WaveformOutputStop(&hhrtim1,
        HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
}

uint32_t MegasonicPWM_GetPeriod(void)
{
    return s_current_period;
}
