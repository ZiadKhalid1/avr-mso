#ifndef ADC_INTERFACE_H_
#define ADC_INTERFACE_H_

#include "../../LIB/STD_TYPES.h"

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7
} ADC_Channel_t;

typedef enum {
    ADC_TRIG_FREE_RUNNING   = 0,  
    ADC_TRIG_ANALOG_COMP    = 1,  
    ADC_TRIG_EXTI0          = 2,  
    ADC_TRIG_TIMER0_COMPA   = 3, 
    ADC_TRIG_TIMER0_OVF     = 4,  
    ADC_TRIG_TIMER1_COMPB   = 5,  
    ADC_TRIG_TIMER1_OVF     = 6,  
    ADC_TRIG_TIMER1_CAPTURE = 7   
} ADC_TriggerSource_t;

typedef enum {
    ADC_RES_8BIT = 0,  
    ADC_RES_10BIT     
} ADC_Resolution_t;

void     ADC_Init(ADC_Resolution_t resolution);
void     ADC_StartAutoTrigger(ADC_Channel_t channel, ADC_TriggerSource_t trigger_src);
void     ADC_StartSingleConversion(ADC_Channel_t channel);

u8       ADC_ReadSample8Bit(void);

u16      ADC_ReadSample10Bit(void);

void     ADC_EnableInterrupt(void (*callBackPtr)(void));
void     ADC_DisableInterrupt(void);

void     ADC_Stop(void);

#endif /* ADC_INTERFACE_H_ */