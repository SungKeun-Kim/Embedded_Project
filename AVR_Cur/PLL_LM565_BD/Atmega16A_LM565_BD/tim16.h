#ifndef _TIM16_H_
#define _TIM16_H_

//----------------------------------------
// Timer/Counter0 interrupt mask equates

#define OCIE0_MASK     0x02      // timer/counter0 output compare interrutp mask
#define TOIE0_MASK     0x01      // timer/counter0 timer overflow interrutp mask

// Timer/Counter0 Clock source equates

#define T0_NOCLOCK         0x00
#define T0_PRESCALE_1      0x01
#define T0_PRESCALE_8      0x02
#define T0_PRESCALE_64     0x03
#define T0_PRESCALE_256    0x04
#define T0_PRESCALE_1024   0x05
#define T0_EXT_FALLING     0x06     // external clock from T0 pin
#define T0_EXT_RISING      0x07

#define T0_CLOCK_MASK      0x07

// Timer/Counter0 operating mode equates

#define T0_MODE_NORMAL     0x00  // Mode 0: Normal counter
#define T0_MODE_PWM_PC     0x40  // Mode 1: Phase-Centered PWM
#define T0_MODE_CTC        0x08  // Mode 2: Clear timer on Compare Match
#define T0_MODE_FPWM       0x48  // Mode 3: Fast PWM

#define T0_WGM_MASK        0x48

// Timer/Counter0 compare output mode equates

#define T0_COMP_NO_OUT     0x00
#define T0_COMP_TOGGLE     0x10
#define T0_COMP_CLEAR      0x20
#define T0_COMP_SET        0x30

#define T0_COMP_MASK       0x30

//----------------------------------------
// Timer/Counter1 interrupt mask equates

#define ICIE1_MASK     0x20     // timer/counter1 input capture interrutp mask
#define OCIE1A_MASK    0x10     // timer/counter1 output compare A interrutp mask
#define OCIE1B_MASK    0x08     // timer/counter1 output compare B interrutp mask
#define TOIE1_MASK     0x04     // timer/counter1 timer overflow interrutp mask

//----------------------------------------
// Timer/Counter1 Force compare equates

#define T1_FOC1A        0x08
#define T1_FOC1B        0x04

#define T1_FOC_MASK     0x0c

//----------------------------------------
// Timer/Counter1 Clock source equates

#define T1_NOCLOCK         0x00
#define T1_PRESCALE_1      0x01
#define T1_PRESCALE_8      0x02
#define T1_PRESCALE_64     0x03
#define T1_PRESCALE_256    0x04
#define T1_PRESCALE_1024   0x05
#define T1_EXT_FALLING     0x06     // external clock from T1 pin
#define T1_EXT_RISING      0x07

#define T1_CLOCK_MASK      0x07

// Timer/Counter1 operating mode equates

#define T1_WGM32_MASK     0x18 // WGM1[3:2] in TCCR1B
#define T1_WGM10_MASK     0x03 // WGM1[1:0] in TCCR1A

                      //      WGM                   Update of TOVn Flag
                      //    32   10                TOP      OCRnx     Set on
#define T1_MODE_NORMAL    (0x00+0x00)  // Mode 0:  0xffff   immediate MAX
#define T1_MODE_PWM_8     (0x00+0x01)  // Mode 1:  0x00ff   TOP       BOTTOM
#define T1_MODE_PWM_9     (0x00+0x02)  // Mode 2:  0x01ff   TOP       BOTTOM
#define T1_MODE_PWM_10    (0x00+0x03)  // Mode 3:  0x03ff   TOP       BOTTOM
#define T1_MODE_CTC_2     (0x08+0x00)  // Mode 4:  OCR1A    immediate MAX
#define T1_MODE_FPWM_8    (0x08+0x01)  // Mode 5:  0x00ff   BOTTOM    TOP
#define T1_MODE_FPWM_9    (0x08+0x02)  // Mode 6:  0x01ff   BOTTOM    TOP
#define T1_MODE_FPWM_10   (0x08+0x03)  // Mode 7:  0x03ff   BOTTOM    TOP
#define T1_MODE_PWM_PFC_1 (0x10+0x00)  // Mode 8:  ICR1     BOTTOM    BOTTOM
#define T1_MODE_PWM_PFC_2 (0x10+0x02)  // Mode 9:  OCR1A    BOTTOM    BOTTOM
#define T1_MODE_PWM_PC_1  (0x10+0x03)  // Mode 10: ICR1     TOP       BOTTOM
#define T1_MODE_PWM_PC_2  (0x10+0x04)  // Mode 11: OCR1A    TOP       BOTTOM
#define T1_MODE_CTC_1     (0x18+0x00)  // Mode 12: ICR1     immediate MAX
                                       // Mode 13: reserved
#define T1_MODE_FPWM_1    (0x18+0x02)  // Mode 14: ICR1     BOTTOM    TOP
#define T1_MODE_FPWM_2    (0x18+0x03)  // Mode 15: OCR1A    BOTTOM    TOP

// Legend:
//    BOTTOM:  0x0000
//    MAX:     0xffff
//    TOP:     TOP value defined at TOP column above

// Timer/Counter1 compare output mode equates

#define T1_COMPA_NO_OUT     0x00
#define T1_COMPA_TOGGLE     0x40
#define T1_COMPA_CLEAR      0x80
#define T1_COMPA_SET        0xc0

#define T1_COMPB_NO_OUT     0x00
#define T1_COMPB_TOGGLE     0x10
#define T1_COMPB_CLEAR      0x20
#define T1_COMPB_SET        0x30

#define T1_COMPA_MASK       0xc0
#define T1_COMPB_MASK       0x30
#define T1_COMP_MASK        (T1_COMPA_MASK+T1_COMPB_MASK)

void set_timer1_mode(char mode);

// this function does not update timer hardware with the selected
// timer clock source definition. actual hardware update is performed
// by start_timer1() function.
void select_timer1_clock(char source);

void select_timer1_output(char output);

// start timer1 with clock source selected by
// select_timer1_clock() function
void start_timer1(void);

// setting timer1 clock source to T1_NOCLOCK stops it
void stop_timer1(void);

void force_timer1_compare(char force);

//----------------------------------#
//   TIMER/COUNTER2                 #
//==================================#

// Timer/Counter0 interrupt mask equates

#define OCIE2_MASK     0x80      // timer/counter2 output compare interrutp mask
#define TOIE2_MASK     0x40      // timer/counter2 timer overflow interrutp mask

// Timer/Counter2 Control Register

// Force Ouput Compare/Write Only
// Writing one to Bit7 of TCCR2 forces the waveform generation unit
// immediately to work as if an output compare match occurs

#define T2_FOC_MASK        0x80

// Timer/Counter2 Clock source equates

#define T2_NOCLOCK         0x00
#define T2_PRESCALE_1      0x01
#define T2_PRESCALE_8      0x02
#define T2_PRESCALE_64     0x03
#define T2_PRESCALE_256    0x04
#define T2_PRESCALE_1024   0x05
#define T2_EXT_FALLING     0x06   // external clock from T2 pin
#define T2_EXT_RISING      0x07

#define T2_CLOCK_MASK      0x07

// Timer/Counter2 operating mode equates

#define T2_MODE_NORMAL     0x00  // Mode 0: Normal counter
#define T2_MODE_PWM_PC     0x40  // Mode 1: Phase-Centered PWM
#define T2_MODE_CTC        0x08  // Mode 2: Clear timer on Compare Match
#define T2_MODE_FPWM       0x48  // Mode 3: Fast PWM

#define T2_WGM_MASK        0x48

// Timer/Counter2 compare output mode equates

#define T2_COMP_NO_OUT     0x00
#define T2_COMP_TOGGLE     0x10
#define T2_COMP_CLEAR      0x20
#define T2_COMP_SET        0x30

#define T2_COMP_MASK       0x30






// use above interrupt mask bit equates for interrupt enable/disable
void enable_timer_intr(char mask);
void disable_timer_intr(char mask);

// individual timer/counter0 interrupt enable/disable
void enable_timer_tov0_intr(void);
void disable_timer_tov0_intr(void);
void enable_timer_oc0_intr(void);
void disable_timer_oc0_intr(void);

// individual timer/counter1 interrupt enable/disable
void enable_timer_oc1a_intr(void);
void disable_timer_oc1a_intr(void);
void enable_timer_oc1b_intr(void);
void disable_timer_oc1b_intr(void);
void enable_timer_ic1_intr(void);
void disable_timer_ic1_intr(void);
void enable_timer_tov1_intr(void);
void disable_timer_tov1_intr(void);

// individual timer/counter2 interrupt enable/disable
void enable_timer_tov2_intr(void);
void disable_timer_tov2_intr(void);
void enable_timer_oc2_intr(void);
void disable_timer_oc2_intr(void);

extern void funRemote_on();
extern void funRemote_off();
//extern void funTuning();
extern void funRemoteKey_prog();

extern unsigned char auto_tun_flag,ucRunOnes,uctuning_cut;
extern volatile unsigned char tick;



#endif
