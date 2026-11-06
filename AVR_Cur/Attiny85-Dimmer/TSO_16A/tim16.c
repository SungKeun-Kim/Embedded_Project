
#include <iom16.h>
#include "tim16.h"

static char timer1_mode;
static char timer1_clock;

void set_timer1_mode(char mode)
{
   timer1_mode = mode;
   TCCR1B = (TCCR1B & ~T1_WGM32_MASK) | (mode & T1_WGM32_MASK);
   TCCR1A = (TCCR1A & ~T1_WGM10_MASK) | (mode & T1_WGM10_MASK);
}

void start_timer1(void)
{ TCCR1B = (TCCR1B & ~T1_CLOCK_MASK) | (timer1_clock & T1_CLOCK_MASK); }

// setting timer1 clock source to T1_NOCLOCK stops it
void stop_timer1(void)
{ TCCR1B = (TCCR1B & ~T1_CLOCK_MASK) | (T1_NOCLOCK); }

void select_timer1_clock(char source)
{ timer1_clock = source; }

void select_timer1_output(char output)
{ TCCR1A = (TCCR1A & ~T1_COMP_MASK) | (output & T1_COMP_MASK); }

void force_timer1_compare(char force)
{ TCCR1A = (TCCR1A & ~T1_FOC_MASK) | (force & T1_FOC_MASK); }

//----------------------------------------------------------

void enable_timer_intr(char mask)   { TIMSK = TIMSK | mask; }
void disable_timer_intr(char mask)  { TIMSK = TIMSK & ~mask; }

void enable_timer_oc1a_intr(void)   { TIMSK = TIMSK | OCIE1A_MASK; }
void enable_timer_oc1b_intr(void)   { TIMSK = TIMSK | OCIE1B_MASK; }
void disable_timer_oc1a_intr(void)  { TIMSK = TIMSK & ~OCIE1A_MASK; }
void disable_timer_oc1b_intr(void)  { TIMSK = TIMSK & ~OCIE1B_MASK; }
void enable_timer_ic1_intr(void)    { TIMSK = TIMSK | ICIE1_MASK; }
void disable_timer_ic1_intr(void)   { TIMSK = TIMSK & ~ICIE1_MASK; }
void enable_timer_tov1_intr(void)   { TIMSK = TIMSK | TOIE1_MASK; }
void disable_timer_tov1_intr(void)  { TIMSK = TIMSK & ~TOIE1_MASK; }

void enable_timer_oc0_intr(void)    { TIMSK = TIMSK | OCIE0_MASK; }
void disable_timer_oc0_intr(void)   { TIMSK = TIMSK & ~OCIE0_MASK; }
void enable_timer_tov0_intr(void)   { TIMSK = TIMSK | TOIE0_MASK; }
void disable_timer_tov0_intr(void)  { TIMSK = TIMSK & ~TOIE0_MASK; }

void enable_timer_oc2_intr(void)    { TIMSK = TIMSK | OCIE2_MASK; }
void disable_timer_oc2_intr(void)   { TIMSK = TIMSK & ~OCIE2_MASK; }
void enable_timer_tov2_intr(void)   { TIMSK = TIMSK | TOIE2_MASK; }
void disable_timer_tov2_intr(void)  { TIMSK = TIMSK & ~TOIE2_MASK; }
