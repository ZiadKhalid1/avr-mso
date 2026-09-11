#ifndef		ADC_PRIVATE_H
#define		ADC_PRIVATE_H

#include "../../LIB/STD_TYPES.h"

/*========== Registers (ATmega328P) ==========*/
#define     ADMUX_Reg               *((volatile u8  *)(0x7C))
#define     ADCSRA_Reg              *((volatile u8  *)(0x7A))
#define     ADCSRB_Reg              *((volatile u8  *)(0x7B))
#define     ADCH_Reg                *((volatile u8  *)(0x79))
#define     ADCL_Reg                *((volatile u8  *)(0x78))
#define     ADC_Reg                 *((volatile u16 *)(0x78))

/*========== ADMUX Bits ==========*/
#define     REFS1_Bit                  7
#define     REFS0_Bit                  6
#define     ADLAR_Bit                  5
/* Bit 4 is reserved on ATmega328P and must never be written */
#define     MUX3_Bit                   3
#define     MUX2_Bit                   2
#define     MUX1_Bit                   1
#define     MUX0_Bit                   0

/*========== ADCSRA Bits ==========*/
#define     ADEN_Bit                   7
#define     ADSC_Bit                   6
#define     ADATE_Bit                  5
#define     ADIF_Bit                   4
#define     ADIE_Bit                   3
#define     ADPS2_Bit                  2
#define     ADPS1_Bit                  1
#define     ADPS0_Bit                  0

/*========== ADCSRB Bits ==========*/
#define     ADTS2_Bit                  2
#define     ADTS1_Bit                  1
#define     ADTS0_Bit                  0

/*========== Field Masks ==========*/
/* Keep REFS1|REFS0|ADLAR (bits 7:5). Bit 4 is reserved and is cleared to 0. */
#define     ADC_ADMUX_CFG_MASK         0xE0
/* Keep ACME (bit 6) and reserved bits as-is, clear ADTS2:ADTS0 (bits 2:0) */
#define     ADC_ADCSRB_CFG_MASK        0xF8
#define     ADC_CHANNEL_MASK           0x07
#define     ADC_TRIGGER_MASK           0x07

/*========== Bounds for Input Guards ==========*/
#define     ADC_MIN_CHANNEL            ADC_CHANNEL_0
#define     ADC_MAX_CHANNEL            ADC_CHANNEL_7
#define     ADC_MIN_TRIGGER            ADC_TRIG_FREE_RUNNING
#define     ADC_MAX_TRIGGER            ADC_TRIG_TIMER1_CAPTURE

#endif