#ifndef TIMER1_INTERFACE_H_
#define TIMER1_INTERFACE_H_

#include "../../LIB/STD_TYPES.h"

// Clock Prescaler Options
typedef enum {
    TIMER1_PRESCALER_STOP = 0,
    TIMER1_PRESCALER_1,
    TIMER1_PRESCALER_8,
    TIMER1_PRESCALER_64,
    TIMER1_PRESCALER_256,
    TIMER1_PRESCALER_1024
} Timer1_Prescaler_t;

// Supported Timer1 Modes
typedef enum {
    TIMER1_MODE_NORMAL = 0,
    TIMER1_MODE_CTC_TOP_OCR1A,
    TIMER1_MODE_CTC_TOP_ICR1
} Timer1_Mode_t;

// Input Capture Unit Trigger Edge Options
typedef enum {
    TIMER1_ICU_FALLING_EDGE = 0,
    TIMER1_ICU_RISING_EDGE
} Timer1_ICU_Edge_t;

// Core Timer Functions
void Timer1_Init(Timer1_Mode_t mode, Timer1_Prescaler_t prescaler);
void Timer1_Stop(void);

void Timer1_SetCompareA(u16 value);
void Timer1_SetCompareB(u16 value);

void Timer1_SetCounter(u16 value);
u16  Timer1_GetCounter(void);

// Input Capture Unit Functions
void Timer1_ICU_Init(Timer1_ICU_Edge_t edge);
void Timer1_ICU_SetEdge(Timer1_ICU_Edge_t edge);
u16  Timer1_ICU_GetCapturedValue(void);

// Interrupt Control Functions
void Timer1_EnableCompareAInt(void);
void Timer1_DisableCompareAInt(void);
void Timer1_EnableCompareBInt(void);
void Timer1_DisableCompareBInt(void);
void Timer1_EnableICUInt(void);
void Timer1_DisableICUInt(void);

// Callback Functions
void Timer1_SetCompareACallback(void (*callback)(void));
void Timer1_SetCompareBCallback(void (*callback)(void));
void Timer1_SetICUCallback(void (*callback)(u16 captured_ticks));

#endif
