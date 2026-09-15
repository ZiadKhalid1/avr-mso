#ifndef TIMER1_PRIVATE_H_
#define TIMER1_PRIVATE_H_

#include "../../LIB/STD_TYPES.h"
// Control Registers
#define TCCR1A_Reg      (*(volatile u8*)0x80)
#define TCCR1B_Reg      (*(volatile u8*)0x81)

// 16-bit Registers (Split for atomic access)
#define TCNT1L_Reg      (*(volatile u8*)0x84)
#define TCNT1H_Reg      (*(volatile u8*)0x85)

#define ICR1L_Reg       (*(volatile u8*)0x86)
#define ICR1H_Reg       (*(volatile u8*)0x87)

#define OCR1AL_Reg      (*(volatile u8*)0x88)
#define OCR1AH_Reg      (*(volatile u8*)0x89)

#define OCR1BL_Reg      (*(volatile u8*)0x8A)
#define OCR1BH_Reg      (*(volatile u8*)0x8B)

// Interrupt and Status Registers
#define TIMSK1_Reg      (*(volatile u8*)0x6F)
#define PRR_Reg         (*(volatile u8*)0x64)
#define SREG_Reg        (*(volatile u8*)0x5F)

// Global Interrupt Enable Bit
#define I_BIT           7

// TCCR1B Bits
#define CS10_Bit        0
#define CS11_Bit        1
#define CS12_Bit        2
#define WGM12_Bit       3
#define WGM13_Bit       4
#define ICES1_Bit       6
#define ICNC1_Bit       7

// TIMSK1 Bits
#define OCIE1A_Bit      1
#define OCIE1B_Bit      2
#define ICIE1_Bit       5

// PRR Bit
#define PRTIM1_Bit      3

#endif
