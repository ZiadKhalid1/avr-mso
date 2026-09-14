#ifndef AC_PRIVATE_H_
#define AC_PRIVATE_H_

#include "../../LIB/STD_TYPES.h"

/*========== Registers (ATmega328P) ==========*/
#define     ACSR_Reg                *((volatile u8  *)(0x50))
#define     ADCSRA_Reg              *((volatile u8  *)(0x7A))
#define     ADCSRB_Reg              *((volatile u8  *)(0x7B))
#define     ADMUX_Reg               *((volatile u8  *)(0x7C))
#define     DIDR1_Reg               *((volatile u8  *)(0x7F))

/*========== ACSR Bits ==========*/
#define     ACD_Bit                    7
#define     ACBG_Bit                   6
#define     ACO_Bit                    5
#define     ACI_Bit                    4
#define     ACIE_Bit                   3
#define     ACIC_Bit                   2
#define     ACIS1_Bit                  1
#define     ACIS0_Bit                  0

/*========== ADCSRB Bits ==========*/
#define     ACME_Bit                   6

/*========== ADCSRA Bits ==========*/
#define     ADEN_Bit                   7

/*========== DIDR1 Bits ==========*/
#define     AIN1D_Bit                  1
#define     AIN0D_Bit                  0

#endif /* AC_PRIVATE_H_ */