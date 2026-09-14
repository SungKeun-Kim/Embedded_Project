/**
 * @file board_runtime_config.h
 * @brief Board bring-up overrides that change often during hardware tuning.
 *
 * Keep this file ASCII-only.  The large params.h file contains many legacy
 * comments, so frequently edited hardware values live here to avoid fragile
 * patching around encoded text.
 */
#ifndef BOARD_RUNTIME_CONFIG_H
#define BOARD_RUNTIME_CONFIG_H

/* Manual frequency bring-up mode. */
#define AUTO_TUNE_ENABLED       0U
#define RUN_RETUNE_ENABLED      0U

/* RUN screen manual voltage adjustment. */
#define RUN_MANUAL_POWER_STEP_01W   5U       /* 0.05 W per button step */
#define RUN_BUCK_SLEW_INTERVAL_MS   10U      /* Runtime DAC slew interval */
#define RUN_BUCK_SLEW_STEP_01PCT    2U       /* 0.2% DAC duty per interval */
#define RUN_VOLTAGE_TRACK_ENABLED   1U       /* Keep measured VBUS near the mapped RUN voltage */
#define RUN_OVERCURRENT_CONFIRM_COUNT 1U     /* Trip immediately at current limit */
#define RUN_POWER_MAP_VOLT_MIN_01V  1000U    /* RUN W display low endpoint: 10.00V */
#define RUN_POWER_MAP_VOLT_MAX_01V  3800U    /* RUN W display high endpoint: 38.00V */
#define RUN_MANUAL_VOLTAGE_MAX_01V  3800U    /* RUN UP/DOWN target clamp: 38.00V */
#define RUN_BUCK_LIMIT_NOTICE_MS    900U     /* Momentary LCD notice at voltage ceiling */
#define RUN_START_UNDERVOLT_TOL_01V 150U     /* Allow START if Buck is within 1.50V below target */

/* LM5005 FB network on the current board: R27=100k, R33=4.99k, R35=8.87k. */
#define BUCK_OUTPUT_MIN_01V     255U         /* DAC 3.3V output estimate: 2.55V */
#define BUCK_OUTPUT_MAX_01V     3800U        /* RUN 38.00V command uses the maximum DAC drive */
#define RUN_BUCK_OVERVOLT_01V   4050U        /* Stop at 40.50V */

/* PA7 voltage feedback divider on the current board. */
#define ADC_VOL_DIV_TOP_OHM     91000UL
#define ADC_VOL_DIV_BOTTOM_OHM  4990UL

/* UCC21520 DT resistor is 6.8 kohm, roughly 68 ns internal driver deadtime. */
#define HRTIM_DEADTIME_NS       60U
#define HRTIM_DEADTIME_MIN_NS   40U
#define HRTIM_DEADTIME_MAX_NS   120U

/* MCU input non-overlap by selected frequency channel. */
#define HRTIM_DEADTIME_CH0_NS   90U         /*  400 kHz */
#define HRTIM_DEADTIME_CH1_NS   80U         /*  600 kHz */
#define HRTIM_DEADTIME_CH2_NS   75U         /*  800 kHz */
#define HRTIM_DEADTIME_CH3_NS   70U         /* 1000 kHz */
#define HRTIM_DEADTIME_CH4_NS   65U         /* 1200 kHz */
#define HRTIM_DEADTIME_CH5_NS   60U         /* 1400 kHz */
#define HRTIM_DEADTIME_CH6_NS   55U         /* 1600 kHz */
#define HRTIM_DEADTIME_CH7_NS   50U         /* 1800 kHz */
#define HRTIM_DEADTIME_CH8_NS   50U         /* 2000 kHz */
#define HRTIM_DEADTIME_CH9_NS   45U         /* 2300 kHz */

/* Default gate duty by selected frequency channel. */
#define HRTIM_DUTY_CH0_01PCT    400U        /*  400 kHz: 40.0% */
#define HRTIM_DUTY_CH1_01PCT    400U        /*  600 kHz: 40.0% */
#define HRTIM_DUTY_CH2_01PCT    390U        /*  800 kHz: 39.0% */
#define HRTIM_DUTY_CH3_01PCT    380U        /* 1000 kHz: 38.0% */
#define HRTIM_DUTY_CH4_01PCT    360U        /* 1200 kHz: 36.0% */
#define HRTIM_DUTY_CH5_01PCT    340U        /* 1400 kHz: 34.0% */
#define HRTIM_DUTY_CH6_01PCT    320U        /* 1600 kHz: 32.0% */
#define HRTIM_DUTY_CH7_01PCT    300U        /* 1800 kHz: 30.0% */
#define HRTIM_DUTY_CH8_01PCT    280U        /* 2000 kHz: 28.0% */
#define HRTIM_DUTY_CH9_01PCT    260U        /* 2300 kHz: 26.0% */

#endif /* BOARD_RUNTIME_CONFIG_H */
