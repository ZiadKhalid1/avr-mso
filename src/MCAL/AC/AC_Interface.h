#ifndef AC_INTERFACE_H_
#define AC_INTERFACE_H_

#include "../../LIB/STD_TYPES.h"
#include "../../LIB/BIT_MATH.h"

/* Negative Input Multiplexer Selection (AIN1 pin vs ADC channel multiplexer) */
typedef enum {
    AC_NEG_INPUT_AIN1 = 0,    /* Standard Pin PD7 (AIN1) */
    AC_NEG_INPUT_ADC0,        /* Channel ADC0 (PC0) */
    AC_NEG_INPUT_ADC1,        /* Channel ADC1 (PC1) */
    AC_NEG_INPUT_ADC2,        /* Channel ADC2 (PC2) */
    AC_NEG_INPUT_ADC3,        /* Channel ADC3 (PC3) */
    AC_NEG_INPUT_ADC4,        /* Channel ADC4 (PC4) */
    AC_NEG_INPUT_ADC5,        /* Channel ADC5 (PC5) */
    AC_NEG_INPUT_ADC6,        /* Channel ADC6 (Extra on Nano/Pro Mini) */
    AC_NEG_INPUT_ADC7         /* Channel ADC7 (Extra on Nano/Pro Mini) */
} AC_NegInput_t;

/* Positive Input Selection (AIN0 pin vs Internal 1.1V Bandgap Reference) */
typedef enum {
    AC_POS_INPUT_AIN0 = 0,    /* Standard Pin PD6 (AIN0) */
    AC_POS_INPUT_BANDGAP      /* Internal fixed 1.1V reference voltage */
} AC_PosInput_t;

/* Interrupt / Trigger Edge Modes */
typedef enum {
    AC_INT_TOGGLE = 0,        /* Output toggle triggers interrupt */
    AC_INT_FALLING_EDGE = 2,  /* Output falling edge triggers interrupt */
    AC_INT_RISING_EDGE = 3    /* Output rising edge triggers interrupt */
} AC_InterruptMode_t;

/* Analog Comparator Current Output State */
typedef enum {
    AC_OUTPUT_LOW = 0,
    AC_OUTPUT_HIGH
} AC_OutputState_t;

/* Configuration Structure */
typedef struct {
    AC_PosInput_t       positive_source;
    AC_NegInput_t       negative_source;
    AC_InterruptMode_t  interrupt_mode;
} AC_Config_t;

/* Core Management APIs */
void             AC_Init(const AC_Config_t *config);
void             AC_Enable(void);
void             AC_Disable(void);

/* Polling & State APIs */
AC_OutputState_t AC_GetOutputState(void);

/* Hardware Timer1 Input Capture Coupling (Cycle-Accurate Triggering) */
void             AC_EnableTimer1Capture(void);
void             AC_DisableTimer1Capture(void);

/* Interrupt & Callback System */
void             AC_EnableInterrupt(void);
void             AC_DisableInterrupt(void);
void             AC_SetCallback(void (*callback)(void));

#endif /* AC_INTERFACE_H_ */
