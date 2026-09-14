#ifndef UART_PRIVATE_H_
#define UART_PRIVATE_H_
// Register Definitions for ATmega328 UART Module
#define UCSRnA_Reg              *((volatile u8*)0xC0)
#define UCSRnB_Reg             *((volatile u8*)0xC1)
#define UCSRnC_Reg             *((volatile u8*)0xC2)
#define UDR_Reg                 *((volatile u8*)0xC6)
#define UBRRL_Reg              *((volatile u8*)0xC4)
#define UBRRH_Reg             *((volatile u8*)0xC5)
// Bit Definitions for UCSRnA Register
#define RXC                     7
#define TXC                     6   
#define UDRE                    5
#define FE                      4
#define DOR                     3
#define PE                      2
#define U2X                     1
#define MPCM                    0
// Bit Definitions for UCSRnB Register
#define RXCIE                   7
#define TXCIE                   6
#define UDRIE                   5
#define RXEN                    4
#define TXEN                    3
#define UCSZ2                   2
// Bit Definitions for UCSRnC Register
#define URSEL                   7
#define UMSEL                   6
#define UPM1                    5
#define UPM0                    4
#define USBS                    3
#define UCSZ1                   2
#define UCSZ0                   1

#endif