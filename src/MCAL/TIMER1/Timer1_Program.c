#include "../../LIB/STD_TYPES.h"
#include "../../LIB/BIT_MATH.h"
#include "Timer1_Interface.h"
#include "Timer1_Private.h"

static void (*Timer1_CompareA_Callback)(void) = Null;
static void (*Timer1_CompareB_Callback)(void) = Null;
static void (*Timer1_ICU_Callback)(u16 captured_ticks) = Null;

// Helper Functions for Atomic 16-bit Register Access
static inline void Timer1_Write16Bit(volatile u8 *regH, volatile u8 *regL, u16 value)
{
    u8 sreg_temp = SREG_Reg;
    CLR_BIT(SREG_Reg, I_BIT);

    *regH = (u8)(value >> 8);
    *regL = (u8)(value & 0xFF);

    SREG_Reg = sreg_temp;
}

static inline u16 Timer1_Read16Bit(volatile u8 *regH, volatile u8 *regL)
{
    u8 sreg_temp = SREG_Reg;
    u16 value;
    CLR_BIT(SREG_Reg, I_BIT);

    value = *regL;
    value |= ((u16)(*regH) << 8);

    SREG_Reg = sreg_temp;
    return value;
}

// Prescaler configuration
static void Timer1_ApplyPrescaler(Timer1_Prescaler_t prescaler)
{
    CLR_BIT(TCCR1B_Reg, CS12_Bit);
    CLR_BIT(TCCR1B_Reg, CS11_Bit);
    CLR_BIT(TCCR1B_Reg, CS10_Bit);

    switch (prescaler)
    {
        case TIMER1_PRESCALER_STOP:
            break;
        case TIMER1_PRESCALER_1:
            SET_BIT(TCCR1B_Reg, CS10_Bit);
            break;
        case TIMER1_PRESCALER_8:
            SET_BIT(TCCR1B_Reg, CS11_Bit);
            break;
        case TIMER1_PRESCALER_64:
            SET_BIT(TCCR1B_Reg, CS11_Bit);
            SET_BIT(TCCR1B_Reg, CS10_Bit);
            break;
        case TIMER1_PRESCALER_256:
            SET_BIT(TCCR1B_Reg, CS12_Bit);
            break;
        case TIMER1_PRESCALER_1024:
            SET_BIT(TCCR1B_Reg, CS12_Bit);
            SET_BIT(TCCR1B_Reg, CS10_Bit);
            break;
        default:
            break;
    }
}

void Timer1_Init(Timer1_Mode_t mode, Timer1_Prescaler_t prescaler)
{
    // Enable Timer1 in Power Reduction Register
    CLR_BIT(PRR_Reg, PRTIM1_Bit);

    TCCR1A_Reg = 0x00;
    TCCR1B_Reg = 0x00;
    Timer1_Write16Bit(&TCNT1H_Reg, &TCNT1L_Reg, 0x0000);

    switch (mode)
    {
        case TIMER1_MODE_NORMAL:
            break;
        case TIMER1_MODE_CTC_TOP_OCR1A:
            SET_BIT(TCCR1B_Reg, WGM12_Bit);
            break;
        case TIMER1_MODE_CTC_TOP_ICR1:
            SET_BIT(TCCR1B_Reg, WGM13_Bit);
            SET_BIT(TCCR1B_Reg, WGM12_Bit);
            break;
        default:
            break;
    }

    Timer1_ApplyPrescaler(prescaler);
}

void Timer1_Stop(void)
{
    CLR_BIT(TCCR1B_Reg, CS12_Bit);
    CLR_BIT(TCCR1B_Reg, CS11_Bit);
    CLR_BIT(TCCR1B_Reg, CS10_Bit);
}

void Timer1_SetCompareA(u16 value)
{
    Timer1_Write16Bit(&OCR1AH_Reg, &OCR1AL_Reg, value);
}

void Timer1_SetCompareB(u16 value)
{
    Timer1_Write16Bit(&OCR1BH_Reg, &OCR1BL_Reg, value);
}

void Timer1_SetCounter(u16 value)
{
    Timer1_Write16Bit(&TCNT1H_Reg, &TCNT1L_Reg, value);
}

u16 Timer1_GetCounter(void)
{
    return Timer1_Read16Bit(&TCNT1H_Reg, &TCNT1L_Reg);
}

void Timer1_ICU_Init(Timer1_ICU_Edge_t edge)
{
    Timer1_ICU_SetEdge(edge);
    SET_BIT(TCCR1B_Reg, ICNC1_Bit);
}

void Timer1_ICU_SetEdge(Timer1_ICU_Edge_t edge)
{
    if (edge == TIMER1_ICU_RISING_EDGE)
    {
        SET_BIT(TCCR1B_Reg, ICES1_Bit);
    }
    else
    {
        CLR_BIT(TCCR1B_Reg, ICES1_Bit);
    }
}

u16 Timer1_ICU_GetCapturedValue(void)
{
    return Timer1_Read16Bit(&ICR1H_Reg, &ICR1L_Reg);
}

void Timer1_EnableCompareAInt(void)
{
    SET_BIT(TIMSK1_Reg, OCIE1A_Bit);
}

void Timer1_DisableCompareAInt(void)
{
    CLR_BIT(TIMSK1_Reg, OCIE1A_Bit);
}

void Timer1_EnableCompareBInt(void)
{
    SET_BIT(TIMSK1_Reg, OCIE1B_Bit);
}

void Timer1_DisableCompareBInt(void)
{
    CLR_BIT(TIMSK1_Reg, OCIE1B_Bit);
}

void Timer1_EnableICUInt(void)
{
    SET_BIT(TIMSK1_Reg, ICIE1_Bit);
}

void Timer1_DisableICUInt(void)
{
    CLR_BIT(TIMSK1_Reg, ICIE1_Bit);
}

void Timer1_SetCompareACallback(void (*callback)(void))
{
    Timer1_CompareA_Callback = callback;
}

void Timer1_SetCompareBCallback(void (*callback)(void))
{
    Timer1_CompareB_Callback = callback;
}

void Timer1_SetICUCallback(void (*callback)(u16 captured_ticks))
{
    Timer1_ICU_Callback = callback;
}

// Vector 10: TIMER1_CAPT_vect
void __vector_10(void) __attribute__((signal, used));
void __vector_10(void)
{
    if (Timer1_ICU_Callback != Null)
    {
        u16 captured = Timer1_Read16Bit(&ICR1H_Reg, &ICR1L_Reg);
        Timer1_ICU_Callback(captured);
    }
}

// Vector 11: TIMER1_COMPA_vect
void __vector_11(void) __attribute__((signal, used));
void __vector_11(void)
{
    if (Timer1_CompareA_Callback != Null)
    {
        Timer1_CompareA_Callback();
    }
}

// Vector 12: TIMER1_COMPB_vect
void __vector_12(void) __attribute__((signal, used));
void __vector_12(void)
{
    if (Timer1_CompareB_Callback != Null)
    {
        Timer1_CompareB_Callback();
    }
}
