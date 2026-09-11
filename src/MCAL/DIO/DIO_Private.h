#ifndef DIO_PRIVATE_H
#define DIO_PRIVATE_H 


/*======================================================= */

//PORTB ATmega328P
/*======================================================= */
#define PORTB_ADDRESS 0X25
#define DDRB_ADDRESS 0X24
#define PINB_ADDRESS 0X23
#define DDRB (*(volatile u8 *)DDRB_ADDRESS)
#define PORTB (*(volatile u8 *)PORTB_ADDRESS)
#define PINB (*(volatile u8 *)PINB_ADDRESS)
/*================================================================*/
// PORTC ATmega328P
/*================================================================*/
#define PORTC_ADDRESS 0X28
#define DDRC_ADDRESS 0X27
#define PINC_ADDRESS 0X26
#define DDRC (*(volatile u8 *)DDRC_ADDRESS)
#define PORTC (*(volatile u8 *)PORTC_ADDRESS)
#define PINC (*(volatile u8 *)PINC_ADDRESS)
/*================================================================*/
// PORTD ATmega328P
/*================================================================*/

#define PORTD_ADDRESS 0X2B
#define DDRD_ADDRESS 0X2A
#define PIND_ADDRESS 0X29
#define DDRD (*(volatile u8 *)DDRD_ADDRESS)
#define PORTD (*(volatile u8 *)PORTD_ADDRESS)
#define PIND (*(volatile u8 *)PIND_ADDRESS)
//================================================================//

#define PINB_IO_ADDR    0x03
#define PINC_IO_ADDR    0x06
#define PIND_IO_ADDR    0x09

#endif 